/**
 * @file BackupService.h
 * @brief Резервное копирование и восстановление базы данных (SQL-дамп формата pg_dump).
 */
#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace sedm {

class Database;
class AuditService;

/// Итог создания резервной копии.
struct BackupResult {
    QString path;            ///< файл копии
    bool usedPgDump = false; ///< копия создана утилитой pg_dump (иначе — встроенным модулем)
    QString note;            ///< пояснение (например, почему pg_dump не использована)
};

/// Файл резервной копии в каталоге копий.
struct BackupFileInfo {
    QString path;
    QDateTime createdAt;
    qint64 size = 0;
};

/// Сервис резервного копирования (п. 4.2.1, 4.3.1.1, 4.4.4 ТЗ; решение В16 —
/// гибридный способ). Копия — текстовый SQL-дамп схемы sedm:
/// 1) утилитой pg_dump из установленного PostgreSQL (формат pg_dump, данные
///    командами INSERT — копию можно восстановить и без psql);
/// 2) если pg_dump не найдена или завершилась с ошибкой — встроенным модулем
///    (тот же формат: сценарий схемы и команды INSERT).
/// Восстановление выполняется в одной транзакции: при ошибке данные не меняются.
class BackupService {
public:
    static constexpr int kAutoBackupKeep = 12;          ///< хранится автокопий (1 час)
    static constexpr int kAutoIntervalMinutes = 5;      ///< период автокопий (п. 4.4.4 ТЗ)

    /// audit может отсутствовать (автокопии в фоновом потоке журнал не пишут).
    explicit BackupService(Database& db, AuditService* audit = nullptr);

    /// Каталог bin PostgreSQL, заданный администратором (пусто — поиск автоматически).
    void setPgBinDir(const QString& dir) { m_pgBinDir = dir; }

    BackupResult createBackup(const QString& path);
    /// Автокопия в каталог dir с удалением старых (хранятся kAutoBackupKeep последних).
    BackupResult createAutoBackup(const QString& dir, const QString& prefix = QStringLiteral("sedm_auto"));
    /// Метка изменения данных: автокопия не создаётся, если данные не менялись.
    QString changeMarker() const;
    /// Восстанавливает базу данных из файла копии; перед этим — страховочная автокопия.
    void restoreBackup(const QString& path, const QString& safetyDir);

    /// Текст встроенного SQL-дампа.
    QString builtinDump() const;
    static QVector<BackupFileInfo> listBackups(const QString& dir);
    /// Путь к утилите PostgreSQL (pg_dump, psql): каталог администратора, PATH,
    /// стандартные каталоги установки (наибольшая версия). Пусто — не найдена.
    static QString findPgTool(const QString& name, const QString& preferredDir = QString());
    /// Версия утилиты PostgreSQL («pg_dump (PostgreSQL) 18.6»); пусто — не запускается.
    static QString toolVersion(const QString& toolPath);

private:
    BackupResult writeBackup(const QString& path) const;
    bool runPgDump(const QString& pgDump, const QString& path, QString* error) const;
    void runPsql(const QString& psql, const QString& path) const;
    static void rotate(const QString& dir, const QString& prefix, int keep);

    Database& m_db;
    AuditService* m_audit;
    QString m_pgBinDir;
};

}  // namespace sedm
