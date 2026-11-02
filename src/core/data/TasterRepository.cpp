/**
 * @file TasterRepository.cpp
 * @brief Реализация хранения дегустаторов.
 */
#include "TasterRepository.h"
#include "Database.h"
#include "LoginGenerator.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
const QString kColumns = QStringLiteral(
    "id, team_id, full_name, login, pin_hash, failed_attempts, locked_until");

Taster fromQuery(const QSqlQuery& q)
{
    Taster t;
    t.setId(q.value(0).toInt());
    t.setTeamId(q.value(1).toInt());
    t.setFullName(q.value(2).toString());
    t.setLogin(q.value(3).toString());
    t.setSecretHash(q.value(4).toString().trimmed());
    t.setLockState(q.value(5).toInt(), q.value(6).toDateTime());
    return t;
}

QVector<Taster> readAll(QSqlQuery& q)
{
    QVector<Taster> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}
}  // namespace

QVector<Taster> TasterRepository::list() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT t.id, t.team_id, t.full_name, t.login, t.pin_hash, t.failed_attempts, t.locked_until "
        "  FROM taster t JOIN team m ON m.id = t.team_id ORDER BY m.number, lower(t.full_name)"));
    m_db.exec(q);
    return readAll(q);
}

QVector<Taster> TasterRepository::forTeam(int teamId) const
{
    QSqlQuery q = m_db.prepare(
        QStringLiteral("SELECT %1 FROM taster WHERE team_id = :t ORDER BY lower(full_name)").arg(kColumns));
    q.bindValue(QStringLiteral(":t"), teamId);
    m_db.exec(q);
    return readAll(q);
}

std::optional<Taster> TasterRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM taster WHERE id = :id").arg(kColumns));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

std::optional<Taster> TasterRepository::byLogin(const QString& login) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM taster WHERE login_key = :k").arg(kColumns));
    q.bindValue(QStringLiteral(":k"), LoginGenerator::normalizedForLookup(login));
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

QSet<QString> TasterRepository::existingLogins() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT login FROM taster"));
    m_db.exec(q);
    QSet<QString> result;
    while (q.next())
        result.insert(q.value(0).toString());
    return result;
}

void TasterRepository::insert(Taster& taster) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO taster (team_id, full_name, login, login_key, pin_hash) "
        "VALUES (:team_id, :full_name, :login, :login_key, :pin_hash) RETURNING id"));
    q.bindValue(QStringLiteral(":team_id"), taster.teamId());
    q.bindValue(QStringLiteral(":full_name"), sql::text(taster.fullName()));
    q.bindValue(QStringLiteral(":login"), taster.login());
    q.bindValue(QStringLiteral(":login_key"), LoginGenerator::normalizedForLookup(taster.login()));
    q.bindValue(QStringLiteral(":pin_hash"), taster.secretHash());
    m_db.exec(q);
    q.next();
    taster.setId(q.value(0).toInt());
}

void TasterRepository::updateName(int id, const QString& fullName) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE taster SET full_name = :n WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), sql::text(fullName.simplified()));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

void TasterRepository::updatePin(int id, const QString& pinHash) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE taster SET pin_hash = :h, failed_attempts = 0, locked_until = NULL, "
        "session_token = NULL WHERE id = :id"));
    q.bindValue(QStringLiteral(":h"), pinHash);
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

void TasterRepository::saveLockState(const Taster& taster) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE taster SET failed_attempts = :a, locked_until = :u WHERE id = :id"));
    q.bindValue(QStringLiteral(":a"), taster.failedAttempts());
    q.bindValue(QStringLiteral(":u"), taster.lockedUntil().isValid() ? QVariant(taster.lockedUntil())
                                                                    : QVariant(QVariant::DateTime));
    q.bindValue(QStringLiteral(":id"), taster.id());
    m_db.exec(q);
}

void TasterRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM taster WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

void TasterRepository::setSessionToken(int id, const QString& token) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE taster SET session_token = CAST(:tok AS uuid) WHERE id = :id"));
    q.bindValue(QStringLiteral(":tok"), token.isEmpty() ? QVariant(QVariant::String) : QVariant(token));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

QString TasterRepository::sessionToken(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT coalesce(session_token::text, '') FROM taster WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    return q.next() ? q.value(0).toString() : QString();
}

bool TasterRepository::hasEvaluations(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT EXISTS (SELECT 1 FROM evaluation WHERE taster_id = :t)"));
    q.bindValue(QStringLiteral(":t"), id);
    m_db.exec(q);
    q.next();
    return q.value(0).toBool();
}

int TasterRepository::countWithEvaluations(int teamId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT count(DISTINCT t.id) FROM taster t JOIN evaluation e ON e.taster_id = t.id "
        "WHERE t.team_id = :t"));
    q.bindValue(QStringLiteral(":t"), teamId);
    m_db.exec(q);
    q.next();
    return q.value(0).toInt();
}

}  // namespace sedm
