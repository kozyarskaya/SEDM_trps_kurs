/**
 * @file AuditRepository.h
 * @brief Хранение журнала действий пользователей (таблица audit_log).
 */
#pragma once

#include "AuditEntry.h"

#include <QVector>

namespace sedm {

class Database;

/// Доступ к журналу действий.
class AuditRepository {
public:
    explicit AuditRepository(Database& db) : m_db(db) {}

    void append(const AuditEntry& entry) const;   ///< время записи — время события
    /// Записи от новых к старым; limit = 0 — все записи.
    QVector<AuditEntry> list(int limit = 0, int offset = 0) const;
    int count() const;

private:
    Database& m_db;
};

}  // namespace sedm
