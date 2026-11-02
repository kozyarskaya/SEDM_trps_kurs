/**
 * @file SessionService.h
 * @brief Конфигурация сеанса дегустации и создание нового проекта.
 */
#pragma once

#include "TastingSession.h"

namespace sedm {

class Database;
class AuditService;

/// Сервис сеанса (п. 4.2.1, 4.3.1.1 ТЗ): название, дата и время дегустации,
/// разрешение повторного входа дегустаторов после завершения (только просмотр).
class SessionService {
public:
    static constexpr int kMaxTitleLength = 150;

    SessionService(Database& db, AuditService& audit);

    TastingSession session() const;
    void saveSession(const TastingSession& session);
    /// Новый проект: удаляет все данные дегустации и журнал; пароль администратора
    /// сохраняется. Резервную копию перед очисткой создаёт окно (BackupService).
    void newProject();

private:
    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
