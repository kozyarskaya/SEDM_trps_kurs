/**
 * @file ParticipantImporter.cpp
 * @brief Реализация загрузки реестра участников.
 */
#include "ParticipantImporter.h"
#include "CsvReader.h"
#include "DomainError.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

#include "xlsxdocument.h"

namespace sedm {

int ImportPreview::validCount() const
{
    int n = 0;
    for (const ImportRow& r : rows)
        n += r.isValid() ? 1 : 0;
    return n;
}

int ImportPreview::errorCount() const
{
    return rows.size() - validCount();
}

int ImportPreview::duplicateCount() const
{
    int n = 0;
    for (const ImportRow& r : rows)
        n += (r.isValid() && r.duplicateOf) ? 1 : 0;
    return n;
}

// ------------------------------------------------------------------ источники
std::unique_ptr<ParticipantSource> ParticipantSource::forFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("csv") || ext == QLatin1String("txt"))
        return std::make_unique<CsvParticipantSource>();
    if (ext == QLatin1String("xlsx"))
        return std::make_unique<XlsxParticipantSource>();
    throw DomainError(QStringLiteral("Формат файла «.%1» не поддерживается. Выберите файл CSV или XLSX.").arg(ext));
}

QVector<QStringList> CsvParticipantSource::readTable(const QString& path) const
{
    return CsvReader::readFile(path);
}

QVector<QStringList> XlsxParticipantSource::readTable(const QString& path) const
{
    QXlsx::Document doc(path);
    if (!doc.load())
        throw DomainError(QStringLiteral("Не удалось прочитать книгу Excel «%1». Проверьте, что файл "
                                         "в формате XLSX и не открыт в другой программе.").arg(path));
    const QXlsx::CellRange range = doc.dimension();
    QVector<QStringList> table;
    for (int row = range.firstRow(); row <= range.lastRow(); ++row) {
        QStringList cells;
        bool any = false;
        for (int col = range.firstColumn(); col <= range.lastColumn(); ++col) {
            const QVariant v = doc.read(row, col);
            QString text;
            if (v.type() == QVariant::Double) {
                // телефоны в Excel часто хранятся числом: 79123456789 -> «79123456789»
                const double d = v.toDouble();
                text = (d == static_cast<double>(static_cast<qint64>(d)))
                           ? QString::number(static_cast<qint64>(d)) : QString::number(d);
            } else {
                text = v.toString();
            }
            text = text.trimmed();
            any = any || !text.isEmpty();
            cells.append(text);
        }
        if (any)
            table.append(cells);
    }
    return table;
}

// ------------------------------------------------------------ распознавание
ParticipantImporter::ColumnMap ParticipantImporter::mapHeaders(const QStringList& headers, bool* hasHeader)
{
    static const QHash<QString, int> names = {
        {QStringLiteral("фио"), 0}, {QStringLiteral("ф.и.о."), 0}, {QStringLiteral("ф. и. о."), 0},
        {QStringLiteral("фамилия имя отчество"), 0}, {QStringLiteral("участник"), 0},
        {QStringLiteral("пасечник"), 0}, {QStringLiteral("владелец"), 0}, {QStringLiteral("full name"), 0},
        {QStringLiteral("фамилия"), 1}, {QStringLiteral("имя"), 2}, {QStringLiteral("отчество"), 3},
        {QStringLiteral("телефон"), 4}, {QStringLiteral("тел"), 4}, {QStringLiteral("тел."), 4},
        {QStringLiteral("контактный телефон"), 4}, {QStringLiteral("phone"), 4},
        {QStringLiteral("e-mail"), 5}, {QStringLiteral("email"), 5}, {QStringLiteral("эл. почта"), 5},
        {QStringLiteral("электронная почта"), 5}, {QStringLiteral("почта"), 5},
        {QStringLiteral("регион"), 6}, {QStringLiteral("область"), 6}, {QStringLiteral("субъект рф"), 6},
        {QStringLiteral("region"), 6},
        {QStringLiteral("номинация"), 7}, {QStringLiteral("вид мёда"), 7}, {QStringLiteral("вид меда"), 7},
        {QStringLiteral("сорт мёда"), 7}, {QStringLiteral("nomination"), 7},
    };
    ColumnMap map;
    int recognized = 0;
    for (int i = 0; i < headers.size(); ++i) {
        const QString key = headers[i].simplified().toLower();
        const auto it = names.constFind(key);
        if (it == names.constEnd())
            continue;
        ++recognized;
        int* fields[] = {&map.fullName, &map.surname, &map.firstName, &map.patronymic,
                        &map.phone, &map.email, &map.region, &map.nomination};
        if (*fields[it.value()] < 0)
            *fields[it.value()] = i;
    }
    const bool header = recognized >= 2;
    if (hasHeader)
        *hasHeader = header;
    if (!header) {   // заголовка нет — порядок столбцов по ТЗ
        map = ColumnMap();
        map.fullName = 0;
        map.phone = 1;
        map.email = 2;
        map.region = 3;
        map.nomination = 4;
    }
    return map;
}

ImportPreview ParticipantImporter::buildPreview(const QVector<QStringList>& table, const QString& fileName)
{
    ImportPreview preview;
    preview.fileName = fileName;
    if (table.isEmpty())
        throw DomainError(QStringLiteral("Файл не содержит данных."));

    bool hasHeader = false;
    const ColumnMap map = mapHeaders(table.first(), &hasHeader);
    if (hasHeader && !map.isUsable())
        throw DomainError(QStringLiteral("В файле не найдены обязательные столбцы: ФИО (или Фамилия и Имя), "
                                         "Регион, Номинация."));
    preview.headers = hasHeader ? table.first() : QStringList{QStringLiteral("ФИО"), QStringLiteral("Телефон"),
                                                              QStringLiteral("E-mail"), QStringLiteral("Регион"),
                                                              QStringLiteral("Номинация")};
    auto cell = [](const QStringList& r, int i) { return (i >= 0 && i < r.size()) ? r[i].trimmed() : QString(); };

    QHash<QString, int> seen;   // ФИО+телефон -> номер строки (повторы внутри файла)
    for (int i = hasHeader ? 1 : 0; i < table.size(); ++i) {
        const QStringList& r = table[i];
        ImportRow row;
        row.line = i + 1;
        QString name = cell(r, map.fullName);
        if (name.isEmpty() && map.surname >= 0)
            name = QStringList{cell(r, map.surname), cell(r, map.firstName), cell(r, map.patronymic)}
                       .join(QLatin1Char(' ')).simplified();
        row.participant.setFullName(name);
        row.participant.setPhone(cell(r, map.phone));
        row.participant.setEmail(cell(r, map.email));
        row.participant.setRegion(cell(r, map.region));
        row.nominationName = cell(r, map.nomination).simplified();
        row.participant.setNominationId(row.nominationName.isEmpty() ? 0 : 1);   // проверка наличия
        row.problems = row.participant.validate();
        row.participant.setNominationId(0);

        static const QRegularExpression digits(QStringLiteral("[^0-9]"));
        const QString key = name.toLower() + QLatin1Char('|') + QString(row.participant.phone()).remove(digits);
        if (row.problems.isEmpty() && seen.contains(key))
            row.problems << QStringLiteral("Повтор строки %1 этого файла.").arg(seen.value(key));
        else
            seen.insert(key, row.line);
        preview.rows.append(row);
    }
    return preview;
}

}  // namespace sedm
