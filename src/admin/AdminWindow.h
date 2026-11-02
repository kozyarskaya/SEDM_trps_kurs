/**
 * @file AdminWindow.h
 * @brief Главное окно программы администратора (макет Б1).
 */
#pragma once

#include "AdminPage.h"
#include "AuditService.h"
#include "AuthService.h"

#include <QMainWindow>
#include <QTimer>
#include <QVector>

class QButtonGroup;
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;

namespace sedm::admin {

class AutoBackupScheduler;
class BackupPage;
class BasketsPage;
class MonitoringPage;
class ParticipantsPage;
class ResultsPage;
class SamplesPage;
class SessionPage;
class TeamsPage;

/// Главное окно администратора: тёмная боковая панель разделов со счётчиками и
/// ходом подготовки дегустации, страницы разделов, главное меню (структурная
/// схема меню администратора), строка состояния: подключение, автокопия, журнал.
class AdminWindow : public QMainWindow {
    Q_OBJECT
public:
    AdminWindow(Database& db, AuditService& audit, QWidget* parent = nullptr);
    ~AdminWindow() override;

private:
    void buildMenu();
    QWidget* buildSidebar();
    void addNav(const QString& text, const QString& icon, AdminPage* page, int countKey = -1);
    void showPage(AdminPage* page);
    void updateSummary();
    void updateConnection();
    void newProject();
    void connectionSettings();
    void changePassword();
    void refreshAll();
    void showHelp();

    Database& m_db;
    AuditService& m_audit;
    AuthService m_auth;
    AdminContext m_ctx;
    AutoBackupScheduler* m_scheduler;

    QStackedWidget* m_stack;
    QButtonGroup* m_navGroup;
    struct NavItem {
        QPushButton* button;
        AdminPage* page;
        QLabel* count;
        int countKey;
    };
    QVector<NavItem> m_nav;
    QLabel* m_sessionLabel;
    QProgressBar* m_prepBar;
    QLabel* m_prepText;
    QLabel* m_connection;
    QLabel* m_backupStatus;
    QLabel* m_auditStatus;
    QTimer m_pingTimer;

    ParticipantsPage* m_participants;
    SamplesPage* m_samples;
    BasketsPage* m_baskets;
    TeamsPage* m_teams;
    SessionPage* m_session;
    MonitoringPage* m_monitoring;
    ResultsPage* m_results;
    BackupPage* m_backup;
};

}  // namespace sedm::admin
