/**
 * @file ReportRepository.cpp
 * @brief Реализация сводных запросов.
 */
#include "ReportRepository.h"
#include "Database.h"
#include "SqlUtil.h"

#include <QHash>
#include <QStringList>
#include <QVariant>

namespace sedm {

namespace {
QString shortName(const QString& fullName)
{
    const QStringList parts = fullName.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QString();
    QString r = parts.first();
    for (int i = 1; i < parts.size() && i < 3; ++i)
        r += QStringLiteral(" ") + parts[i].left(1) + QStringLiteral(".");
    return r;
}
}  // namespace

int ProjectSummary::stagesDone() const
{
    int done = 0;
    done += participants > 0 ? 1 : 0;                                 // 1. реестр участников
    done += samples > 0 ? 1 : 0;                                      // 2. образцы зарегистрированы
    done += (samples > 0 && unassignedSamples == 0) ? 1 : 0;          // 3. распределены по корзинам
    done += (teams > 0 && tasters > 0) ? 1 : 0;                       // 4. команды и дегустаторы
    done += (baskets > 0 && unassignedBaskets == 0) ? 1 : 0;          // 5. корзины назначены командам
    done += sessionDateSet ? 1 : 0;                                   // 6. дата и время сеанса
    return done;
}

ProjectSummary ReportRepository::summary() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT (SELECT count(*) FROM participant), (SELECT count(*) FROM nomination), "
        "       (SELECT count(*) FROM sample), (SELECT count(*) FROM sample WHERE basket_id IS NULL), "
        "       (SELECT count(*) FROM basket), (SELECT count(*) FROM basket WHERE team_id IS NULL), "
        "       (SELECT count(*) FROM team), (SELECT count(*) FROM taster), "
        "       (SELECT count(*) FROM evaluation), (SELECT count(*) FROM audit_log), "
        "       (SELECT starts_at IS NOT NULL FROM tasting_session WHERE id = 1), "
        "       (SELECT title FROM tasting_session WHERE id = 1)"));
    m_db.exec(q);
    ProjectSummary s;
    if (q.next()) {
        s.participants = q.value(0).toInt();
        s.nominations = q.value(1).toInt();
        s.samples = q.value(2).toInt();
        s.unassignedSamples = q.value(3).toInt();
        s.baskets = q.value(4).toInt();
        s.unassignedBaskets = q.value(5).toInt();
        s.teams = q.value(6).toInt();
        s.tasters = q.value(7).toInt();
        s.evaluations = q.value(8).toInt();
        s.auditEntries = q.value(9).toInt();
        s.sessionDateSet = q.value(10).toBool();
        s.sessionTitle = q.value(11).toString();
    }
    return s;
}

QVector<TasterProgressRow> ReportRepository::tasterProgress() const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT t.id, t.full_name, m.number, m.name, "
        "       (SELECT count(*) FROM sample s JOIN basket b ON b.id = s.basket_id WHERE b.team_id = m.id), "
        "       (SELECT count(*) FROM evaluation e JOIN sample s ON s.id = e.sample_id "
        "          JOIN basket b ON b.id = s.basket_id "
        "         WHERE e.taster_id = t.id AND b.team_id = m.id AND e.appearance IS NOT NULL "
        "           AND e.consistency IS NOT NULL AND e.aroma IS NOT NULL AND e.taste IS NOT NULL), "
        "       (SELECT count(*) FROM basket b WHERE b.team_id = m.id), "
        "       (SELECT count(*) FROM basket_progress p JOIN basket b ON b.id = p.basket_id "
        "         WHERE p.taster_id = t.id AND b.team_id = m.id), "
        "       (SELECT max(e.saved_at) FROM evaluation e WHERE e.taster_id = t.id) "
        "  FROM taster t JOIN team m ON m.id = t.team_id "
        " ORDER BY m.number, lower(t.full_name)"));
    m_db.exec(q);
    QVector<TasterProgressRow> rows;
    while (q.next()) {
        TasterProgressRow r;
        r.tasterId = q.value(0).toInt();
        r.tasterName = shortName(q.value(1).toString());
        r.teamNumber = q.value(2).toInt();
        r.teamName = q.value(3).toString();
        r.samplesAssigned = q.value(4).toInt();
        r.samplesEvaluated = q.value(5).toInt();
        r.basketsAssigned = q.value(6).toInt();
        r.basketsFinished = q.value(7).toInt();
        r.lastSaved = q.value(8).toDateTime();
        rows.append(r);
    }
    return rows;
}

