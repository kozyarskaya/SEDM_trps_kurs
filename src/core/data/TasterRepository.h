/**
 * @file TasterRepository.h
 * @brief Хранение учётных записей дегустаторов (таблица taster).
 */
#pragma once

#include "Taster.h"

#include <QSet>
#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к дегустаторам. PIN-код хранится только в виде bcrypt-хэша.
class TasterRepository {
public:
    explicit TasterRepository(Database& db) : m_db(db) {}

    QVector<Taster> list() const;                 ///< по команде и ФИО
    QVector<Taster> forTeam(int teamId) const;
    std::optional<Taster> byId(int id) const;
    /// Поиск по логину без учёта регистра и различия «е/ё».
    std::optional<Taster> byLogin(const QString& login) const;
    QSet<QString> existingLogins() const;
    void insert(Taster& taster) const;            ///< задаёт id
    void updateName(int id, const QString& fullName) const;
    void updatePin(int id, const QString& pinHash) const;
    void saveLockState(const Taster& taster) const;
    void remove(int id) const;
    /// Сеанс входа: новый токен при входе (вход с другого ПК делает старый недействительным).
    void setSessionToken(int id, const QString& token) const;
    QString sessionToken(int id) const;
    int countWithEvaluations(int teamId) const;   ///< дегустаторов команды с оценками
    bool hasEvaluations(int id) const;            ///< выставлял ли дегустатор оценки

private:
    Database& m_db;
};

}  // namespace sedm
