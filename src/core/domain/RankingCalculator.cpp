/**
 * @file RankingCalculator.cpp
 * @brief Алгоритм «плотной» расстановки мест.
 */
#include "RankingCalculator.h"

#include <QHash>
#include <QSet>
#include <algorithm>

namespace sedm {

void RankingCalculator::assignPlaces(QVector<SampleResult>& results)
{
    // 1. Сортировка: сначала образцы со средним баллом (по убыванию), затем без
    //    балла; при равенстве — по порядковому номеру регистрации.
    std::stable_sort(results.begin(), results.end(),
                     [](const SampleResult& a, const SampleResult& b) {
                         const auto ca = a.averageCents();
                         const auto cb = b.averageCents();
                         if (ca.has_value() != cb.has_value())
                             return ca.has_value();
                         if (ca && *ca != *cb)
                             return *ca > *cb;
                         return a.sample().seqNumber() < b.sample().seqNumber();
                     });

    // 2. Плотная нумерация: место растёт на 1 только при смене балла.
    int place = 0;
    std::optional<qint64> previous;
    for (SampleResult& r : results) {
        const auto cents = r.averageCents();
        if (!cents) {
            r.setPlace(0);
            continue;
        }
        if (!previous || *cents != *previous)
            ++place;
        r.setPlace(place);
        previous = cents;
    }
}

QVector<NominationResult> RankingCalculator::rankByNomination(const QVector<SampleResult>& all)
{
    QVector<NominationResult> groups;
    QHash<int, int> indexById;   // номинация -> позиция в groups
    for (const SampleResult& r : all) {
        const int id = r.sample().nominationId();
        auto it = indexById.constFind(id);
        if (it == indexById.constEnd()) {
            NominationResult g;
            g.nominationId = id;
            g.name = r.nominationName();
            it = indexById.insert(id, groups.size());
            groups.append(g);
        }
        groups[it.value()].results.append(r);
    }
    for (NominationResult& g : groups) {
        assignPlaces(g.results);
        QSet<int> seen;
        for (const SampleResult& r : g.results)
            for (const TasterScore& s : r.scores())
                if (!seen.contains(s.tasterId)) {
                    seen.insert(s.tasterId);
                    g.tasters.append({s.tasterId, s.tasterName, s.teamNumber});
                }
        std::stable_sort(g.tasters.begin(), g.tasters.end(), [](const TasterColumn& a, const TasterColumn& b) {
            if (a.teamNumber != b.teamNumber)
                return a.teamNumber < b.teamNumber;
            return a.name.localeAwareCompare(b.name) < 0;
        });
    }
    return groups;
}

}  // namespace sedm
