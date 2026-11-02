/**
 * @file SampleRepository.cpp
 * @brief Реализация хранения образцов.
 */
#include "SampleRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
const QString kColumns = QStringLiteral("id, seq_number, cipher, participant_id, nomination_id, basket_id");

Sample fromQuery(const QSqlQuery& q)
{
    Sample s;
    s.setId(q.value(0).toInt());
    s.setSeqNumber(q.value(1).toInt());
    s.setCipher(q.value(2).toString().trimmed());
    s.setParticipantId(q.value(3).toInt());
    s.setNominationId(q.value(4).toInt());
    s.setBasketId(sql::optInt(q.value(5)));
    return s;
}

QVector<Sample> readAll(QSqlQuery& q)
{
    QVector<Sample> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}
}  // namespace

QVector<Sample> SampleRepository::list() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM sample ORDER BY seq_number").arg(kColumns));
    m_db.exec(q);
    return readAll(q);
}

std::optional<Sample> SampleRepository::byId(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT %1 FROM sample WHERE id = :id").arg(kColumns));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
    if (!q.next())
        return std::nullopt;
    return fromQuery(q);
}

QSet<QString> SampleRepository::usedCiphers() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT cipher FROM sample"));
    m_db.exec(q);
    QSet<QString> result;
    while (q.next())
        result.insert(q.value(0).toString().trimmed());
    return result;
}

void SampleRepository::insert(Sample& sample) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO sample (cipher, participant_id, nomination_id, basket_id) "
        "VALUES (:cipher, :participant_id, :nomination_id, :basket_id) RETURNING id, seq_number"));
    q.bindValue(QStringLiteral(":cipher"), sample.cipher());
    q.bindValue(QStringLiteral(":participant_id"), sample.participantId());
    q.bindValue(QStringLiteral(":nomination_id"), sample.nominationId());
    q.bindValue(QStringLiteral(":basket_id"), sql::toVariant(sample.basketId()));
    m_db.exec(q);
    q.next();
    sample.setId(q.value(0).toInt());
    sample.setSeqNumber(q.value(1).toInt());
}

void SampleRepository::update(const Sample& sample) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "UPDATE sample SET participant_id = :participant_id, nomination_id = :nomination_id "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":participant_id"), sample.participantId());
    q.bindValue(QStringLiteral(":nomination_id"), sample.nominationId());
    q.bindValue(QStringLiteral(":id"), sample.id());
    m_db.exec(q);
}

void SampleRepository::remove(int id) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("DELETE FROM sample WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    m_db.exec(q);
}

void SampleRepository::setBasket(int sampleId, std::optional<int> basketId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("UPDATE sample SET basket_id = :basket_id WHERE id = :id"));
    q.bindValue(QStringLiteral(":basket_id"), sql::toVariant(basketId));
    q.bindValue(QStringLiteral(":id"), sampleId);
    m_db.exec(q);
}

QVector<Sample> SampleRepository::available(int nominationId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT %1 FROM sample WHERE nomination_id = :n AND basket_id IS NULL ORDER BY seq_number")
                                   .arg(kColumns));
    q.bindValue(QStringLiteral(":n"), nominationId);
    m_db.exec(q);
    return readAll(q);
}

QVector<Sample> SampleRepository::inBasket(int basketId) const
{
    QSqlQuery q = m_db.prepare(
        QStringLiteral("SELECT %1 FROM sample WHERE basket_id = :b ORDER BY seq_number").arg(kColumns));
    q.bindValue(QStringLiteral(":b"), basketId);
    m_db.exec(q);
    return readAll(q);
}

bool SampleRepository::hasEvaluations(int sampleId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT EXISTS (SELECT 1 FROM evaluation WHERE sample_id = :id)"));
    q.bindValue(QStringLiteral(":id"), sampleId);
    m_db.exec(q);
    q.next();
    return q.value(0).toBool();
}

int SampleRepository::count() const
{
    return m_db.scalar(QStringLiteral("SELECT count(*) FROM sample")).toInt();
}

int SampleRepository::unassignedCount() const
{
    return m_db.scalar(QStringLiteral("SELECT count(*) FROM sample WHERE basket_id IS NULL")).toInt();
}

}  // namespace sedm
