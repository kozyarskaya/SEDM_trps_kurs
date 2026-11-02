/**
 * @file AuditEntry.h
 * @brief Запись журнала действий пользователей.
 */
#pragma once

#include <QDateTime>
#include <QString>

namespace sedm {

/// Запись журнала действий (п. 4.3.4 ТЗ): кто, когда, с какого рабочего места и что сделал.
class AuditEntry {
public:
    AuditEntry() = default;
    AuditEntry(const QDateTime& at, const QString& actor, const QString& role,
               const QString& workstation, const QString& action, const QString& details)
        : m_at(at), m_actor(actor), m_role(role), m_workstation(workstation),
          m_action(action), m_details(details) {}

    qint64 id() const { return m_id; }
    void setId(qint64 id) { m_id = id; }
    const QDateTime& at() const { return m_at; }
    const QString& actor() const { return m_actor; }
    const QString& role() const { return m_role; }
    const QString& workstation() const { return m_workstation; }
    const QString& action() const { return m_action; }
    const QString& details() const { return m_details; }

private:
    qint64 m_id = 0;        ///< порядковый номер записи
    QDateTime m_at;         ///< время действия
    QString m_actor;        ///< логин пользователя
    QString m_role;         ///< роль («администратор», «дегустатор»)
    QString m_workstation;  ///< имя компьютера
    QString m_action;       ///< действие («Вход», «Сохранение оценок» …)
    QString m_details;      ///< подробности (шифры, номера корзин …)
};

}  // namespace sedm
