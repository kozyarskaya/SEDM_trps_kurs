/**
 * @file AuditService.h
 * @brief Журнал действий пользователей (п. 4.3.4–4.3.5 ТЗ).
 */
#pragma once

#include "AuditEntry.h"

#include <QString>
#include <QVector>

namespace sedm {

class Database;

/// Сервис журнала действий. Каждое значимое действие (вход, сохранение оценок,
/// изменение данных, экспорт, резервное копирование) записывается в таблицу
/// audit_log. Если сервер недоступен, запись временно сохраняется в локальный
/// CSV-файл и переносится в базу при восстановлении связи — журнал не теряется.
class AuditService {
public:
    /// pendingFile — путь к локальному файлу отложенных записей.
    AuditService(Database& db, const QString& pendingFile);

    /// Кто выполняет действия (логин и роль); имя компьютера определяется автоматически.
    void setActor(const QString& login, const QString& role);
    const QString& actor() const { return m_actor; }

    /// Записывает действие; при ошибке СУБД — в локальный файл (исключений не бросает).
    void log(const QString& action, const QString& details = QString());
    /// Переносит отложенные записи из локального файла в базу.
    void flushPending();
    int pendingCount() const;

    QVector<AuditEntry> list(int limit = 0, int offset = 0) const;
    int count() const;
    /// Выгрузка всего журнала в CSV (UTF-8, разделитель «;», п. 4.3.4–4.3.5 ТЗ);
    /// возвращает число записей. DomainError, если файл не удалось записать.
    int exportCsv(const QString& path);

    /// Имя компьютера (рабочего места) для журнала.
    static QString workstation();

private:
    void appendPending(const AuditEntry& entry) const;

    Database& m_db;
    QString m_pendingFile;
    QString m_actor = QStringLiteral("—");
    QString m_role;
};

}  // namespace sedm
