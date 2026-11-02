/**
 * @file ResultsPage.cpp
 * @brief Реализация окна «Итоги и ведомость».
 */
#include "ResultsPage.h"
#include "Messages.h"
#include "ResultService.h"
#include "Theme.h"

#include <QComboBox>
#include <QDate>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

ResultsPage::ResultsPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Итоги и ведомость"), parent)
{
    auto* update = addAction(QStringLiteral("Обновить"), false, QStringLiteral("refresh"));
    auto* exportButton = addAction(QStringLiteral("Экспорт ведомости XLSX…"), true, QStringLiteral("export"));
    connect(update, &QPushButton::clicked, this, &ResultsPage::refresh);
    connect(exportButton, &QPushButton::clicked, this, &ResultsPage::exportXlsx);

    auto* pick = new QHBoxLayout;
    m_nomination = new QComboBox;
    m_nomination->setMinimumWidth(320);
    pick->addWidget(new QLabel(QStringLiteral("Номинация:")));
    pick->addWidget(m_nomination);
    pick->addStretch(1);
    body()->addLayout(pick);
    m_table = new QTableWidget;
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    body()->addWidget(m_table, 1);
    m_note = new QLabel(QStringLiteral(
        "Итоговый балл дегустатора — среднее четырёх оценок. Средний балл образца — среднее итоговых баллов всех "
        "дегустаторов, выставивших все четыре оценки (округление до сотых). Равные баллы — одно место, следующее "
        "место не пропускается (1, 2, 2, 3). Образцы без полных оценок места не получают."));
    m_note->setWordWrap(true);
    m_note->setProperty("role", "muted");
    body()->addWidget(m_note);
    connect(m_nomination, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ResultsPage::showNomination);
}

void ResultsPage::refresh()
{
    if (!run([&]() { m_results = ResultService(m_ctx.db, m_ctx.audit).compute(); }, false))
        return;
    const QString current = m_nomination->currentText();
    {
        const QSignalBlocker block(m_nomination);
        m_nomination->clear();
        for (const NominationResult& n : m_results)
            m_nomination->addItem(QStringLiteral("%1 (%2)").arg(n.name).arg(n.results.size()));
        const int idx = m_nomination->findText(current);
        m_nomination->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    int samples = 0, ranked = 0;
    for (const NominationResult& n : m_results)
        for (const SampleResult& r : n.results) {
            ++samples;
            ranked += r.place() > 0 ? 1 : 0;
        }
    setSubtitle(QStringLiteral("%1 номинаций · %2 образцов · места определены для %3").arg(m_results.size())
                    .arg(samples).arg(ranked));
    showNomination();
}

void ResultsPage::showNomination()
{
    const int index = m_nomination->currentIndex();
    m_table->clear();
    if (index < 0 || index >= m_results.size()) {
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        return;
    }
    const NominationResult& n = m_results[index];
    QStringList headers = {QStringLiteral("Место"), QStringLiteral("Шифр"), QStringLiteral("Участник"),
                           QStringLiteral("Регион")};
    QHash<int, int> column;
    for (int i = 0; i < n.tasters.size(); ++i) {
        headers << QStringLiteral("%1\n(к. %2)").arg(n.tasters[i].name).arg(n.tasters[i].teamNumber);
        column.insert(n.tasters[i].tasterId, 4 + i);
    }
    headers << QStringLiteral("Средний балл");
    m_table->setColumnCount(headers.size());
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setRowCount(n.results.size());
    const QLocale ru(QLocale::Russian);
    const QColor gold(0xFC, 0xEF, 0xD2);
    for (int r = 0; r < n.results.size(); ++r) {
        const SampleResult& s = n.results[r];
        auto set = [&](int c, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            if (s.place() == 1)
                item->setBackground(gold);
            m_table->setItem(r, c, item);
            return item;
        };
        QFont bold = m_table->font();
        bold.setBold(true);
        set(0, s.place() > 0 ? QString::number(s.place()) : QStringLiteral("—"))->setFont(bold);
        set(1, s.sample().cipher())->setFont(bold);
        set(2, s.owner().fullName());
        set(3, s.owner().region());
        for (int c = 4; c < 4 + n.tasters.size(); ++c)
            set(c, QString());
        for (const TasterScore& t : s.scores()) {
            const auto total = t.evaluation.total();
            set(column.value(t.tasterId), total ? ru.toString(*total, 'f', 2) : QStringLiteral("не завершена"));
        }
        const auto avg = s.average();
        set(headers.size() - 1, avg ? ru.toString(*avg, 'f', 2) : QStringLiteral("—"))->setFont(bold);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

void ResultsPage::exportXlsx()
{
    const QString name = QStringLiteral("Итоговая ведомость %1.xlsx").arg(QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Экспорт итоговой ведомости"), name,
                                                      QStringLiteral("Книга Excel (*.xlsx)"));
    if (path.isEmpty())
        return;
    QElapsedTimer timer;
    timer.start();
    if (!run([&]() { ResultService(m_ctx.db, m_ctx.audit).exportXlsx(path); }))
        return;
    const double seconds = timer.elapsed() / 1000.0;
    if (ui::Messages::confirm(this, QStringLiteral("Ведомость сформирована"),
                              QStringLiteral("Итоговая ведомость сохранена в файл «%1» (%2 с). Открыть её?")
                                  .arg(QFileInfo(path).fileName())
                                  .arg(QLocale(QLocale::Russian).toString(seconds, 'f', 1)),
                              QStringLiteral("Открыть"), QStringLiteral("Не открывать")))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

}  // namespace sedm::admin
