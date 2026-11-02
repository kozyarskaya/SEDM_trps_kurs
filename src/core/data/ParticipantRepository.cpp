/**
 * @file ParticipantRepository.cpp
 * @brief Реализация хранения реестра участников.
 */
#include "ParticipantRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
const QString kColumns = QStringLiteral("id, full_name, phone, email, region, nomination_id");

Participant fromQuery(const QSqlQuery& q)
{
    Participant p;
    p.setId(q.value(0).toInt());
    p.setFullName(q.value(1).toString());
    p.setPhone(q.value(2).toString());
    p.setEmail(q.value(3).toString());
    p.setRegion(q.value(4).toString());
    p.setNominationId(q.value(5).toInt());
    return p;
}

void bindFields(QSqlQuery& q, const Participant& p)
{
    q.bindValue(QStringLiteral(":full_name"), sql::text(p.fullName()));
    q.bindValue(QStringLiteral(":phone"), sql::text(p.phone()));
    q.bindValue(QStringLiteral(":email"), sql::text(p.email()));
    q.bindValue(QStringLiteral(":region"), sql::text(p.region()));
    q.bindValue(QStringLiteral(":nomination_id"), p.nominationId());
}
}  // namespace

QVector<Participant> ParticipantRepository::list() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM participant ORDER BY lower(full_name), id")
                                   .arg(kColumns));
    m_db.exec(q);
    QVector<Participant> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}

std::optional<Participant> ParticipantRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM participant WHERE id = :id").arg(kColumns));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

void ParticipantRepository::insert(Participant& participant) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO participant (full_name, phone, email, region, nomination_id) "
        "VALUES (:full_name, :phone, :email, :region, :nomination_id) RETURNING id"));
    bindFields(q, participant);
    m_db.exec(q);
    q.next();
    participant.setId(q.value(0).toInt());
}

void ParticipantRepository::update(const Participant& participant) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE participant SET full_name = :full_name, phone = :phone, email = :email, "
        "region = :region, nomination_id = :nomination_id WHERE id = :id"));
    bindFields(q, participant);
    q.bindValue(QStringLiteral(":id"), participant.id());
    m_db.exec(q);
}

void ParticipantRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM participant WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

int ParticipantRepository::count() const
{
    return m_db.scalar(QStringLiteral("SELECT count(*) FROM participant")).toInt();
}

int ParticipantRepository::sampleCount(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT count(*) FROM sample WHERE participant_id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    q.next();
    return q.value(0).toInt();
}

}  // namespace sedm
