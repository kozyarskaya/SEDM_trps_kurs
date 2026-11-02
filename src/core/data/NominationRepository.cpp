/**
 * @file NominationRepository.cpp
 * @brief Реализация хранения номинаций.
 */
#include "NominationRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
Nomination fromQuery(const QSqlQuery& q)
{
    Nomination n;
    n.setId(q.value(0).toInt());
    n.setName(q.value(1).toString());
    return n;
}
}  // namespace

QVector<Nomination> NominationRepository::list() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT id, name FROM nomination ORDER BY lower(name)"));
    m_db.exec(q);
    QVector<Nomination> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}

std::optional<Nomination> NominationRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT id, name FROM nomination WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

std::optional<Nomination> NominationRepository::byName(const QString& name) const
{
    QSqlQuery q = m_db.prepare(
        QStringLiteral("SELECT id, name FROM nomination WHERE lower(name) = lower(:name)"));
    q.bindValue(QStringLiteral(":name"), Nomination::normalizedName(name));
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

void NominationRepository::insert(Nomination& nomination) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("INSERT INTO nomination (name) VALUES (:name) RETURNING id"));
    q.bindValue(QStringLiteral(":name"), sql::text(nomination.name()));
    m_db.exec(q);
    q.next();
    nomination.setId(q.value(0).toInt());
}

void NominationRepository::update(const Nomination& nomination) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE nomination SET name = :name WHERE id = :id"));
    q.bindValue(QStringLiteral(":name"), sql::text(nomination.name()));
    q.bindValue(QStringLiteral(":id"), nomination.id());
    m_db.exec(q);
}

void NominationRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM nomination WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

int NominationRepository::usageCount(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "WITH n AS (SELECT CAST(:id AS integer) AS id) "
        "SELECT (SELECT count(*) FROM participant p, n WHERE p.nomination_id = n.id)"
        "     + (SELECT count(*) FROM sample s, n WHERE s.nomination_id = n.id)"
        "     + (SELECT count(*) FROM basket b, n WHERE b.nomination_id = n.id)"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    q.next();
    return q.value(0).toInt();
}

}  // namespace sedm
