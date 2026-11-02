/**
 * @file PinGenerator.cpp
 * @brief Реализация генерации PIN-кодов.
 */
#include "PinGenerator.h"

#include <QRandomGenerator>

namespace sedm {

bool PinGenerator::isWellFormed(const QString& pin)
{
    if (pin.size() != kPinLength)
        return false;
    for (const QChar ch : pin)
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9'))
            return false;
    return true;
}

bool PinGenerator::isTrivial(const QString& pin)
{
    if (!isWellFormed(pin))
        return false;
    bool same = true, ascending = true, descending = true;
    for (int i = 1; i < pin.size(); ++i) {
        const int prev = pin[i - 1].digitValue();
        const int cur = pin[i].digitValue();
        same = same && cur == prev;
        ascending = ascending && cur == prev + 1;
        descending = descending && cur == prev - 1;
    }
    return same || ascending || descending;
}

QString PinGenerator::generate()
{
    QString pin;
    do {
        const quint32 value = QRandomGenerator::system()->bounded(10000u);
        pin = QStringLiteral("%1").arg(value, kPinLength, 10, QLatin1Char('0'));
    } while (isTrivial(pin));
    return pin;
}

}  // namespace sedm
