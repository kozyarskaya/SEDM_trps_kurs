/**
 * @file BackupService.cpp
 * @brief Реализация резервного копирования и восстановления.
 */
#include "BackupService.h"
#include "AuditService.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"
#include "SchemaInstaller.h"
#include "SqlScript.h"
#include "SqlUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextCodec>
#include <QVersionNumber>

#include <algorithm>

namespace sedm {

namespace {
constexpr int kToolTimeoutMs = 120000;   ///< предельное время работы pg_dump и psql
constexpr int kRowsPerInsert = 100;      ///< строк в одной команде INSERT

/// Таблицы в порядке зависимостей и порядок строк в каждой.
struct TableSpec {
    const char* name;
    const char* orderBy;
};
const TableSpec kTables[] = {
    {"schema_version", "version"}, {"nomination", "id"}, {"participant", "id"}, {"team", "id"},
    {"basket", "id"}, {"sample", "id"}, {"taster", "id"}, {"evaluation", "taster_id, sample_id"},
    {"score_draft", "taster_id, sample_id"}, {"basket_progress", "taster_id, basket_id"},
    {"tasting_session", "id"}, {"app_admin", "id"}, {"audit_log", "id"},
};

/// Сообщения утилит PostgreSQL: UTF-8 или кодовая страница Windows.
QString decodeToolOutput(const QByteArray& bytes)
{
    QTextCodec::ConverterState state;
    const QString utf = QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
    return state.invalidChars > 0 ? QString::fromLocal8Bit(bytes).trimmed() : utf.trimmed();
}

QProcessEnvironment toolEnvironment(const Database& db)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PGPASSWORD"), db.settings().password);   // только для дочернего процесса
    env.insert(QStringLiteral("PGCONNECT_TIMEOUT"), QStringLiteral("5"));
    env.insert(QStringLiteral("PGCLIENTENCODING"), QStringLiteral("UTF8"));
    return env;
}

QStringList connectionArgs(const Database& db)
{
    const ConnectionSettings& s = db.settings();
    return {QStringLiteral("--host=%1").arg(s.host), QStringLiteral("--port=%1").arg(s.port),
            QStringLiteral("--username=%1").arg(s.user), QStringLiteral("--dbname=%1").arg(s.database),
            QStringLiteral("--no-password")};
}
}  // namespace

BackupService::BackupService(Database& db, AuditService* audit)
    : m_db(db), m_audit(audit)
{
}

