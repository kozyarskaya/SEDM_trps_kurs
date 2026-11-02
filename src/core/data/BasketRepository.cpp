/**
 * @file BasketRepository.cpp
 * @brief Реализация хранения корзин.
 */
#include "BasketRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
// Состав корзины — массив идентификаторов образцов в порядке регистрации
const QString kSelect = QStringLiteral(
    "SELECT b.id, b.number, b.nomination_id, b.desired_count, b.team_id, "
    "       coalesce(array_to_string(array_agg(s.id ORDER BY s.seq_number) "
    "                FILTER (WHERE s.id IS NOT NULL), ','), '') "
    "  FROM basket b LEFT JOIN sample s ON s.basket_id = b.id ");
const QString kGroup = QStringLiteral(" GROUP BY b.id ORDER BY b.number");

Basket fromQuery(const QSqlQuery& q)
{
    Basket b;
    b.setId(q.value(0).toInt());
    b.setNumber(q.value(1).toInt());
    b.setNominationId(q.value(2).toInt());
    QVector<int> ids;
    const QString list = q.value(5).toString();
    if (!list.isEmpty())
        for (const QString& part : list.split(QLatin1Char(',')))
            ids.append(part.toInt());
    b.setSampleIds(ids);
    b.setDesiredCount(qMax(q.value(3).toInt(), ids.size()));
    b.setTeamId(sql::optInt(q.value(4)));
    return b;
}

QVector<Basket> readAll(QSqlQuery& q)
{
    QVector<Basket> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}
}  // namespace

QVector<Basket> BasketRepository::list() const
{
    QSqlQuery q = m_db.prepare(kSelect + kGroup);
    m_db.exec(q);
    return readAll(q);
}

std::optional<Basket> BasketRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(kSelect + QStringLiteral(" WHERE b.id = :id") + kGroup);
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

QVector<Basket> BasketRepository::forTeam(int teamId) const
{
    QSqlQuery q = m_db.prepare(kSelect + QStringLiteral(" WHERE b.team_id = :t") + kGroup);
    q.bindValue(QStringLiteral(":t"), teamId);
    m_db.exec(q);
    return readAll(q);
}

void BasketRepository::insert(Basket& basket) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO basket (number, nomination_id, desired_count, team_id) "
        "VALUES (:number, :nomination_id, :desired_count, :team_id) RETURNING id"));
    q.bindValue(QStringLiteral(":number"), basket.number());
    q.bindValue(QStringLiteral(":nomination_id"), basket.nominationId());
    q.bindValue(QStringLiteral(":desired_count"), basket.desiredCount());
    q.bindValue(QStringLiteral(":team_id"), sql::toVariant(basket.teamId()));
    m_db.exec(q);
    q.next();
    basket.setId(q.value(0).toInt());
}

void BasketRepository::update(const Basket& basket) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE basket SET number = :number, nomination_id = :nomination_id, "
        "desired_count = :desired_count, team_id = :team_id WHERE id = :id"));
    q.bindValue(QStringLiteral(":number"), basket.number());
    q.bindValue(QStringLiteral(":nomination_id"), basket.nominationId());
    q.bindValue(QStringLiteral(":desired_count"), basket.desiredCount());
    q.bindValue(QStringLiteral(":team_id"), sql::toVariant(basket.teamId()));
    q.bindValue(QStringLiteral(":id"), basket.id());
    m_db.exec(q);
}

void BasketRepository::assignTeam(int basketId, std::optional<int> teamId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE basket SET team_id = :team_id WHERE id = :id"));
    q.bindValue(QStringLiteral(":team_id"), sql::toVariant(teamId));
    q.bindValue(QStringLiteral(":id"), basketId);
    m_db.exec(q);
}

void BasketRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM basket WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

int BasketRepository::nextNumber() const
{
    return m_db.scalar(QStringLiteral("SELECT coalesce(max(number), 0) + 1 FROM basket")).toInt();
}

bool BasketRepository::hasEvaluations(int basketId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT EXISTS (SELECT 1 FROM evaluation e JOIN sample s ON s.id = e.sample_id "
        "WHERE s.basket_id = :b)"));
    q.bindValue(QStringLiteral(":b"), basketId);
    m_db.exec(q);
    q.next();
    return q.value(0).toBool();
}

}  // namespace sedm
