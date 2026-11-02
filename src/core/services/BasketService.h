/**
 * @file BasketService.h
 * @brief Формирование корзин образцов и назначение их командам.
 */
#pragma once

#include "Basket.h"
#include "Sample.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;
class AuditService;

/// Сервис корзин (п. 4.3.1.1 ТЗ): номер, номинация, желаемое количество,
/// состав из пула доступных образцов. Удалённый из корзины образец
/// возвращается в пул. Корзина назначается одной команде (решение В12).
class BasketService {
public:
    BasketService(Database& db, AuditService& audit);

    QVector<Basket> baskets() const;
    std::optional<Basket> basket(int basketId) const;
    int suggestedNumber() const;   ///< свободный номер для новой корзины

    Basket createBasket(int number, int nominationId, int desiredCount);
    void updateBasket(int basketId, int number, int nominationId, int desiredCount);
    /// Удаление: только если образцы корзины ещё не оценивались; образцы — в пул.
    void removeBasket(int basketId);

    /// Пул: образцы номинации, не входящие ни в одну корзину.
    QVector<Sample> availableSamples(int nominationId) const;
    QVector<Sample> basketSamples(int basketId) const;
    void addSample(int basketId, int sampleId);
    /// Возвращает образец в пул (нельзя, если он уже оценивался).
    void removeSample(int basketId, int sampleId);
    /// Дополняет корзину образцами из пула до желаемого количества; возвращает число добавленных.
    int fillFromPool(int basketId);

    /// Назначение команде (нет значения — снять назначение).
    void assignTeam(int basketId, std::optional<int> teamId);

private:
    Basket load(int basketId) const;

    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