// --------------------------------------------------------- поиск утилит PostgreSQL
QString BackupService::findPgTool(const QString& name, const QString& preferredDir)
{
#ifdef Q_OS_WIN
    const QString exe = name + QStringLiteral(".exe");
#else
    const QString exe = name;
#endif
    if (!preferredDir.isEmpty()) {
        const QString candidate = QDir(preferredDir).filePath(exe);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    const QString inPath = QStandardPaths::findExecutable(name);
    if (!inPath.isEmpty())
        return inPath;

    // Стандартные каталоги установщика PostgreSQL: <Program Files>\PostgreSQL\<версия>\bin
    QStringList roots;
    for (const char* var : {"ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"}) {
        const QString root = qEnvironmentVariable(var);
        if (!root.isEmpty() && !roots.contains(root))
            roots << root;
    }
    QString best;
    QVersionNumber bestVersion;
    for (const QString& root : roots) {
        const QDir pg(QDir(root).filePath(QStringLiteral("PostgreSQL")));
        for (const QString& ver : pg.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString candidate = pg.filePath(ver + QStringLiteral("/bin/") + exe);
            const QVersionNumber v = QVersionNumber::fromString(ver);
            if (QFileInfo::exists(candidate) && (best.isEmpty() || v > bestVersion)) {
                best = candidate;
                bestVersion = v;
            }
        }
    }
    return best;
}

QString BackupService::toolVersion(const QString& toolPath)
{
    if (toolPath.isEmpty())
        return QString();
    QProcess p;
    p.start(toolPath, {QStringLiteral("--version")});
    if (!p.waitForFinished(5000))
        return QString();
    return decodeToolOutput(p.readAllStandardOutput());
}

// ------------------------------------------------------------------- создание
bool BackupService::runPgDump(const QString& pgDump, const QString& path, QString* error) const
{
    QStringList args = connectionArgs(m_db);
    args << QStringLiteral("--schema=sedm") << QStringLiteral("--format=plain")
         << QStringLiteral("--no-owner") << QStringLiteral("--no-privileges")
         << QStringLiteral("--clean") << QStringLiteral("--if-exists")
         << QStringLiteral("--rows-per-insert=%1").arg(kRowsPerInsert)
         << QStringLiteral("--encoding=UTF8") << QStringLiteral("--file=%1").arg(QDir::toNativeSeparators(path));
    QProcess p;
    p.setProcessEnvironment(toolEnvironment(m_db));
    p.start(pgDump, args);
    if (!p.waitForStarted(10000)) {
        *error = QStringLiteral("не удалось запустить %1").arg(pgDump);
        return false;
    }
    if (!p.waitForFinished(kToolTimeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        *error = QStringLiteral("превышено время ожидания");
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        *error = decodeToolOutput(p.readAllStandardError());
        QFile::remove(path);
        return false;
    }
    return true;
}

QString BackupService::builtinDump() const
{
    QString out;
    const QString version = m_db.scalar(QStringLiteral("SHOW server_version")).toString();
    out += QStringLiteral(
        "--\n-- СЭДМ — резервная копия базы данных (SQL-дамп схемы sedm)\n"
        "-- Создана встроенным модулем копирования %1\n"
        "-- Сервер PostgreSQL %2, база данных «%3»\n"
        "-- Восстановление: программа администратора СЭДМ (Сервис → Восстановить из копии…)\n"
        "-- или: psql -v ON_ERROR_STOP=1 --single-transaction -f <файл>\n--\n\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")), version,
             m_db.settings().database);
    out += QStringLiteral("SET client_encoding = 'UTF8';\nSET standard_conforming_strings = on;\n"
                          "SET client_min_messages = warning;\n\n"
                          "DROP SCHEMA IF EXISTS sedm CASCADE;\nCREATE SCHEMA sedm;\n\n");
    out += SchemaInstaller::schemaScript();
    out += QStringLiteral("\n\n-- =============================== Данные ===============================\n"
                          "SET search_path TO sedm;\n"
                          "TRUNCATE schema_version, tasting_session;   -- строки, добавленные сценарием схемы\n");

    for (const TableSpec& t : kTables) {
        const QString table = QString::fromLatin1(t.name);
        // Столбцы таблицы — из системного каталога (порядок как в определении)
        QSqlQuery cols = m_db.prepare(QStringLiteral(
            "SELECT attname FROM pg_attribute WHERE attrelid = CAST(:t AS regclass) "
            "AND attnum > 0 AND NOT attisdropped ORDER BY attnum"));
        cols.bindValue(QStringLiteral(":t"), QStringLiteral("sedm.") + table);
        m_db.exec(cols);
        QStringList names, quoted;
        while (cols.next()) {
            names << sql::quoteIdent(cols.value(0).toString());
            quoted << QStringLiteral("quote_nullable(%1)").arg(names.last());
        }
        // Значения форматирует сервер (quote_nullable): точное текстовое представление
        QSqlQuery rows = m_db.prepare(QStringLiteral("SELECT concat_ws(', ', %1) FROM sedm.%2 ORDER BY %3")
                                          .arg(quoted.join(QStringLiteral(", ")), sql::quoteIdent(table),
                                               QString::fromLatin1(t.orderBy)));
        m_db.exec(rows);
        out += QStringLiteral("\n-- Таблица %1\n").arg(table);
        int inBatch = 0;
        while (rows.next()) {
            if (inBatch == 0)
                out += QStringLiteral("INSERT INTO %1 (%2) VALUES\n").arg(sql::quoteIdent(table),
                                                                          names.join(QStringLiteral(", ")));
            else
                out += QStringLiteral(",\n");
            out += QStringLiteral("    (") + rows.value(0).toString() + QLatin1Char(')');
            if (++inBatch == kRowsPerInsert) {
                out += QStringLiteral(";\n");
                inBatch = 0;
            }
        }
        if (inBatch > 0)
            out += QStringLiteral(";\n");
    }

    // Текущие значения последовательностей (номера записей и сквозной номер образцов)
    out += QStringLiteral("\n-- Последовательности\n");
    QSqlQuery seqs = m_db.prepare(QStringLiteral(
        "SELECT c.relname FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace "
        "WHERE n.nspname = 'sedm' AND c.relkind = 'S' ORDER BY c.relname"));
    m_db.exec(seqs);
    QStringList sequences;
    while (seqs.next())
        sequences << seqs.value(0).toString();
    for (const QString& seq : sequences) {
        QSqlQuery v = m_db.prepare(QStringLiteral("SELECT last_value, is_called FROM sedm.%1").arg(sql::quoteIdent(seq)));
        m_db.exec(v);
        if (v.next())
            out += QStringLiteral("SELECT pg_catalog.setval(%1, %2, %3);\n")
                       .arg(sql::quoteLiteral(QStringLiteral("sedm.") + sql::quoteIdent(seq)),
                            v.value(0).toString(),
                            v.value(1).toBool() ? QStringLiteral("true") : QStringLiteral("false"));
    }
    out += QStringLiteral("\n-- Конец резервной копии\n");
    return out;
}

