/**
 * @file ImportDialog.h
 * @brief Загрузка реестра участников из CSV или XLSX с предварительным просмотром.
 */
#pragma once

#include "ParticipantImporter.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace sedm {
class RegistryService;
}

namespace sedm::admin {

/// Окно загрузки реестра (п. 4.3.1.1, 4.3.4 ТЗ): выбор файла, проверка строк,
/// просмотр замечаний (ошибки, повторы, новые номинации) и загрузка проверенных
/// строк одной транзакцией.
class ImportDialog : public QDialog {
    Q_OBJECT
public:
    ImportDialog(RegistryService& registry, QWidget* parent = nullptr);
    /// Сразу открыть файл (например, переданный из меню).
    void openFile(const QString& path);

private:
    void chooseFile();
    void showPreview();
    void import();

    RegistryService& m_registry;
    ImportPreview m_preview;
    QLabel* m_file;
    QTableWidget* m_table;
    QLabel* m_summary;
    QCheckBox* m_skipDuplicates;
    QPushButton* m_import;
};

}  // namespace sedm::admin
