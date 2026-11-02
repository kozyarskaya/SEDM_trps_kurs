/**
 * @file TeamRepository.h
 * @brief Хранение дегустационных команд (таблица team).
 */
#pragma once

#include "Team.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к командам.
class TeamRepository {
public:
    explicit TeamRepository(Database& db) : m_db(db) {}

    QVector<Team> list() const;   ///< по номеру
    std::optional<Team> byId(int id) const;
    void insert(Team& team) const;   ///< задаёт id
    void update(const Team& team) const;
    void remove(int id) const;
    int nextNumber() const;          ///< свободный номер (наибольший + 1)

private:
    Database& m_db;
};

}  // namespace sedm