BackupResult BackupService::writeBackup(const QString& path) const
{
    BackupResult result;
    result.path = path;
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QString pgDump = findPgTool(QStringLiteral("pg_dump"), m_pgBinDir);
    QString error;
    if (!pgDump.isEmpty() && runPgDump(pgDump, path, &error)) {
        result.usedPgDump = true;
    } else {
        result.note = pgDump.isEmpty()
                          ? QStringLiteral("Утилита pg_dump не найдена — копия создана встроенным модулем.")
                          : QStringLiteral("pg_dump завершилась с ошибкой (%1) — копия создана встроенным модулем.")
                                .arg(error.section(QLatin1Char('\n'), 0, 0));
        const QString dump = builtinDump();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(dump.toUtf8()) < 0 || !file.commit())
            throw DomainError(QStringLiteral("Не удалось записать файл резервной копии «%1».").arg(path));
    }
    return result;
}

BackupResult BackupService::createBackup(const QString& path)
{
    const BackupResult result = writeBackup(path);
    if (m_audit)
        m_audit->log(QStringLiteral("Создание резервной копии"),
                     QStringLiteral("Файл «%1» (%2)").arg(QFileInfo(path).fileName(),
                         result.usedPgDump ? QStringLiteral("pg_dump") : QStringLiteral("встроенный модуль")));
    return result;
}

