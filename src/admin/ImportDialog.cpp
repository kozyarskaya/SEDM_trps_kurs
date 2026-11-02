/**
 * @file ImportDialog.cpp
 * @brief Реализация окна загрузки реестра участников.
 */
#include "ImportDialog.h"
#include "AdminPage.h"
#include "Messages.h"
#include "RegistryService.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

ImportDialog::ImportDialog(RegistryService& registry, QWidget* parent)
    : QDialog(parent), m_registry(registry)
{
    setWindowTitle(QStringLiteral("Загрузка реестра участников"));
    resize(1100, 640);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);

    auto* intro = new QLabel(QStringLiteral(
        "Файл CSV (кодировка UTF-8, разделитель «;») или книга Excel (XLSX). Столбцы: ФИО; телефон; e-mail; "
        "регион; номинация — по заголовкам в первой строке или в этом порядке. Вместо ФИО можно указать "
        "отдельные столбцы «Фамилия», «Имя», «Отчество». Новые номинации будут добавлены в справочник."));
    intro->setWordWrap(true);
    intro->setProperty("role", "muted");
    layout->addWidget(intro);

    auto* fileRow = new QHBoxLayout;
    auto* choose = new QPushButton(QStringLiteral("Выбрать файл…"));
    m_file = new QLabel(QStringLiteral("Файл не выбран"));
    m_file->setProperty("role", "muted");
    fileRow->addWidget(choose);
    fileRow->addWidget(m_file, 1);
    layout->addLayout(fileRow);

    m_table = new QTableWidget;
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Строка"), QStringLiteral("ФИО"), QStringLiteral("Телефон"),
                                        QStringLiteral("E-mail"), QStringLiteral("Регион"), QStringLiteral("Номинация"),
                                        QStringLiteral("Замечания")});
    setupTable(m_table);
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    layout->addWidget(m_table, 1);

    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    m_skipDuplicates = new QCheckBox(QStringLiteral("Не добавлять повторно участников, которые уже есть в реестре "
                                                    "(совпадают ФИО и телефон)"));
    m_skipDuplicates->setChecked(true);
    layout->addWidget(m_skipDuplicates);

    auto* buttons = new QDialogButtonBox;
    m_import = buttons->addButton(QStringLiteral("Загрузить"), QDialogButtonBox::AcceptRole);
    m_import->setProperty("kind", "primary");
    m_import->setEnabled(false);
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(choose, &QPushButton::clicked, this, &ImportDialog::chooseFile);
    connect(buttons, &QDialogButtonBox::accepted, this, &ImportDialog::import);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_skipDuplicates, &QCheckBox::toggled, this, &ImportDialog::showPreview);
}

void ImportDialog::chooseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Реестр участников"), QString(),
        QStringLiteral("Реестр участников (*.csv *.xlsx *.txt);;Файлы CSV (*.csv *.txt);;Книги Excel (*.xlsx)"));
    if (!path.isEmpty())
        openFile(path);
}

void ImportDialog::openFile(const QString& path)
{
    ImportPreview preview;
    if (!ui::Messages::guard(this, [&]() { preview = m_registry.previewImport(path); },
                             QStringLiteral("Файл не прочитан")))
        return;
    m_preview = preview;
    m_file->setText(QDir::toNativeSeparators(path));
    showPreview();
}

void ImportDialog::showPreview()
{
    const QColor errorColor(0xF6, 0xDA, 0xD5);
    const QColor duplicateColor(0xFC, 0xEF, 0xD2);
    m_table->setRowCount(m_preview.rows.size());
    for (int r = 0; r < m_preview.rows.size(); ++r) {
        const ImportRow& row = m_preview.rows[r];
        QStringList notes = row.problems;
        if (row.isValid() && row.duplicateOf)
            notes << (m_skipDuplicates->isChecked() ? QStringLiteral("Уже есть в реестре — будет пропущен")
                                                    : QStringLiteral("Уже есть в реестре — будет добавлен повторно"));
        if (row.isValid() && row.newNomination)
            notes << QStringLiteral("Новая номинация «%1»").arg(Nomination::normalizedName(row.nominationName));
        const QStringList cells = {QString::number(row.line), row.participant.fullName(), row.participant.phone(),
                                   row.participant.email(), row.participant.region(), row.nominationName,
                                   notes.isEmpty() ? QStringLiteral("—") : notes.join(QStringLiteral(" "))};
        for (int c = 0; c < cells.size(); ++c) {
            auto* item = new QTableWidgetItem(cells[c]);
            if (!row.isValid())
                item->setBackground(errorColor);
            else if (row.duplicateOf)
                item->setBackground(duplicateColor);
            m_table->setItem(r, c, item);
        }
    }
    m_table->resizeColumnsToContents();
    const int valid = m_preview.validCount();
    const int duplicates = m_preview.duplicateCount();
    const int toAdd = m_skipDuplicates->isChecked() ? valid - duplicates : valid;
    m_summary->setText(QStringLiteral("Строк в файле: %1. Будет добавлено участников: %2. Строк с ошибками "
                                      "(не загружаются): %3. Уже есть в реестре: %4.")
                           .arg(m_preview.rows.size()).arg(toAdd).arg(m_preview.errorCount()).arg(duplicates));
    m_import->setEnabled(toAdd > 0);
}

void ImportDialog::import()
{
    ImportResult result;
    if (!ui::Messages::guard(this, [&]() { result = m_registry.applyImport(m_preview, m_skipDuplicates->isChecked()); },
                             QStringLiteral("Реестр не загружен")))
        return;
    QString text = QStringLiteral("Добавлено участников: %1.\nПропущено повторов: %2.\nПропущено строк с ошибками: %3.")
                       .arg(result.added).arg(result.skippedDuplicates).arg(result.skippedErrors);
    if (!result.createdNominations.isEmpty())
        text += QStringLiteral("\nДобавлены номинации: %1.").arg(result.createdNominations.join(QStringLiteral(", ")));
    ui::Messages::info(this, QStringLiteral("Реестр загружен"), text);
    accept();
}

}  // namespace sedm::admin
