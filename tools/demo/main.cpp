/**
 * @file main.cpp
 * @brief sedm_demo — создание демонстрационной базы СЭДМ (для показа программы).
 *
 * Использование (параметры суперпользователя и пароль роли приложения —
 * в переменных окружения, чтобы пароли не были видны в списке процессов):
 *   set SEDM_DEMO_SUPERPASS=<пароль postgres>
 *   set SEDM_DEMO_APPPASS=<пароль роли приложения>
 *   sedm_demo --host localhost --port 5432 --database sedm --user sedm_app
 *             [--recreate] [--participants 40] [--teams 4] [--tasters 3] [--basket 6]
 *             [--evaluate] [--cards demo_cards.csv] [--save-connection]
 * --recreate удаляет указанную базу и создаёт её заново — только для учебного сервера!
 */
#include "AppPaths.h"
#include "AuditService.h"
#include "CoreResources.h"
#include "CsvWriter.h"
#include "Database.h"
#include "DbError.h"
#include "DemoSeeder.h"
#include "DomainError.h"
#include "SchemaInstaller.h"
#include "SqlUtil.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTextStream>

using namespace sedm;

namespace {
/// Вывод в консоль в кодировке UTF-8 (строковые литералы исходного текста — UTF-8).
struct Console {
    QTextStream stream{stdout};
    Console() { stream.setCodec("UTF-8"); }
    Console& operator<<(const char* text)
    {
        stream << QString::fromUtf8(text);
        stream.flush();
        return *this;
    }
    Console& operator<<(const QString& text)
    {
        stream << text;
        stream.flush();
        return *this;
    }
    Console& operator<<(int value)
    {
        stream << value;
        stream.flush();
        return *this;
    }
};
}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("sedm_demo"));
    initCoreResources();
    Console out;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Создание демонстрационной базы СЭДМ"));
    parser.addHelpOption();
    const QCommandLineOption host(QStringLiteral("host"), QStringLiteral("Адрес сервера"), QStringLiteral("host"), QStringLiteral("localhost"));
    const QCommandLineOption port(QStringLiteral("port"), QStringLiteral("Порт"), QStringLiteral("port"), QStringLiteral("5432"));
    const QCommandLineOption database(QStringLiteral("database"), QStringLiteral("База данных"), QStringLiteral("name"), QStringLiteral("sedm"));
    const QCommandLineOption user(QStringLiteral("user"), QStringLiteral("Роль приложения"), QStringLiteral("name"), QStringLiteral("sedm_app"));
    const QCommandLineOption superuser(QStringLiteral("superuser"), QStringLiteral("Суперпользователь"), QStringLiteral("name"), QStringLiteral("postgres"));
    const QCommandLineOption recreate(QStringLiteral("recreate"), QStringLiteral("Удалить базу и создать заново"));
    const QCommandLineOption participants(QStringLiteral("participants"), QStringLiteral("Участников"), QStringLiteral("n"), QStringLiteral("40"));
    const QCommandLineOption teams(QStringLiteral("teams"), QStringLiteral("Команд"), QStringLiteral("n"), QStringLiteral("4"));
    const QCommandLineOption tasters(QStringLiteral("tasters"), QStringLiteral("Дегустаторов в команде"), QStringLiteral("n"), QStringLiteral("3"));
    const QCommandLineOption basket(QStringLiteral("basket"), QStringLiteral("Образцов в корзине"), QStringLiteral("n"), QStringLiteral("6"));
    const QCommandLineOption evaluate(QStringLiteral("evaluate"), QStringLiteral("Выставить оценки всех образцов"));
    const QCommandLineOption cards(QStringLiteral("cards"), QStringLiteral("Файл CSV с логинами и PIN-кодами"), QStringLiteral("file"), QStringLiteral("demo_cards.csv"));
    const QCommandLineOption saveConnection(QStringLiteral("save-connection"),
        QStringLiteral("Записать параметры подключения в настройки СЭДМ этого пользователя"));
    parser.addOptions({host, port, database, user, superuser, recreate, participants, teams, tasters, basket,
                       evaluate, cards, saveConnection});
    parser.process(app);

    ConnectionSettings super;
    super.host = parser.value(host);
    super.port = parser.value(port).toInt();
    super.user = parser.value(superuser);
    super.password = qEnvironmentVariable("SEDM_DEMO_SUPERPASS");
    super.database = QStringLiteral("postgres");
    ConnectionSettings appConn = super;
    appConn.user = parser.value(user);
    appConn.password = qEnvironmentVariable("SEDM_DEMO_APPPASS");
    appConn.database = parser.value(database);
    if (super.password.isEmpty() || appConn.password.isEmpty()) {
        out << "Задайте переменные окружения SEDM_DEMO_SUPERPASS и SEDM_DEMO_APPPASS.\n";
        return 2;
    }

    try {
        if (parser.isSet(recreate)) {
            Database admin(QStringLiteral("sedm_demo_admin"));
            admin.open(super);
            admin.execScript(QStringLiteral("DROP DATABASE IF EXISTS %1 WITH (FORCE)").arg(sql::quoteIdent(appConn.database)));
            out << "База " << appConn.database << " удалена.\n";
        }
        const auto r = SchemaInstaller::install(super, appConn);
        out << "Сервер настроен: роль " << (r.roleCreated ? "создана" : "обновлена") << ", база "
            << (r.databaseCreated ? "создана" : "уже была") << ", схема " << (r.schemaCreated ? "создана" : "уже была")
            << ".\n";

        Database db;
        db.open(appConn);
        AuditService audit(db, QStringLiteral("sedm_demo_audit_pending.csv"));
        DemoOptions options;
        options.participants = parser.value(participants).toInt();
        options.teams = parser.value(teams).toInt();
        options.tastersPerTeam = parser.value(tasters).toInt();
        options.basketSize = parser.value(basket).toInt();
        options.evaluate = parser.isSet(evaluate);
        const DemoResult result = DemoSeeder::seed(db, audit, options);
        out << "Образцов: " << result.samples << ", корзин: " << result.baskets << ", дегустаторов: "
            << result.credentials.size() << ", оценок: " << result.evaluations << ".\n";
        out << "Пароль администратора: " << result.adminPassword << "\n";

        QFile file(parser.value(cards));
        if (file.open(QIODevice::WriteOnly)) {
            CsvWriter writer(file);
            writer.writeRow({QStringLiteral("Команда"), QStringLiteral("ФИО"), QStringLiteral("Логин"), QStringLiteral("PIN-код")});
            for (const TasterCredentials& c : result.credentials)
                writer.writeRow({c.teamTitle, c.fullName, c.login, c.pin});
            out << "Логины и PIN-коды дегустаторов записаны в " << file.fileName() << "\n";
        }
        if (parser.isSet(saveConnection)) {
            appConn.save(*AppPaths::settings());   // пароль — в защищённом виде (DPAPI)
            out << "Параметры подключения записаны в " << AppPaths::settingsFile() << "\n";
        }
    } catch (const DbError& e) {
        out << "Ошибка базы данных: " << e.message() << "\n" << e.details() << "\n";
        return 1;
    } catch (const DomainError& e) {
        out << "Ошибка: " << e.message() << "\n";
        return 1;
    }
    return 0;
}
