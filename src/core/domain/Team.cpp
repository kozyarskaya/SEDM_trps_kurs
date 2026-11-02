/**
 * @file Team.cpp
 * @brief Реализация дегустационной команды.
 */
#include "Team.h"
#include "DomainError.h"

namespace sedm {

void Team::setNumber(int number)
{
    if (number <= 0)
        throw DomainError(QStringLiteral("Номер команды должен быть положительным числом."));
    m_number = number;
}

void Team::setName(const QString& name)
{
    const QString normalized = name.simplified();
    if (normalized.size() > kMaxNameLength)
        throw DomainError(QStringLiteral("Название команды длиннее %1 символов.").arg(kMaxNameLength));
    m_name = normalized;
}

QString Team::displayName() const
{
    if (m_name.isEmpty())
        return QStringLiteral("Команда %1").arg(m_number);
    return QStringLiteral("Команда %1 «%2»").arg(m_number).arg(m_name);
}

}  // namespace sedm