BackupResult BackupService::createAutoBackup(const QString& dir, const QString& prefix)
{
    QDir().mkpath(dir);
    const QString name = QStringLiteral("%1_%2.sql").arg(prefix,
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    // автокопии не записываются в журнал: они создаются каждые 5 минут
    const BackupResult r = writeBackup(QDir(dir).filePath(name));
    rotate(dir, prefix, kAutoBackupKeep);
    return r;
}

QString BackupService::changeMarker() const
{
    return m_db.scalar(QStringLiteral(
        "SELECT concat_ws('/', (SELECT max(id) FROM audit_log), (SELECT count(*) FROM evaluation), "
        "(SELECT max(saved_at) FROM evaluation), (SELECT count(*) FROM sample))")).toString();
}

void BackupService::rotate(const QString& dir, const QString& prefix, int keep)
{
    QDir d(dir);
    const QStringList files = d.entryList({prefix + QStringLiteral("_*.sql")}, QDir::Files, QDir::Name | QDir::Reversed);
    for (int i = keep; i < files.size(); ++i)
        d.remove(files[i]);
}

QVector<BackupFileInfo> BackupService::listBackups(const QString& dir)
{
    QVector<BackupFileInfo> list;
    for (const QFileInfo& fi : QDir(dir).entryInfoList({QStringLiteral("*.sql")}, QDir::Files, QDir::Time)) {
        BackupFileInfo b;
        b.path = fi.absoluteFilePath();
        b.createdAt = fi.lastModified();
        b.size = fi.size();
        list.append(b);
    }
    return list;
}

// -------------------------------------------------------------- восстановление
void BackupService::runPsql(const QString& psql, const QString& path) const
{
    QStringList args = connectionArgs(m_db);
    args << QStringLiteral("--no-psqlrc") << QStringLiteral("--quiet") << QStringLiteral("--set=ON_ERROR_STOP=1")
         << QStringLiteral("--single-transaction") << QStringLiteral("--file=%1").arg(QDir::toNativeSeparators(path));
    QProcess p;
    p.setProcessEnvironment(toolEnvironment(m_db));
    p.start(psql, args);
    if (!p.waitForStarted(10000))
        throw DomainError(QStringLiteral("Не удалось запустить утилиту psql (%1).").arg(psql));
    if (!p.waitForFinished(kToolTimeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        throw DomainError(QStringLiteral("Восстановление не завершилось за отведённое время."));
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        throw DomainError(QStringLiteral("Восстановление не выполнено, данные не изменены. Сообщение psql:\n%1")
                              .arg(decodeToolOutput(p.readAllStandardError())));
}

void BackupService::restoreBackup(const QString& path, const QString& safetyDir)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw DomainError(QStringLiteral("Не удалось открыть файл «%1»: %2").arg(path, file.errorString()));
    QString script = QString::fromUtf8(file.readAll());
    file.close();
    if (script.startsWith(QChar(0xFEFF)))
        script.remove(0, 1);

    // Проверка: это копия базы СЭДМ (есть таблицы оценок и сеанса)
    static const QRegularExpression evaluationTable(QStringLiteral("CREATE TABLE (sedm\\.)?evaluation\\b"));
    static const QRegularExpression sessionTable(QStringLiteral("CREATE TABLE (sedm\\.)?tasting_session\\b"));
    if (!evaluationTable.match(script).hasMatch() || !sessionTable.match(script).hasMatch())
        throw DomainError(QStringLiteral("Файл «%1» не является резервной копией базы данных СЭДМ.")
                              .arg(QFileInfo(path).fileName()));

    // Страховочная копия текущего состояния — восстановление можно отменить
    createAutoBackup(safetyDir, QStringLiteral("sedm_before_restore"));

    QString method;
    if (SqlScript::containsCopyFromStdin(script)) {
        const QString psql = findPgTool(QStringLiteral("psql"), m_pgBinDir);
        if (psql.isEmpty())
            throw DomainError(QStringLiteral("Копия содержит данные в формате COPY — для её восстановления нужна "
                                             "утилита psql из состава PostgreSQL. Укажите каталог bin "
                                             "PostgreSQL в параметрах подключения."));
        runPsql(psql, path);
        method = QStringLiteral("psql");
    } else {
        QStringList statements = SqlScript::splitStatements(script);
        // Параметр transaction_timeout (его задаёт pg_dump 17+) неизвестен серверам до версии 17
        if (m_db.scalar(QStringLiteral("SHOW server_version_num")).toInt() < 170000) {
            static const QRegularExpression newParam(QStringLiteral("^SET\\s+transaction_timeout\\b"),
                                                     QRegularExpression::CaseInsensitiveOption);
            statements.erase(std::remove_if(statements.begin(), statements.end(),
                                            [](const QString& s) { return newParam.match(s).hasMatch(); }),
                             statements.end());
        }
        // Все команды — в одной транзакции: при ошибке данные остаются прежними
        try {
            Transaction tx(m_db);
            m_db.execStatements(statements);
            tx.commit();
        } catch (const DbError& e) {
            m_db.open(m_db.settings());
            throw DomainError(QStringLiteral("Восстановление не выполнено, данные не изменены.\n%1")
                                  .arg(e.details().section(QLatin1Char('\n'), 0, 0)));
        }
        method = QStringLiteral("встроенный модуль");
    }
    m_db.open(m_db.settings());   // новое подключение: сброс параметров сеанса, заданных дампом
    if (!SchemaInstaller::isInstalled(m_db))
        throw DomainError(QStringLiteral("После восстановления не найдена схема данных СЭДМ."));
    if (m_audit)
        m_audit->log(QStringLiteral("Восстановление из резервной копии"),
                     QStringLiteral("Файл «%1» (%2)").arg(QFileInfo(path).fileName(), method));
}

}  // namespace sedm
