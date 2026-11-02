/**
 * @file LoginGenerator.cpp
 * @brief Реализация формирования логинов.
 */
#include "LoginGenerator.h"

#include <QStringList>

namespace sedm {

QString LoginGenerator::normalizedForLookup(const QString& login)
{
    QString s = login.simplified().toLower();
    s.replace(QChar(0x0451), QChar(0x0435));  // «ё» -> «е»: Ёлкина = Елкина
    return s;
}

QString LoginGenerator::makeLogin(const QString& fullName, const QSet<QString>& existing)
{
    QString base = fullName.simplified().section(QLatin1Char(' '), 0, 0);
    if (base.isEmpty())
        base = QStringLiteral("Дегустатор");
    // Заглавная буква в каждой части двойной фамилии: «Иванова-Петрова»
    QStringList parts = base.split(QLatin1Char('-'));
    for (QString& part : parts)
        part = part.left(1).toUpper() + part.mid(1).toLower();
    base = parts.join(QLatin1Char('-'));

    QSet<QString> taken;
    for (const QString& login : existing)
        taken.insert(normalizedForLookup(login));

    if (!taken.contains(normalizedForLookup(base)))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = base + QString::number(n);
        if (!taken.contains(normalizedForLookup(candidate)))
            return candidate;
    }
}

}  // namespace sedm
