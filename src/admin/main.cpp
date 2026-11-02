/**
 * @file main.cpp
 * @brief Точка входа программы администратора СЭДМ (sedm_admin.exe).
 *
 * Порядок запуска: ресурсы и тема → подключение к серверу (при первом запуске —
 * мастер первичной настройки) → проверка схемы базы данных → пароль
 * администратора (задание при первом запуске, затем вход) → главное окно.
 */
#include "AdminWindow.h"
#include "AppPaths.h"
#include "AuditService.h"
#include "AuthService.h"
#include "ConnectionDialog.h"
#include "CoreResources.h"
#include "Database.h"
#include "DbError.h"
#include "Icons.h"
#include "Localization.h"
#include "Messages.h"
#include "SchemaInstaller.h"
#include "SetupWizard.h"
#include "Theme.h"
#include "WidgetsResources.h"

#include <QApplication>
#include <QSettings>

using namespace sedm;

static void initAdminResources()
{
    Q_INIT_RESOURCE(admin);
}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("СЭДМ — администратор"));
    QApplication::setOrganizationName(QStringLiteral("SEDM"));
    QApplication::setApplicationVersion(QStringLiteral(SEDM_VERSION));
    initCoreResources();
    ui::initWidgetsResources();
    initAdminResources();
    ui::installRussian(app);
    QApplication::setWindowIcon(ui::Icons::appIcon());
    ui::Theme::apply(app, 14);

    const auto settings = AppPaths::settings();
    ConnectionSettings connection = ConnectionSettings::load(*settings);
    Database db;
    if (!ConnectionSettings::exist(*settings)) {
        admin::SetupWizard wizard(db, connection, admin::SetupWizard::Start::Choose);
        if (wizard.exec() != QDialog::Accepted)
            return 0;
    } else {
        for (;;) {
            try {
                db.open(connection);
                break;
            } catch (const DbError& e) {
                const int answer = ui::Messages::choose(
                    nullptr, QStringLiteral("Нет подключения к серверу"), e.message(),
                    {QStringLiteral("Повторить"), QStringLiteral("Параметры подключения…"),
                     QStringLiteral("Первичная настройка…"), QStringLiteral("Выход")}, 0, 3);
                if (answer == 1) {
                    ui::ConnectionDialog dlg(connection);
                    if (dlg.exec() == QDialog::Accepted) {
                        connection = dlg.settings();
                        connection.save(*settings);
                    }
                } else if (answer == 2) {
                    admin::SetupWizard wizard(db, connection, admin::SetupWizard::Start::Install);
                    if (wizard.exec() == QDialog::Accepted)
                        break;
                } else if (answer != 0) {
                    return 0;
                }
            }
        }
    }
    try {
        if (!SchemaInstaller::isInstalled(db)) {
            admin::SetupWizard wizard(db, db.settings(), admin::SetupWizard::Start::Install);
            if (wizard.exec() != QDialog::Accepted)
                return 0;
        }
    } catch (const DbError& e) {
        ui::Messages::error(nullptr, QStringLiteral("Ошибка базы данных"), e.message());
        return 1;
    }

    AuditService audit(db, AppPaths::pendingAuditFile());
    try {
        audit.flushPending();   // записи журнала, отложенные при сбое связи
    } catch (const DbError&) {
    }
    AuthService auth(db, audit);
    bool adminExists = true;
    if (!ui::Messages::guard(nullptr, [&]() { adminExists = auth.adminExists(); }))
        return 1;
    if (!adminExists) {
        admin::SetupWizard wizard(db, db.settings(), admin::SetupWizard::Start::AdminPassword);
        if (wizard.exec() != QDialog::Accepted)
            return 0;
    }
    admin::AdminLoginDialog login(auth);
    if (login.exec() != QDialog::Accepted)
        return 0;

    admin::AdminWindow window(db, audit);
    window.showMaximized();
    return app.exec();
}
