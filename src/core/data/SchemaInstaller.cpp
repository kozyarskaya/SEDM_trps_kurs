/**
 * @file SchemaInstaller.cpp
 * @brief Реализация первичной настройки сервера PostgreSQL.
 */
#include "SchemaInstaller.h"
#include "Database.h"
#include "DbError.h"
#include "SqlUtil.h"

#include <QFile>
#include <QVariant>

namespace sedm {

using sql::quoteIdent;
using sql::quoteLiteral;

QString SchemaInstaller::schemaScript()
{
    QFile file(QStringLiteral(":/sql/schema.sql"));
    if (!file.open(QIODevice::ReadOnly))
        throw DbError(DbError::Kind::Other, QStringLiteral("Не найден сценарий схемы базы данных."));
    return QString::fromUtf8(file.readAll());
}

bool SchemaInstaller::isInstalled(Database& db)
{
    return db.scalar(QStringLiteral("SELECT to_regclass('sedm.schema_version') IS NOT NULL")).toBool();
}

SchemaInstaller::Result SchemaInstaller::install(const ConnectionSettings& superuser,
                                                 const ConnectionSettings& app)
{
    Result result;
    const QString role = app.user.trimmed();
    const QString dbName = app.database.trimmed();

    // 1. Роль приложения и база данных (подключение суперпользователя к базе postgres)
    {
        Database admin(QStringLiteral("sedm_setup"));
        ConnectionSettings s = superuser;
        s.database = QStringLiteral("postgres");
        admin.open(s);
        admin.execScript(QStringLiteral("SET password_encryption = 'scram-sha-256'"));

        QSqlQuery q = admin.prepare(QStringLiteral("SELECT 1 FROM pg_roles WHERE rolname = :r"));
        q.bindValue(QStringLiteral(":r"), role);
        admin.exec(q);
        if (q.next()) {
            admin.execScript(QStringLiteral("ALTER ROLE %1 LOGIN PASSWORD %2")
                                 .arg(quoteIdent(role), quoteLiteral(app.password)));
        } else {
            admin.execScript(QStringLiteral("CREATE ROLE %1 LOGIN PASSWORD %2")
                                 .arg(quoteIdent(role), quoteLiteral(app.password)));
            result.roleCreated = true;
        }

        QSqlQuery d = admin.prepare(QStringLiteral("SELECT 1 FROM pg_database WHERE datname = :d"));
        d.bindValue(QStringLiteral(":d"), dbName);
        admin.exec(d);
        if (!d.next()) {
            admin.execScript(QStringLiteral("CREATE DATABASE %1 ENCODING 'UTF8' TEMPLATE template0")
                                 .arg(quoteIdent(dbName)));
            result.databaseCreated = true;
        }
    }

    // 2. Права: доступ приложения только к схеме sedm (подключение суперпользователя к новой базе)
    {
        Database admin(QStringLiteral("sedm_setup"));
        ConnectionSettings s = superuser;
        s.database = dbName;
        admin.open(s);
        const QString r = quoteIdent(role);
        const QString dbq = quoteIdent(dbName);
        admin.execScript(QStringLiteral(
            "REVOKE ALL ON DATABASE %1 FROM PUBLIC;"
            "GRANT CONNECT, TEMPORARY, CREATE ON DATABASE %1 TO %2;"
            "REVOKE ALL ON SCHEMA public FROM PUBLIC;"
            "CREATE SCHEMA IF NOT EXISTS sedm AUTHORIZATION %2;"
            "ALTER ROLE %2 IN DATABASE %1 SET search_path = sedm;").arg(dbq, r));
    }

    // 3. Объекты схемы — от имени роли приложения (она становится их владельцем)
    {
        Database appDb(QStringLiteral("sedm_setup"));
        appDb.open(app);
        if (!isInstalled(appDb)) {
            Transaction tx(appDb);
            appDb.execScript(schemaScript());
            tx.commit();
            result.schemaCreated = true;
        }
    }
    return result;
}

}  // namespace sedm
