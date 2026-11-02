/**
 * @file BasketRepository.h
 * @brief Хранение корзин (таблица basket; состав — через sample.basket_id).
 */
#pragma once

#include "Basket.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к корзинам. Состав корзины загружается вместе с ней.
class BasketRepository {
public:
    explicit BasketRepository(Database& db) : m_db(db) {}

    QVector<Basket> list() const;                 ///< по номеру
    std::optional<Basket> byId(int id) const;
    QVector<Basket> forTeam(int teamId) const;    ///< корзины, назначенные команде
    void insert(Basket& basket) const;            ///< задаёт id
    void update(const Basket& basket) const;      ///< номер, номинация, количество, команда
    void assignTeam(int basketId, std::optional<int> teamId) const;
    void remove(int id) const;
    int nextNumber() const;                       ///< свободный номер (наибольший + 1)
    bool hasEvaluations(int basketId) const;      ///< образцы корзины уже оценивались

private:
    Database& m_db;
};

}  // namespace sedm
