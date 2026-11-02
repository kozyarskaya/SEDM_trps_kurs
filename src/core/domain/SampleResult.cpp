/**
 * @file SampleResult.cpp
 * @brief Расчёт среднего балла образца.
 *
 * Итоговый балл дегустатора t = S_t / 4, где S_t — сумма четырёх оценок.
 * Средний балл образца по k полным оценкам: A = (S_1 + … + S_k) / (4k).
 * Для исключения погрешностей двоичной арифметики A хранится в сотых долях:
 * A_100 = round(25·ΣS / k) — вычисляется в целых числах с округлением «половина вверх».
 */
#include "SampleResult.h"

namespace sedm {

SampleResult::SampleResult(const Sample& sample, const Participant& owner,
                           const QString& nominationName)
    : m_sample(sample), m_owner(owner), m_nominationName(nominationName)
{
}

int SampleResult::completeCount() const
{
    int count = 0;
    for (const TasterScore& s : m_scores)
        if (s.evaluation.isComplete())
            ++count;
    return count;
}

qint64 SampleResult::sumTimesFour() const
{
    qint64 sum = 0;
    for (const TasterScore& s : m_scores)
        if (const auto t = s.evaluation.totalTimesFour())
            sum += *t;
    return sum;
}

std::optional<qint64> SampleResult::averageCents() const
{
    const int k = completeCount();
    if (k == 0)
        return std::nullopt;
    // A·100 = ΣS·100 / (4k) = 25·ΣS / k; округление половины вверх: (2·25·ΣS + k) / (2k)
    const qint64 numerator = 50 * sumTimesFour() + k;
    return numerator / (2LL * k);
}

std::optional<double> SampleResult::average() const
{
    const auto cents = averageCents();
    if (!cents)
        return std::nullopt;
    return *cents / 100.0;
}

}  // namespace sedm
