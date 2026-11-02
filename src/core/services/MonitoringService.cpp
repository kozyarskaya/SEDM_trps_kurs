/**
 * @file MonitoringService.cpp
 * @brief Реализация мониторинга хода дегустации.
 */
#include "MonitoringService.h"
#include "BasketRepository.h"
#include "SampleRepository.h"
#include "TeamRepository.h"

#include <QHash>

namespace sedm {

int TeamProgress::evaluatedTotal() const
{
    int n = 0;
    for (const TasterProgressRow& t : tasters)
        n += t.samplesEvaluated;
    return n;
}

int TeamProgress::expectedTotal() const
{
    return sampleCount * tasters.size();
}

bool TeamProgress::isFinished() const
{
    if (tasters.isEmpty() || basketCount == 0)
        return false;
    for (const TasterProgressRow& t : tasters)
        if (t.basketsFinished < basketCount)
            return false;
    return true;
}

int MonitoringSnapshot::evaluatedTotal() const
{
    int n = 0;
    for (const TeamProgress& t : teams)
        n += t.evaluatedTotal();
    return n;
}

int MonitoringSnapshot::expectedTotal() const
{
    int n = 0;
    for (const TeamProgress& t : teams)
        n += t.expectedTotal();
    return n;
}

ProjectSummary MonitoringService::summary() const
{
    return ReportRepository(m_db).summary();
}

MonitoringSnapshot MonitoringService::snapshot() const
{
    MonitoringSnapshot s;
    s.takenAt = QDateTime::currentDateTime();

    QHash<int, int> indexByTeam;   // id команды -> позиция в s.teams
    for (const Team& t : TeamRepository(m_db).list()) {
        TeamProgress p;
        p.team = t;
        indexByTeam.insert(t.id(), s.teams.size());
        s.teams.append(p);
    }
    for (const Basket& b : BasketRepository(m_db).list()) {
        ++s.basketsTotal;
        s.samplesInBaskets += b.sampleCount();
        if (!b.teamId()) {
            ++s.basketsUnassigned;
            continue;
        }
        const int idx = indexByTeam.value(*b.teamId(), -1);
        if (idx >= 0) {
            ++s.teams[idx].basketCount;
            s.teams[idx].sampleCount += b.sampleCount();
        }
    }
    QHash<int, int> indexByNumber;
    for (int i = 0; i < s.teams.size(); ++i)
        indexByNumber.insert(s.teams[i].team.number(), i);
    for (const TasterProgressRow& row : ReportRepository(m_db).tasterProgress()) {
        const int idx = indexByNumber.value(row.teamNumber, -1);
        if (idx >= 0)
            s.teams[idx].tasters.append(row);
    }
    s.samplesTotal = SampleRepository(m_db).count();
    return s;
}

}  // namespace sedm
