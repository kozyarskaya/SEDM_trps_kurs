/**
 * @file ParticipantRepository.h
 * @brief Хранение реестра участников (таблица participant).
 */
#pragma once

#include "Participant.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к реестру участников конкурса.
class ParticipantRepository {
public:
    explicit ParticipantRepository(Database& db) : m_db(db) {}

    QVector<Participant> list() const;   ///< по алфавиту ФИО
    std::optional<Participant> byId(int id) const;
    void insert(Participant& participant) const;   ///< задаёт id
    void update(const Participant& participant) const;
    void remove(int id) const;
    int count() const;
    int sampleCount(int id) const;   ///< зарегистрированных образцов участника

private:
    Database& m_db;
};

}  // namespace sedm
