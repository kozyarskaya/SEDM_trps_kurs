/**
 * @file BackupPage.h
 * @brief Окно «Резервные копии»: создание и восстановление копий базы данных.
 */
#pragma once

#include "AdminPage.h"

class QLabel;
class QLineEdit;
class QTableWidget;

namespace sedm::admin {

class AutoBackupScheduler;

/// Резервное копирование (п. 4.2.1, 4.3.1.1, 4.4.4 ТЗ): копия по команде
/// администратора, восстановление из копии, список автоматических копий
/// (каждые 5 минут) и выбор каталога утилит PostgreSQL (pg_dump, psql).
class BackupPage : public AdminPage {
    Q_OBJECT
public:
    BackupPage(AdminContext& ctx, AutoBackupScheduler& scheduler, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("backup.html"); }

    void createBackup();     ///< меню «Сервис → Создать резервную копию»
    void restoreBackup();    ///< меню «Сервис → Восстановить из копии»

signals:
    /// База данных восстановлена — все окна должны перечитать данные.
    void databaseRestored();

private:
    void restoreFrom(const QString& path);
    void chooseToolsDir();
    QString pgBinDir() const;

    AutoBackupScheduler& m_scheduler;
    QLabel* m_method;
    QLineEdit* m_toolsDir;
    QLabel* m_autoStatus;
    QTableWidget* m_autoTable;
};

}  // namespace sedm::admin
