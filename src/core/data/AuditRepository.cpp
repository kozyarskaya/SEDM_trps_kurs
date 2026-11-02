/**
 * @file AuditRepository.cpp
 * @brief Реализация хранения журнала действий.
 */
#include "AuditRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

void AuditRepository::append(const AuditEntry& entry) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO audit_log (at, actor, role, workstation, action, details) "
        "VALUES (:at, :actor, :role, :ws, :action, :details)"));
    q.bindValue(QStringLiteral(":at"), entry.at().isValid() ? entry.at() : QDateTime::currentDateTime());
    q.bindValue(QStringLiteral(":actor"), sql::text(entry.actor().left(150)));
    q.bindValue(QStringLiteral(":role"), sql::text(entry.role().left(30)));
    q.bindValue(QStringLiteral(":ws"), sql::text(entry.workstation().left(100)));
    q.bindValue(QStringLiteral(":action"), sql::text(entry.action().left(100)));
    q.bindValue(QStringLiteral(":details"), sql::text(entry.details()));
    m_db.exec(q);
}

QVector<AuditEntry> AuditRepository::list(int limit, int offset) const
{
    QString sql = QStringLiteral(
        "SELECT id, at, actor, role, workstation, action, details FROM audit_log ORDER BY at DESC, id DESC");
    if (limit > 0)
        sql += QStringLiteral(" LIMIT :limit OFFSET :offset");
    QSqlQuery q = m_db.prepare(sql);
    if (limit > 0) {
        q.bindValue(QStringLiteral(":limit"), limit);
        q.bindValue(QStringLiteral(":offset"), offset);
    }
    m_db.exec(q);
    QVector<AuditEntry> result;
    while (q.next()) {
        AuditEntry e(q.value(1).toDateTime(), q.value(2).toString(), q.value(3).toString(),
                     q.value(4).toString(), q.value(5).toString(), q.value(6).toString());
        e.setId(q.value(0).toLongLong());
        result.append(e);
    }
    return result;
}

int AuditRepository::count() const
{
    return m_db.scalar(QStringLiteral("SELECT count(*) FROM audit_log")).toInt();
}

}  // namespace sedm
