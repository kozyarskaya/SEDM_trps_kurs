/**
 * @file AppPaths.cpp
 * @brief Реализация путей к файлам клиента.
 */
#include "AppPaths.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace sedm {

namespace {
QString ensureDir(const QString& path)
{
    QDir().mkpath(path);
    return path;
}
}  // namespace

QString AppPaths::settingsFile()
{
    // GenericConfigLocation в Qt 5 для Windows — %LOCALAPPDATA% (локальные настройки пользователя)
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return ensureDir(base + QStringLiteral("/SEDM")) + QStringLiteral("/sedm.ini");
}

std::unique_ptr<QSettings> AppPaths::settings()
{
    auto s = std::make_unique<QSettings>(settingsFile(), QSettings::IniFormat);
    s->setIniCodec("UTF-8");
    return s;
}

QString AppPaths::dataDir()
{
    // %LOCALAPPDATA% — локальные (не перемещаемые) данные пользователя
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return ensureDir(base + QStringLiteral("/SEDM"));
}

QString AppPaths::autoBackupDir()
{
    return ensureDir(dataDir() + QStringLiteral("/autobackup"));
}

QString AppPaths::draftDir()
{
    return ensureDir(dataDir() + QStringLiteral("/drafts"));
}

QString AppPaths::pendingAuditFile()
{
    return dataDir() + QStringLiteral("/audit_pending.csv");
}

}  // namespace sedm
