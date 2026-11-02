/**
 * @file Taster.cpp
 * @brief Реализация дегустатора.
 */
#include "Taster.h"

#include <QStringList>

namespace sedm {

QString Taster::surname() const
{
    return m_fullName.section(QLatin1Char(' '), 0, 0);
}

QString Taster::shortName() const
{
    const QStringList parts = m_fullName.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QString();
    QString result = parts.first();
    for (int i = 1; i < parts.size() && i < 3; ++i)
        result += QStringLiteral(" ") + parts[i].left(1) + QStringLiteral(".");
    return result;
}

}  // namespace sedm