QVector<TasterBasketRow> ReportRepository::basketsForTaster(int tasterId) const
{
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT b.id, b.number, b.nomination_id, b.desired_count, b.team_id, n.name, "
        "       coalesce(array_to_string(array_agg(s.id ORDER BY s.seq_number) "
        "                FILTER (WHERE s.id IS NOT NULL), ','), ''), "
        "       count(e.sample_id) FILTER (WHERE e.appearance IS NOT NULL AND e.consistency IS NOT NULL "
        "                                  AND e.aroma IS NOT NULL AND e.taste IS NOT NULL), "
        "       EXISTS (SELECT 1 FROM basket_progress p WHERE p.taster_id = t.id AND p.basket_id = b.id) "
        "  FROM taster t "
        "  JOIN basket b ON b.team_id = t.team_id "
        "  JOIN nomination n ON n.id = b.nomination_id "
        "  LEFT JOIN sample s ON s.basket_id = b.id "
        "  LEFT JOIN evaluation e ON e.sample_id = s.id AND e.taster_id = t.id "
        " WHERE t.id = :t "
        " GROUP BY t.id, b.id, n.name ORDER BY b.number"));
    q.bindValue(QStringLiteral(":t"), tasterId);
    m_db.exec(q);
    QVector<TasterBasketRow> rows;
    while (q.next()) {
        TasterBasketRow r;
        r.basket.setId(q.value(0).toInt());
        r.basket.setNumber(q.value(1).toInt());
        r.basket.setNominationId(q.value(2).toInt());
        QVector<int> ids;
        const QString list = q.value(6).toString();
        if (!list.isEmpty())
            for (const QString& part : list.split(QLatin1Char(',')))
                ids.append(part.toInt());
        r.basket.setSampleIds(ids);
        r.basket.setDesiredCount(qMax(q.value(3).toInt(), ids.size()));
        r.basket.setTeamId(sql::optInt(q.value(4)));
        r.nominationName = q.value(5).toString();
        r.evaluated = q.value(7).toInt();
        r.finished = q.value(8).toBool();
        rows.append(r);
    }
    return rows;
}

QVector<SampleResult> ReportRepository::results() const
{
    // 1) образцы с владельцами и номинациями
    QSqlQuery q = m_db.prepare(QStringLiteral(
        "SELECT s.id, s.seq_number, s.cipher, s.participant_id, s.nomination_id, s.basket_id, "
        "       p.full_name, p.phone, p.email, p.region, n.name "
        "  FROM sample s JOIN participant p ON p.id = s.participant_id "
        "  JOIN nomination n ON n.id = s.nomination_id ORDER BY lower(n.name), s.seq_number"));
    m_db.exec(q);
    QVector<SampleResult> results;
    QHash<int, int> indexById;
    while (q.next()) {
        Sample s;
        s.setId(q.value(0).toInt());
        s.setSeqNumber(q.value(1).toInt());
        s.setCipher(q.value(2).toString().trimmed());
        s.setParticipantId(q.value(3).toInt());
        s.setNominationId(q.value(4).toInt());
        s.setBasketId(sql::optInt(q.value(5)));
        Participant p;
        p.setId(s.participantId());
        p.setFullName(q.value(6).toString());
        p.setPhone(q.value(7).toString());
        p.setEmail(q.value(8).toString());
        p.setRegion(q.value(9).toString());
        p.setNominationId(s.nominationId());
        indexById.insert(s.id(), results.size());
        results.append(SampleResult(s, p, q.value(10).toString()));
    }

    // 2) оценки всех дегустаторов
    QSqlQuery e = m_db.prepare(QStringLiteral(
        "SELECT e.sample_id, e.taster_id, t.full_name, m.number, "
        "       e.appearance, e.consistency, e.aroma, e.taste, e.note, e.saved_at "
        "  FROM evaluation e JOIN taster t ON t.id = e.taster_id JOIN team m ON m.id = t.team_id "
        " ORDER BY m.number, lower(t.full_name)"));
    m_db.exec(e);
    while (e.next()) {
        const int idx = indexById.value(e.value(0).toInt(), -1);
        if (idx < 0)
            continue;
        TasterScore ts;
        ts.tasterId = e.value(1).toInt();
        ts.tasterName = shortName(e.value(2).toString());
        ts.teamNumber = e.value(3).toInt();
        ts.evaluation = Evaluation(ts.tasterId, e.value(0).toInt());
        for (int i = 0; i < kCriteriaCount; ++i) {
            const QVariant v = e.value(4 + i);
            if (!v.isNull())
                ts.evaluation.setScore(kAllCriteria[static_cast<size_t>(i)], v.toInt());
        }
        ts.evaluation.setNote(e.value(8).toString());
        ts.evaluation.setSavedAt(e.value(9).toDateTime());
        results[idx].addScore(ts);
    }
    return results;
}

}  // namespace sedm
