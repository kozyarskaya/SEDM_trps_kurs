/**
 * @file Database.cpp
 * @brief Реализация подключения к PostgreSQL.
 */
#include "Database.h"
#include "DbError.h"
#include "SqlScript.h"

#include <QSqlError>
#include <QVariant>

namespace sedm {

Database::Database(const QString& connectionName)
    : m_name(connectionName)
{
}

Database::~Database()
{
    close();
}

bool Database::driverAvailable()
{
    return QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"));
}

void Database::open(const ConnectionSettings& settings)
{
    close();
    if (!driverAvailable())
        throw DbError(DbError::Kind::Connection,
                      QStringLiteral("Не найден драйвер PostgreSQL (QPSQL). Переустановите программу "
                                     "или проверьте наличие библиотеки libpq.dll рядом с ней."));
    m_settings = settings;
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_name);
    db.setHostName(settings.host.trimmed());
    db.setPort(settings.port);
    db.setDatabaseName(settings.database.trimmed());
    db.setUserName(settings.user.trimmed());
    db.setPassword(settings.password);
    db.setConnectOptions(QStringLiteral("connect_timeout=5;application_name=SEDM"));
    if (!db.open()) {
        const QSqlError err = db.lastError();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
        throw DbError::fromSqlError(err);
    }
    QSqlQuery q(db);
    q.exec(QStringLiteral("SET search_path TO sedm, pg_catalog"));
    q.exec(QStringLiteral("SET client_encoding TO 'UTF8'"));
}

void Database::close()
{
    if (!QSqlDatabase::contains(m_name))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_name, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_name);
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_name) && QSqlDatabase::database(m_name, false).isOpen();
}

bool Database::ping()
{
    if (isOpen()) {
        QSqlQuery q(handle());
        if (q.exec(QStringLiteral("SELECT 1")))
            return true;
    }
    try {
        open(m_settings);   // переподключение после сбоя сети или сервера
        return true;
    } catch (const DbError&) {
        return false;
    }
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_name, false);
}

QSqlQuery Database::prepare(const QString& sql) const
{
    QSqlQuery q(handle());
    if (!q.prepare(sql))
        throw DbError::fromSqlError(q.lastError());
    return q;
}

void Database::exec(QSqlQuery& query) const
{
    if (!query.exec())
        throw DbError::fromSqlError(query.lastError());
}

void Database::execScript(const QString& script) const
{
    execStatements(SqlScript::splitStatements(script));
}

void Database::execStatements(const QStringList& statements) const
{
    QSqlQuery q(handle());
    for (const QString& statement : statements)
        if (!q.exec(statement))
            throw DbError::fromSqlError(q.lastError());
}

QVariant Database::scalar(const QString& sql) const
{
    QSqlQuery q(handle());
    if (!q.exec(sql))
        throw DbError::fromSqlError(q.lastError());
    return q.next() ? q.value(0) : QVariant();
}

void Database::begin()
{
    if (!handle().transaction())
        throw DbError::fromSqlError(handle().lastError());
}

void Database::commit()
{
    if (!handle().commit())
        throw DbError::fromSqlError(handle().lastError());
}

void Database::rollback()
{
    handle().rollback();
}

Transaction::~Transaction()
{
    if (!m_done)
        m_db.rollback();
}

void Transaction::commit()
{
    m_db.commit();
    m_done = true;
}

}  // namespace sedm
