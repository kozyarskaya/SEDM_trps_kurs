/**
 * @file AuditService.cpp
 * @brief Реализация журнала действий с локальным буфером на случай сбоя связи.
 */
#include "AuditService.h"
#include "AuditRepository.h"
#include "CsvWriter.h"
#include "CsvReader.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>

namespace sedm {

AuditService::AuditService(Database& db, const QString& pendingFile)
    : m_db(db), m_pendingFile(pendingFile)
{
}

void AuditService::setActor(const QString& login, const QString& role)
{
    m_actor = login;
    m_role = role;
}

QString AuditService::workstation()
{
    return QSysInfo::machineHostName();
}

void AuditService::log(const QString& action, const QString& details)
{
    const AuditEntry entry(QDateTime::currentDateTime(), m_actor, m_role, workstation(), action, details);
    try {
        AuditRepository(m_db).append(entry);
    } catch (const DbError&) {
        appendPending(entry);   // сервер недоступен — сохраняем локально
    }
}

void AuditService::appendPending(const AuditEntry& entry) const
{
    QDir().mkpath(QFileInfo(m_pendingFile).absolutePath());
    const bool fresh = !QFile::exists(m_pendingFile);
    QFile file(m_pendingFile);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    CsvWriter writer(file, fresh);
    writer.writeRow({entry.at().toString(Qt::ISODate), entry.actor(), entry.role(), entry.workstation(),
                     entry.action(), entry.details()});
}

int AuditService::pendingCount() const
{
    QFile file(m_pendingFile);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return CsvReader::parse(QString::fromUtf8(file.readAll())).size();
}

void AuditService::flushPending()
{
    QFile file(m_pendingFile);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;
    const auto rows = CsvReader::parse(QString::fromUtf8(file.readAll()));
    file.close();
    AuditRepository repo(m_db);
    Transaction tx(m_db);
    for (const QStringList& r : rows) {
        if (r.size() < 6)
            continue;
        repo.append(AuditEntry(QDateTime::fromString(r[0], Qt::ISODate), r[1], r[2], r[3], r[4], r[5]));
    }
    tx.commit();
    QFile::remove(m_pendingFile);
}

QVector<AuditEntry> AuditService::list(int limit, int offset) const
{
    return AuditRepository(m_db).list(limit, offset);
}

int AuditService::count() const
{
    return AuditRepository(m_db).count();
}

int AuditService::exportCsv(const QString& path)
{
    const QVector<AuditEntry> entries = AuditRepository(m_db).list();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw DomainError(QStringLiteral("Не удалось записать файл «%1»: %2").arg(path, file.errorString()));
    CsvWriter writer(file);
    writer.writeRow({QStringLiteral("Дата и время"), QStringLiteral("Пользователь"), QStringLiteral("Роль"),
                     QStringLiteral("Рабочее место"), QStringLiteral("Действие"), QStringLiteral("Подробности")});
    for (int i = entries.size() - 1; i >= 0; --i) {   // от старых записей к новым
        const AuditEntry& e = entries[i];
        writer.writeRow({e.at().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")), e.actor(), e.role(),
                         e.workstation(), e.action(), e.details()});
    }
    file.close();
    log(QStringLiteral("Выгрузка журнала в CSV"),
        QStringLiteral("Файл «%1», записей %2").arg(QFileInfo(path).fileName()).arg(entries.size()));
    return entries.size();
}

}  // namespace sedm
