/**
 * @file ResultExporter.cpp
 * @brief Запись итоговой ведомости средствами библиотеки QXlsx.
 */
#include "ResultExporter.h"
#include "DomainError.h"

#include <QColor>
#include <QDateTime>
#include <QHash>

#include "xlsxdocument.h"
#include "xlsxformat.h"

namespace sedm {

namespace {
const QColor kHoney(0xFC, 0xEF, 0xD2);    // светло-медовый — заголовки номинаций
const QColor kHeader(0xE8, 0xA3, 0x17);   // медовый — шапки таблиц
const QColor kBorder(0xC9, 0xB4, 0x8F);
const QString kSheetResults = QStringLiteral("Итоги");
const QString kSheetDetails = QStringLiteral("Детализация");

/// Набор оформлений ячеек ведомости.
struct Styles {
    QXlsx::Format title, subtitle, nomination, header, text, center, score, average, place, muted;

    Styles()
    {
        title.setFontBold(true);
        title.setFontSize(14);
        subtitle.setFontSize(11);
        muted.setFontItalic(true);
        muted.setFontColor(QColor(0x5E, 0x4B, 0x37));

        nomination.setFontBold(true);
        nomination.setFontSize(12);
        nomination.setPatternBackgroundColor(kHoney);
        nomination.setBorderStyle(QXlsx::Format::BorderThin);
        nomination.setBorderColor(kBorder);

        header.setFontBold(true);
        header.setPatternBackgroundColor(kHeader);
        header.setTextWrap(true);
        header.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        header.setVerticalAlignment(QXlsx::Format::AlignVCenter);
        header.setBorderStyle(QXlsx::Format::BorderThin);
        header.setBorderColor(kBorder);

        for (QXlsx::Format* f : {&text, &center, &score, &average, &place}) {
            f->setBorderStyle(QXlsx::Format::BorderThin);
            f->setBorderColor(kBorder);
            f->setVerticalAlignment(QXlsx::Format::AlignVCenter);
        }
        center.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        score.setNumberFormat(QStringLiteral("0.00"));
        score.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        average.setNumberFormat(QStringLiteral("0.00"));
        average.setFontBold(true);
        average.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
        place.setFontBold(true);
        place.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    }
};

QVariant cellValue(const std::optional<double>& v)
{
    return v ? QVariant(*v) : QVariant(QStringLiteral("—"));
}

void writeResultsSheet(QXlsx::Document& xlsx, const Styles& st, const TastingSession& session,
                       const QVector<NominationResult>& nominations)
{
    xlsx.selectSheet(kSheetResults);
    int maxTasters = 0;
    for (const NominationResult& n : nominations)
        maxTasters = qMax(maxTasters, n.tasters.size());

    xlsx.write(1, 1, QStringLiteral("Итоговая ведомость экспертной дегустации мёда"), st.title);
    xlsx.write(2, 1, session.displayText(), st.subtitle);
    xlsx.write(3, 1, QStringLiteral("Сформирована %1. Средний балл — среднее итоговых баллов дегустаторов, "
                                    "выставивших все четыре оценки; равные баллы — одно место.")
                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"))),
               st.muted);
    xlsx.setColumnWidth(1, 8);    // место
    xlsx.setColumnWidth(2, 9);    // шифр
    xlsx.setColumnWidth(3, 34);   // ФИО
    xlsx.setColumnWidth(4, 24);   // регион
    if (maxTasters > 0)
        xlsx.setColumnWidth(5, 4 + maxTasters, 15);
    xlsx.setColumnWidth(5 + maxTasters, 6 + maxTasters, 12);

    int row = 5;
    for (const NominationResult& n : nominations) {
        const int cols = 4 + n.tasters.size() + 2;
        xlsx.write(row, 1, QStringLiteral("Номинация «%1» — образцов: %2").arg(n.name).arg(n.results.size()),
                   st.nomination);
        xlsx.mergeCells(QXlsx::CellRange(row, 1, row, qMax(cols, 4)), st.nomination);
        ++row;
        QStringList headers = {QStringLiteral("Место"), QStringLiteral("Шифр"), QStringLiteral("ФИО участника"),
                               QStringLiteral("Регион")};
        for (const TasterColumn& t : n.tasters)
            headers << QStringLiteral("%1\n(команда %2)").arg(t.name).arg(t.teamNumber);
        headers << QStringLiteral("Средний балл") << QStringLiteral("Полных оценок");
        for (int c = 0; c < headers.size(); ++c)
            xlsx.write(row, c + 1, headers[c], st.header);
        xlsx.setRowHeight(row, 32);
        ++row;

        QHash<int, int> columnOf;   // дегустатор -> номер столбца
        for (int i = 0; i < n.tasters.size(); ++i)
            columnOf.insert(n.tasters[i].tasterId, 5 + i);
        for (const SampleResult& r : n.results) {
            xlsx.write(row, 1, r.place() > 0 ? QVariant(r.place()) : QVariant(QStringLiteral("—")), st.place);
            xlsx.write(row, 2, r.sample().cipher(), st.center);
            xlsx.write(row, 3, r.owner().fullName(), st.text);
            xlsx.write(row, 4, r.owner().region(), st.text);
            for (int c = 5; c < 5 + n.tasters.size(); ++c)
                xlsx.write(row, c, QString(), st.score);   // рамки пустых ячеек
            for (const TasterScore& s : r.scores())
                xlsx.write(row, columnOf.value(s.tasterId), cellValue(s.evaluation.total()), st.score);
            xlsx.write(row, 5 + n.tasters.size(), cellValue(r.average()), st.average);
            xlsx.write(row, 6 + n.tasters.size(), r.completeCount(), st.center);
            ++row;
        }
        row += 1;   // пустая строка между номинациями
    }
    if (nominations.isEmpty())
        xlsx.write(row, 1, QStringLiteral("Образцы не зарегистрированы."), st.muted);
}

void writeDetailsSheet(QXlsx::Document& xlsx, const Styles& st, const QVector<NominationResult>& nominations)
{
    xlsx.selectSheet(kSheetDetails);
    const QStringList headers = {
        QStringLiteral("Номинация"), QStringLiteral("Шифр"), QStringLiteral("№ п/п"),
        QStringLiteral("ФИО участника"), QStringLiteral("Регион"), QStringLiteral("Команда"),
        QStringLiteral("Дегустатор"), criterionTitle(Criterion::Appearance),
        criterionTitle(Criterion::Consistency), criterionTitle(Criterion::Aroma),
        criterionTitle(Criterion::Taste), QStringLiteral("Итоговый балл"), QStringLiteral("Примечание"),
        QStringLiteral("Сохранено")};
    const double widths[] = {18, 8, 7, 30, 22, 9, 20, 11, 13, 11, 9, 11, 40, 17};
    for (int c = 0; c < headers.size(); ++c) {
        xlsx.write(1, c + 1, headers[c], st.header);
        xlsx.setColumnWidth(c + 1, widths[c]);
    }
    xlsx.setRowHeight(1, 32);

    int row = 2;
    for (const NominationResult& n : nominations)
        for (const SampleResult& r : n.results) {
            auto writeSample = [&]() {
                xlsx.write(row, 1, n.name, st.text);
                xlsx.write(row, 2, r.sample().cipher(), st.center);
                xlsx.write(row, 3, r.sample().seqNumber(), st.center);
                xlsx.write(row, 4, r.owner().fullName(), st.text);
                xlsx.write(row, 5, r.owner().region(), st.text);
            };
            if (r.scores().isEmpty()) {
                writeSample();
                xlsx.write(row, 7, QStringLiteral("оценок нет"), st.muted);
                ++row;
                continue;
            }
            for (const TasterScore& s : r.scores()) {
                writeSample();
                xlsx.write(row, 6, s.teamNumber, st.center);
                xlsx.write(row, 7, s.tasterName, st.text);
                for (int i = 0; i < kCriteriaCount; ++i) {
                    const auto v = s.evaluation.score(kAllCriteria[static_cast<size_t>(i)]);
                    xlsx.write(row, 8 + i, v ? QVariant(*v) : QVariant(QString()), st.center);
                }
                xlsx.write(row, 12, cellValue(s.evaluation.total()), st.score);
                xlsx.write(row, 13, s.evaluation.note(), st.text);
                xlsx.write(row, 14, s.evaluation.savedAt().toString(QStringLiteral("dd.MM.yyyy HH:mm")), st.center);
                ++row;
            }
        }
}
}  // namespace

void ResultExporter::write(const QString& path, const TastingSession& session,
                           const QVector<NominationResult>& nominations)
{
    QXlsx::Document xlsx;
    const QStringList existing = xlsx.sheetNames();
    if (existing.isEmpty())
        xlsx.addSheet(kSheetResults);
    else
        xlsx.renameSheet(existing.first(), kSheetResults);
    xlsx.addSheet(kSheetDetails);
    xlsx.setDocumentProperty(QStringLiteral("title"), QStringLiteral("Итоговая ведомость дегустации мёда"));
    xlsx.setDocumentProperty(QStringLiteral("creator"), QStringLiteral("СЭДМ"));

    const Styles styles;
    writeResultsSheet(xlsx, styles, session, nominations);
    writeDetailsSheet(xlsx, styles, nominations);
    xlsx.selectSheet(kSheetResults);
    if (!xlsx.saveAs(path))
        throw DomainError(QStringLiteral("Не удалось записать файл «%1». Возможно, он открыт в Excel — "
                                         "закройте его и повторите.").arg(path));
}

}  // namespace sedm
