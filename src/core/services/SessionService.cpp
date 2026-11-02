/**
 * @file SessionService.cpp
 * @brief Реализация сервиса сеанса дегустации.
 */
#include "SessionService.h"
#include "AuditService.h"
#include "DomainError.h"
#include "SessionRepository.h"

namespace sedm {

SessionService::SessionService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

TastingSession SessionService::session() const
{
    return SessionRepository(m_db).load();
}

void SessionService::saveSession(const TastingSession& session)
{
    if (session.title().isEmpty())
        throw DomainError(QStringLiteral("Укажите название дегустации."));
    if (session.title().size() > kMaxTitleLength)
        throw DomainError(QStringLiteral("Название дегустации — не длиннее %1 символов.").arg(kMaxTitleLength));
    SessionRepository(m_db).save(session);
    m_audit.log(QStringLiteral("Изменение настроек сеанса"),
                QStringLiteral("%1; повторный вход %2").arg(session.displayText(),
                    session.allowReentry() ? QStringLiteral("разрешён") : QStringLiteral("запрещён")));
}

void SessionService::newProject()
{
    SessionRepository(m_db).clearAllData();
    m_audit.log(QStringLiteral("Создание нового проекта"), QStringLiteral("Все данные дегустации удалены"));
}

}  // namespace sedm
