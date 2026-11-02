/**
 * @file SessionRepository.cpp
 * @brief Реализация хранения сеанса, учётной записи администратора, очистки данных.
 */
#include "SessionRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

TastingSession SessionRepository::load() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT title, starts_at, allow_reentry FROM tasting_session WHERE id = 1"));
    m_db.exec(q);
    TastingSession s;
    if (q.next()) {
        s.setTitle(q.value(0).toString());
        s.setStartsAt(q.value(1).toDateTime());
        s.setAllowReentry(q.value(2).toBool());
    }
    return s;
}

void SessionRepository::save(const TastingSession& session) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO tasting_session (id, title, starts_at, allow_reentry) VALUES (1, :t, :s, :r) "
        "ON CONFLICT (id) DO UPDATE SET title = EXCLUDED.title, starts_at = EXCLUDED.starts_at, "
        "allow_reentry = EXCLUDED.allow_reentry"));
    q.bindValue(QStringLiteral(":t"), sql::text(session.title()));
    q.bindValue(QStringLiteral(":s"), session.startsAt().isValid() ? QVariant(session.startsAt())
                                                                  : QVariant(QVariant::DateTime));
    q.bindValue(QStringLiteral(":r"), session.allowReentry());
    m_db.exec(q);
}

bool SessionRepository::hasAdmin() const
{
    return m_db.scalar(QStringLiteral("SELECT EXISTS (SELECT 1 FROM app_admin WHERE id = 1)")).toBool();
}

AdminAccount SessionRepository::admin() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT password_hash, failed_attempts, locked_until FROM app_admin WHERE id = 1"));
    m_db.exec(q);
    AdminAccount a;
    if (q.next()) {
        a.setSecretHash(q.value(0).toString().trimmed());
        a.setLockState(q.value(1).toInt(), q.value(2).toDateTime());
    }
    return a;
}

void SessionRepository::setAdminPassword(const QString& passwordHash) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO app_admin (id, password_hash) VALUES (1, :h) "
        "ON CONFLICT (id) DO UPDATE SET password_hash = EXCLUDED.password_hash, "
        "failed_attempts = 0, locked_until = NULL"));
    q.bindValue(QStringLiteral(":h"), passwordHash);
    m_db.exec(q);
}

void SessionRepository::saveAdminLockState(const AdminAccount& admin) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE app_admin SET failed_attempts = :a, locked_until = :u WHERE id = 1"));
    q.bindValue(QStringLiteral(":a"), admin.failedAttempts());
    q.bindValue(QStringLiteral(":u"), admin.lockedUntil().isValid() ? QVariant(admin.lockedUntil())
                                                                   : QVariant(QVariant::DateTime));
    m_db.exec(q);
}

void SessionRepository::clearAllData() const
{
    Transaction tx(m_db);
    m_db.execScript(QStringLiteral(
        "TRUNCATE basket_progress, score_draft, evaluation, taster, sample, basket, team, "
        "participant, nomination, audit_log RESTART IDENTITY CASCADE;"
        "ALTER SEQUENCE sample_seq_number RESTART WITH 1;"
        "UPDATE tasting_session SET title = 'Дегустация мёда', starts_at = NULL, "
        "allow_reentry = false WHERE id = 1;"));
    tx.commit();
}

}  // namespace sedm
