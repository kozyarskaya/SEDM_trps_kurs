/**
 * @file NominationRepository.h
 * @brief Хранение справочника номинаций (таблица nomination).
 */
#pragma once

#include "Nomination.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к справочнику номинаций.
class NominationRepository {
public:
    explicit NominationRepository(Database& db) : m_db(db) {}

    QVector<Nomination> list() const;                            ///< по алфавиту
    std::optional<Nomination> byId(int id) const;
    std::optional<Nomination> byName(const QString& name) const;  ///< без учёта регистра
    void insert(Nomination& nomination) const;                   ///< задаёт id
    void update(const Nomination& nomination) const;
    void remove(int id) const;
    /// Сколько записей ссылается на номинацию (участники, образцы, корзины).
    int usageCount(int id) const;

private:
    Database& m_db;
};

}  // namespace sedm
