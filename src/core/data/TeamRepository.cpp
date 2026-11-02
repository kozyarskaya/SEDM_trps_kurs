/**
 * @file TeamRepository.cpp
 * @brief Реализация хранения команд.
 */
#include "TeamRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
Team fromQuery(const QSqlQuery& q)
{
    Team t;
    t.setId(q.value(0).toInt());
    t.setNumber(q.value(1).toInt());
    t.setName(q.value(2).toString());
    return t;
}
}  // namespace

QVector<Team> TeamRepository::list() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT id, number, name FROM team ORDER BY number"));
    m_db.exec(q);
    QVector<Team> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}

std::optional<Team> TeamRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT id, number, name FROM team WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

void TeamRepository::insert(Team& team) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("INSERT INTO team (number, name) VALUES (:number, :name) RETURNING id"));
    q.bindValue(QStringLiteral(":number"), team.number());
    q.bindValue(QStringLiteral(":name"), sql::text(team.name()));
    m_db.exec(q);
    q.next();
    team.setId(q.value(0).toInt());
}

void TeamRepository::update(const Team& team) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE team SET number = :number, name = :name WHERE id = :id"));
    q.bindValue(QStringLiteral(":number"), team.number());
    q.bindValue(QStringLiteral(":name"), sql::text(team.name()));
    q.bindValue(QStringLiteral(":id"), team.id());
    m_db.exec(q);
}

void TeamRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM team WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

int TeamRepository::nextNumber() const
{
    return m_db.scalar(QStringLiteral("SELECT coalesce(max(number), 0) + 1 FROM team")).toInt();
}

}  // namespace sedm
