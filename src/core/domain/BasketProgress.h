/**
 * @file BasketProgress.h
 * @brief Ход дегустации корзины конкретным дегустатором (класс-ассоциация).
 */
#pragma once

#include <QDateTime>
#include <QVector>

namespace sedm {

class Basket;
class Evaluation;

/// Дегустация корзины дегустатором: признак и время завершения.
/// Завершить можно только когда по каждому образцу корзины проставлены все
/// четыре оценки (п. 4.3.1.2 ТЗ); после завершения оценки не изменяются.
class BasketProgress {
public:
    BasketProgress() = default;
    BasketProgress(int tasterId, int basketId);

    int tasterId() const { return m_tasterId; }
    int basketId() const { return m_basketId; }

    bool isFinished() const { return m_finishedAt.isValid(); }
    const QDateTime& finishedAt() const { return m_finishedAt; }
    /// Загрузка состояния из базы данных.
    void setFinishedAt(const QDateTime& time) { m_finishedAt = time; }

    /// Образцы корзины, по которым у дегустатора нет полной оценки.
    static QVector<int> incompleteSampleIds(const Basket& basket,
                                            const QVector<Evaluation>& evaluations);

    /// Завершает дегустацию корзины (DomainError, если оценены не все образцы
    /// или корзина уже завершена).
    void finish(const Basket& basket, const QVector<Evaluation>& evaluations,
                const QDateTime& now);

private:
    int m_tasterId = 0;      ///< дегустатор
    int m_basketId = 0;      ///< корзина
    QDateTime m_finishedAt;  ///< время завершения (не задано — не завершена)
};

}  // namespace sedm
