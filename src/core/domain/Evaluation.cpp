/**
 * @file Evaluation.cpp
 * @brief Реализация оценки образца и расчёта итогового балла.
 */
#include "Evaluation.h"
#include "DomainError.h"

namespace sedm {

Evaluation::Evaluation(int tasterId, int sampleId)
    : m_tasterId(tasterId), m_sampleId(sampleId)
{
}

std::optional<int> Evaluation::score(Criterion criterion) const
{
    return m_scores[static_cast<size_t>(criterion)];
}

void Evaluation::setScore(Criterion criterion, int value)
{
    if (!isValidScore(value))
        throw DomainError(QStringLiteral("Оценка «%1» должна быть целым числом от %2 до %3.")
                              .arg(criterionTitle(criterion)).arg(kMinScore).arg(kMaxScore));
    m_scores[static_cast<size_t>(criterion)] = value;
}

void Evaluation::clearScore(Criterion criterion)
{
    m_scores[static_cast<size_t>(criterion)].reset();
}

int Evaluation::ratedCount() const
{
    int count = 0;
    for (const auto& s : m_scores)
        if (s.has_value())
            ++count;
    return count;
}

bool Evaluation::isComplete() const
{
    return ratedCount() == kCriteriaCount;
}

bool Evaluation::isEmpty() const
{
    return ratedCount() == 0 && m_note.isEmpty();
}

std::optional<int> Evaluation::totalTimesFour() const
{
    if (!isComplete())
        return std::nullopt;
    int sum = 0;
    for (const auto& s : m_scores)
        sum += *s;
    return sum;
}

std::optional<double> Evaluation::total() const
{
    const auto sum = totalTimesFour();
    if (!sum)
        return std::nullopt;
    return *sum / static_cast<double>(kCriteriaCount);
}

void Evaluation::setNote(const QString& note)
{
    const QString trimmed = note.trimmed();
    if (trimmed.size() > kMaxNoteLength)
        throw DomainError(QStringLiteral("Примечание длиннее %1 символов.").arg(kMaxNoteLength));
    m_note = trimmed;
}

bool Evaluation::sameContentAs(const Evaluation& other) const
{
    return m_scores == other.m_scores && m_note == other.m_note;
}

}  // namespace sedm
