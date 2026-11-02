/**
 * @file Basket.h
 * @brief Корзина — набор образцов одной номинации для оценки одной командой.
 */
#pragma once

#include <QString>
#include <QVector>
#include <optional>

namespace sedm {

class Sample;

/// Корзина образцов (п. 4.2.1, 4.3.1.1 ТЗ). Правила:
/// - в корзину попадают только образцы её номинации;
/// - образец может находиться только в одной корзине;
/// - число образцов не превышает желаемого количества;
/// - корзина назначается не более чем одной команде.
class Basket {
public:
    static constexpr int kMaxDesiredCount = 100;  ///< предел желаемого количества

    Basket() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int number() const { return m_number; }
    /// Номер корзины — положительное целое (DomainError).
    void setNumber(int number);

    int nominationId() const { return m_nominationId; }
    /// Номинацию можно сменить только у пустой корзины (DomainError).
    void setNominationId(int nominationId);

    int desiredCount() const { return m_desiredCount; }
    /// Желаемое количество: 1…kMaxDesiredCount и не меньше текущего числа образцов.
    void setDesiredCount(int count);

    std::optional<int> teamId() const { return m_teamId; }
    void setTeamId(std::optional<int> teamId) { m_teamId = teamId; }
    bool isAssigned() const { return m_teamId.has_value(); }

    const QVector<int>& sampleIds() const { return m_sampleIds; }
    /// Загрузка состава из базы данных без проверок (данные уже согласованы).
    void setSampleIds(const QVector<int>& ids) { m_sampleIds = ids; }

    int sampleCount() const { return m_sampleIds.size(); }
    int freeSlots() const { return m_desiredCount - m_sampleIds.size(); }
    bool isFull() const { return freeSlots() <= 0; }
    bool contains(int sampleId) const { return m_sampleIds.contains(sampleId); }

    /// Можно ли добавить образец; при отказе reason получает объяснение.
    bool canAccept(const Sample& sample, QString* reason = nullptr) const;
    /// Добавляет образец в корзину (DomainError при нарушении правил).
    void addSample(const Sample& sample);
    /// Убирает образец из корзины — он возвращается в пул доступных.
    void removeSample(int sampleId);

private:
    int m_id = 0;                  ///< идентификатор в базе данных
    int m_number = 0;              ///< номер корзины
    int m_nominationId = 0;        ///< номинация корзины
    int m_desiredCount = 1;        ///< желаемое количество образцов
    std::optional<int> m_teamId;   ///< команда, которой назначена корзина
    QVector<int> m_sampleIds;      ///< образцы в корзине (идентификаторы)
};

}  // namespace sedm
