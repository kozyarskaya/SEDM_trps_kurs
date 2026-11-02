/**
 * @file SampleResult.h
 * @brief Результат образца для итоговой ведомости: оценки дегустаторов, средний балл, место.
 */
#pragma once

#include "Evaluation.h"
#include "Participant.h"
#include "Sample.h"

#include <QString>
#include <QVector>
#include <optional>

namespace sedm {

/// Оценка образца одним дегустатором в итоговой ведомости.
struct TasterScore {
    int tasterId = 0;        ///< дегустатор
    QString tasterName;      ///< «Фамилия И. О.»
    int teamNumber = 0;      ///< номер команды
    Evaluation evaluation;   ///< оценки по критериям
};

/// Результат образца. Средний балл — среднее итоговых баллов всех дегустаторов,
/// у которых сохранены все четыре оценки этого образца; округляется до сотых.
class SampleResult {
public:
    SampleResult() = default;
    SampleResult(const Sample& sample, const Participant& owner, const QString& nominationName);

    const Sample& sample() const { return m_sample; }
    const Participant& owner() const { return m_owner; }
    const QString& nominationName() const { return m_nominationName; }

    void addScore(const TasterScore& score) { m_scores.append(score); }
    const QVector<TasterScore>& scores() const { return m_scores; }

    int completeCount() const;                    ///< число полных оценок
    qint64 sumTimesFour() const;                  ///< сумма итоговых баллов × 4
    std::optional<qint64> averageCents() const;   ///< средний балл в сотых долях (округление)
    std::optional<double> average() const;        ///< средний балл, округлённый до сотых

    int place() const { return m_place; }         ///< место в номинации (0 — без места)
    void setPlace(int place) { m_place = place; }

private:
    Sample m_sample;                 ///< образец
    Participant m_owner;             ///< владелец образца
    QString m_nominationName;        ///< название номинации
    QVector<TasterScore> m_scores;   ///< оценки дегустаторов
    int m_place = 0;                 ///< место в номинации
};

}  // namespace sedm
