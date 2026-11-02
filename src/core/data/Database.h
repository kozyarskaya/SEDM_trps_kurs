/**
 * @file Database.h
 * @brief Подключение к PostgreSQL (драйвер Qt QPSQL) и выполнение запросов.
 */
#pragma once

#include "ConnectionSettings.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

namespace sedm {

/// Подключение к серверу СЭДМ. Все ошибки СУБД превращаются в исключение DbError
/// с понятным текстом. Взаимодействие — по протоколу PostgreSQL поверх TCP/IP с
/// аутентификацией SCRAM-SHA-256 (п. 4.1.2, 4.4.6 ТЗ).
class Database {
public:
    explicit Database(const QString& connectionName = QStringLiteral("sedm"));
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Открывает подключение (DbError при неудаче).
    void open(const ConnectionSettings& settings);
    void close();
    bool isOpen() const;
    /// Проверка связи с сервером (SELECT 1); при разрыве — попытка переподключения.
    bool ping();

    const ConnectionSettings& settings() const { return m_settings; }
    QSqlDatabase handle() const;

    /// Подготовленный запрос с именованными параметрами (:name).
    QSqlQuery prepare(const QString& sql) const;
    /// Выполняет подготовленный запрос (DbError при ошибке).
    void exec(QSqlQuery& query) const;
    /// Выполняет сценарий из нескольких команд (например, схему БД) — по одной
    /// команде, чтобы ошибка любой из них была обнаружена (см. SqlScript).
    void execScript(const QString& script) const;
    /// Выполняет команды по порядку; при ошибке — DbError (остальные не выполняются).
    void execStatements(const QStringList& statements) const;
    /// Выполняет запрос без параметров и возвращает первое значение первой строки.
    QVariant scalar(const QString& sql) const;

    void begin();
    void commit();
    void rollback();

    /// Есть ли драйвер QPSQL (библиотека libpq найдена).
    static bool driverAvailable();

private:
    QString m_name;                 ///< имя подключения Qt
    ConnectionSettings m_settings;  ///< параметры последнего открытия
};

/// Транзакция в стиле RAII: если commit() не вызван, изменения откатываются.
class Transaction {
public:
    explicit Transaction(Database& db) : m_db(db) { m_db.begin(); }
    ~Transaction();
    void commit();

private:
    Database& m_db;
    bool m_done = false;
};

}  // namespace sedm
