/**
 * @file EvaluationRepository.cpp
 * @brief Реализация хранения оценок, черновиков и завершения корзин.
 */
#include "EvaluationRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QVariant>

namespace sedm {

namespace {
Evaluation fromQuery(const QSqlQuery& q)
{
    Evaluation e(q.value(0).toInt(), q.value(1).toInt());
    for (int i = 0; i < kCriteriaCount; ++i) {
        const QVariant v = q.value(2 + i);
        if (!v.isNull())
            e.setScore(kAllCriteria[static_cast<size_t>(i)], v.toInt());
    }
    e.setNote(q.value(6).toString());
    e.setSavedAt(q.value(7).toDateTime());
    return e;
}
}  // namespace

QVector<Evaluation> EvaluationRepository::load(const QString& table, int tasterId, int basketId) const
{
    QString sql = QStringLiteral(
        "SELECT e.taster_id, e.sample_id, e.appearance, e.consistency, e.aroma, e.taste, e.note, e.saved_at "
        "  FROM %1 e JOIN sample s ON s.id = e.sample_id WHERE e.taster_id = :t").arg(table);
    if (basketId > 0)
        sql += QStringLiteral(" AND s.basket_id = :b");
    sql += QStringLiteral(" ORDER BY s.seq_number");
    QSqlQuery q = m_db.prepare(sql);
    q.bindValue(QStringLiteral(":t"), tasterId);
    if (basketId > 0)
        q.bindValue(QStringLiteral(":b"), basketId);
    m_db.exec(q);
    QVector<Evaluation> result;
    while (q.next())
        result.append(fromQuery(q));
    return result;
}

void EvaluationRepository::store(const QString& table, const QVector<Evaluation>& rows) const
{
    QString sql = QStringLiteral(
        "INSERT INTO %1 (taster_id, sample_id, appearance, consistency, aroma, taste, note, saved_at) "
        "VALUES (:t, :s, :a, :c, :r, :k, :note, now()) "
        "ON CONFLICT (taster_id, sample_id) DO UPDATE SET appearance = EXCLUDED.appearance, "
        "consistency = EXCLUDED.consistency, aroma = EXCLUDED.aroma, taste = EXCLUDED.taste, "
        "note = EXCLUDED.note, saved_at = now()").arg(table);
    // параметры критериев в порядке kAllCriteria: вид, консистенция, аромат, вкус
    const QString params[kCriteriaCount] = {QStringLiteral(":a"), QStringLiteral(":c"),
                                            QStringLiteral(":r"), QStringLiteral(":k")};
    for (const Evaluation& e : rows) {
        QSqlQuery q = m_db.prepare(sql);
        q.bindValue(QStringLiteral(":t"), e.tasterId());
        q.bindValue(QStringLiteral(":s"), e.sampleId());
        for (int i = 0; i < kCriteriaCount; ++i)
            q.bindValue(params[i], sql::toVariant(e.score(kAllCriteria[static_cast<size_t>(i)])));
        q.bindValue(QStringLiteral(":note"), sql::text(e.note()));
        m_db.exec(q);
    }
}

QVector<Evaluation> EvaluationRepository::forTaster(int tasterId) const
{
    return load(QStringLiteral("evaluation"), tasterId, 0);
}

QVector<Evaluation> EvaluationRepository::forTasterBasket(int tasterId, int basketId) const
{
    return load(QStringLiteral("evaluation"), tasterId, basketId);
}

void EvaluationRepository::save(const QVector<Evaluation>& evaluations) const
{
    store(QStringLiteral("evaluation"), evaluations);
}

QVector<Evaluation> EvaluationRepository::drafts(int tasterId, int basketId) const
{
    return load(QStringLiteral("score_draft"), tasterId, basketId);
}

void EvaluationRepository::saveDrafts(const QVector<Evaluation>& drafts) const
{
    store(QStringLiteral("score_draft"), drafts);
}

void EvaluationRepository::deleteDrafts(int tasterId, int basketId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "DELETE FROM score_draft d USING sample s WHERE s.id = d.sample_id "
        "AND d.taster_id = :t AND s.basket_id = :b"));
    q.bindValue(QStringLiteral(":t"), tasterId);
    q.bindValue(QStringLiteral(":b"), basketId);
    m_db.exec(q);
}

QSet<int> EvaluationRepository::finishedBaskets(int tasterId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral("SELECT basket_id FROM basket_progress WHERE taster_id = :t"));
    q.bindValue(QStringLiteral(":t"), tasterId);
    m_db.exec(q);
    QSet<int> result;
    while (q.next())
        result.insert(q.value(0).toInt());
    return result;
}

void EvaluationRepository::markFinished(int tasterId, int basketId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "INSERT INTO basket_progress (taster_id, basket_id, finished_at) VALUES (:t, :b, now())"));
    q.bindValue(QStringLiteral(":t"), tasterId);
    q.bindValue(QStringLiteral(":b"), basketId);
    m_db.exec(q);
}

int EvaluationRepository::count() const
{
    return m_db.scalar(QStringLiteral("SELECT count(*) FROM evaluation")).toInt();
}

}  // namespace sedm
