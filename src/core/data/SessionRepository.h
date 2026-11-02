/**
 * @file SessionRepository.h
 * @brief Хранение конфигурации сеанса дегустации и учётной записи администратора.
 */
#pragma once

#include "Account.h"
#include "TastingSession.h"

namespace sedm {

class Database;

/// Доступ к конфигурации сеанса (tasting_session), учётной записи администратора
/// (app_admin) и очистке данных при создании нового проекта.
class SessionRepository {
public:
    explicit SessionRepository(Database& db) : m_db(db) {}

    TastingSession load() const;
    void save(const TastingSession& session) const;

    bool hasAdmin() const;                             ///< задан ли пароль администратора
    AdminAccount admin() const;
    void setAdminPassword(const QString& passwordHash) const;
    void saveAdminLockState(const AdminAccount& admin) const;

    /// Новый проект: удаляет все данные дегустации (пароль администратора сохраняется).
    void clearAllData() const;

private:
    Database& m_db;
};

}  // namespace sedm
