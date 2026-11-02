/**
 * @file main.cpp
 * @brief Точка входа программы дегустатора СЭДМ (sedm_taster.exe).
 *
 * Порядок запуска: ресурсы и тема оформления → параметры подключения к серверу
 * (при первом запуске — окно «Подключение к серверу») → подключение к базе
 * данных → главное окно дегустатора.
 */
#include "AppPaths.h"
#include "ConnectionDialog.h"
#include "CoreResources.h"
#include "Database.h"
#include "DbError.h"
#include "Icons.h"
#include "Localization.h"
#include "Messages.h"
#include "SchemaInstaller.h"
#include "TasterWindow.h"
#include "Theme.h"
#include "WidgetsResources.h"

#include <QApplication>
#include <QSettings>

static void initTasterResources()
{
    Q_INIT_RESOURCE(taster);
}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("СЭДМ — дегустатор"));
    QApplication::setOrganizationName(QStringLiteral("SEDM"));
    QApplication::setApplicationVersion(QStringLiteral(SEDM_VERSION));
    sedm::initCoreResources();
    sedm::ui::initWidgetsResources();
    initTasterResources();
    sedm::ui::installRussian(app);
    QApplication::setWindowIcon(sedm::ui::Icons::appIcon());
    sedm::ui::Theme::apply(app, 18, 1.0, sedm::ui::Theme::resourceText(QStringLiteral(":/taster/taster.qss")));

    // Подключение к серверу базы данных; при ошибке — повторить или изменить параметры
    const auto settings = sedm::AppPaths::settings();
    sedm::ConnectionSettings connection = sedm::ConnectionSettings::load(*settings);
    bool askSettings = !sedm::ConnectionSettings::exist(*settings) || !connection.isComplete();
    sedm::Database db;
    for (;;) {
        if (askSettings) {
            sedm::ui::ConnectionDialog dlg(connection);
            if (dlg.exec() != QDialog::Accepted)
                return 0;
            connection = dlg.settings();
            connection.save(*settings);
        }
        QString problem;
        try {
            db.open(connection);
            if (sedm::SchemaInstaller::isInstalled(db))
                break;
            problem = QStringLiteral("На сервере нет базы данных СЭДМ. Выполните первичную настройку в "
                                     "программе администратора.");
        } catch (const sedm::DbError& e) {
            problem = e.message();
        }
        const int answer = sedm::ui::Messages::choose(
            nullptr, QStringLiteral("Нет подключения к серверу"), problem,
            {QStringLiteral("Повторить"), QStringLiteral("Параметры подключения…"), QStringLiteral("Выход")}, 0, 2);
        if (answer == 2 || answer < 0)
            return 0;
        askSettings = (answer == 1);
    }

    sedm::taster::TasterWindow window(db);
    window.showMaximized();
    return app.exec();
}
