/**
 * @file SetupWizard.h
 * @brief Мастер первичной настройки: сервер PostgreSQL, база СЭДМ, пароль администратора.
 */
#pragma once

#include "ConnectionSettings.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTimer;

namespace sedm {
class AuthService;
class Database;
}

namespace sedm::admin {

/// Первый запуск программы администратора (п. 4.1.4 ТЗ): либо первичная
/// настройка сервера (роль приложения с паролем SCRAM-SHA-256, база данных,
/// схема sedm — под учётной записью суперпользователя), либо подключение к уже
/// настроенной базе; затем — пароль администратора (хранится как bcrypt-хэш).
class SetupWizard : public QDialog {
    Q_OBJECT
public:
    enum class Start {
        Choose,         ///< выбор: настроить сервер или подключиться
        Install,        ///< сразу первичная настройка (база не найдена)
        AdminPassword   ///< только пароль администратора
    };

    SetupWizard(Database& db, const ConnectionSettings& current, Start start, QWidget* parent = nullptr);

private:
    enum Page { kChoose, kInstall, kConnect, kPassword, kDone };
    void goTo(Page page);
    void next();
    void back();
    bool install();
    bool connectExisting();
    bool createAdmin();
    void finish();

    Database& m_db;
    QStackedWidget* m_pages;
    Page m_page = kChoose;
    QString m_appPassword;        ///< пароль роли приложения (сообщается для ПК дегустаторов)
    bool m_installed = false;

    QRadioButton* m_modeInstall;
    QLineEdit* m_iHost;
    QSpinBox* m_iPort;
    QLineEdit* m_iSuper;
    QLineEdit* m_iSuperPass;
    QLineEdit* m_iDatabase;
    QLineEdit* m_iRole;
    QLineEdit* m_iRolePass;
    QLineEdit* m_cHost;
    QSpinBox* m_cPort;
    QLineEdit* m_cDatabase;
    QLineEdit* m_cUser;
    QLineEdit* m_cPass;
    QLineEdit* m_admin1;
    QLineEdit* m_admin2;
    QLabel* m_doneText;
    QPushButton* m_back;
    QPushButton* m_next;
};

/// Вход администратора по паролю с блокировкой после пяти ошибок (п. 4.4.5 ТЗ).
class AdminLoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit AdminLoginDialog(AuthService& auth, QWidget* parent = nullptr);

private:
    void tryLogin();
    void tick();

    AuthService& m_auth;
    QLineEdit* m_password;
    QLabel* m_message;
    QPushButton* m_enter;
    QTimer* m_timer;
    int m_lockLeft = 0;
};

}  // namespace sedm::admin
