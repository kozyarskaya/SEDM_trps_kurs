/**
 * @file TastingSession.cpp
 * @brief Реализация конфигурации сеанса дегустации.
 */
#include "TastingSession.h"

#include <QLocale>

namespace sedm {

bool TastingSession::canTasterEnter(bool allBasketsFinished) const
{
    return !allBasketsFinished || m_allowReentry;
}

QString TastingSession::displayText() const
{
    if (!m_startsAt.isValid())
        return m_title;
    const QLocale ru(QLocale::Russian, QLocale::Russia);
    return QStringLiteral("%1 · %2 г., %3")
        .arg(m_title, ru.toString(m_startsAt.date(), QStringLiteral("d MMMM yyyy")),
             m_startsAt.time().toString(QStringLiteral("HH:mm")));
}

}  // namespace sedm
