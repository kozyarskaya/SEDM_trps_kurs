/**
 * @file BasketProgress.cpp
 * @brief Правило завершения дегустации корзины.
 */
#include "BasketProgress.h"
#include "Basket.h"
#include "DomainError.h"
#include "Evaluation.h"

namespace sedm {

BasketProgress::BasketProgress(int tasterId, int basketId)
    : m_tasterId(tasterId), m_basketId(basketId)
{
}

QVector<int> BasketProgress::incompleteSampleIds(const Basket& basket,
                                                 const QVector<Evaluation>& evaluations)
{
    QVector<int> missing;
    for (int sampleId : basket.sampleIds()) {
        bool complete = false;
        for (const Evaluation& e : evaluations) {
            if (e.sampleId() == sampleId) {
                complete = e.isComplete();
                break;
            }
        }
        if (!complete)
            missing.append(sampleId);
    }
    return missing;
}

void BasketProgress::finish(const Basket& basket, const QVector<Evaluation>& evaluations,
                            const QDateTime& now)
{
    if (isFinished())
        throw DomainError(QStringLiteral("Дегустация корзины № %1 уже завершена.").arg(basket.number()));
    if (basket.sampleIds().isEmpty())
        throw DomainError(QStringLiteral("В корзине № %1 нет образцов.").arg(basket.number()));
    const QVector<int> missing = incompleteSampleIds(basket, evaluations);
    if (!missing.isEmpty())
        throw DomainError(QStringLiteral("Нельзя завершить дегустацию: не оценены по всем четырём "
                                         "критериям образцов — %1.").arg(missing.size()));
    m_finishedAt = now;
}

}  // namespace sedm
