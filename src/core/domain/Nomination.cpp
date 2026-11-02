/**
 * @file Nomination.cpp
 * @brief Реализация номинации конкурса.
 */
#include "Nomination.h"
#include "DomainError.h"

namespace sedm {

Nomination::Nomination(int id, const QString& name)
    : m_id(id)
{
    setName(name);
}

void Nomination::setName(const QString& name)
{
    const QString normalized = normalizedName(name);
    if (normalized.isEmpty())
        throw DomainError(QStringLiteral("Название номинации не может быть пустым."));
    if (normalized.size() > kMaxNameLength)
        throw DomainError(QStringLiteral("Название номинации длиннее %1 символов.").arg(kMaxNameLength));
    m_name = normalized;
}

QString Nomination::normalizedName(const QString& raw)
{
    QString s = raw.simplified();
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

}  // namespace sedm
