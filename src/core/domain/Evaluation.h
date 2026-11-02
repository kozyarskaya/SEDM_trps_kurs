/**
 * @file Evaluation.h
 * @brief Оценка образца одним дегустатором по четырём критериям.
 */
#pragma once

#include "Criterion.h"

#include <QDateTime>
#include <QString>
#include <array>
#include <optional>

namespace sedm {

/// Оценка дегустатором одного образца (п. 4.3.1.2 ТЗ): четыре целые оценки 0–100,
/// необязательное примечание и итоговый балл — среднее арифметическое оценок.
/// Пока не проставлены все четыре оценки, итоговый балл не определён.
/// Тот же класс хранит черновик — контрольную точку несохранённого ввода (п. 4.4.4 ТЗ).
class Evaluation {
public:
    static constexpr int kMaxNoteLength = 500;  ///< предельная длина примечания

    Evaluation() = default;
    Evaluation(int tasterId, int sampleId);

    int tasterId() const { return m_tasterId; }
    void setTasterId(int id) { m_tasterId = id; }
    int sampleId() const { return m_sampleId; }
    void setSampleId(int id) { m_sampleId = id; }

    /// Оценка по критерию; пусто — ещё не проставлена.
    std::optional<int> score(Criterion criterion) const;
    /// Ставит оценку; значение вне 0–100 отклоняется (DomainError).
    void setScore(Criterion criterion, int value);
    /// Снимает оценку по критерию.
    void clearScore(Criterion criterion);

    int ratedCount() const;                   ///< сколько критериев оценено
    bool isComplete() const;                  ///< оценены все четыре критерия
    bool isEmpty() const;                     ///< не оценено ничего и нет примечания
    std::optional<double> total() const;      ///< итоговый балл (среднее четырёх оценок)
    std::optional<int> totalTimesFour() const;///< сумма четырёх оценок (точный расчёт мест)

    const QString& note() const { return m_note; }
    /// Примечание; длиннее kMaxNoteLength символов отклоняется (DomainError).
    void setNote(const QString& note);

    const QDateTime& savedAt() const { return m_savedAt; }
    void setSavedAt(const QDateTime& time) { m_savedAt = time; }

    /// Совпадают ли оценки и примечание (для сравнения с черновиком).
    bool sameContentAs(const Evaluation& other) const;

private:
    int m_tasterId = 0;                                        ///< дегустатор
    int m_sampleId = 0;                                        ///< образец
    std::array<std::optional<int>, kCriteriaCount> m_scores{}; ///< оценки по критериям
    QString m_note;                                            ///< примечание
    QDateTime m_savedAt;                                       ///< время сохранения
};

}  // namespace sedm
