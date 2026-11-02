/**
 * @file Participant.cpp
 * @brief Реализация участника конкурса и проверок полей реестра.
 */
#include "Participant.h"

#include <QRegularExpression>

namespace sedm {

QString Participant::surname() const
{
    return m_fullName.section(QLatin1Char(' '), 0, 0);
}

QString Participant::shortName() const
{
    const QStringList parts = m_fullName.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QString();
    QString result = parts.first();            // фамилия
    for (int i = 1; i < parts.size() && i < 3; ++i)
        result += QStringLiteral(" ") + parts[i].left(1) + QStringLiteral(".");  // инициалы
    return result;
}

QStringList Participant::validate() const
{
    QStringList problems;
    if (m_fullName.isEmpty())
        problems << QStringLiteral("Не указаны ФИО участника.");
    else if (!isValidFullName(m_fullName))
        problems << QStringLiteral("ФИО должны состоять минимум из фамилии и имени (только буквы, дефис, пробел).");
    if (!isValidPhone(m_phone))
        problems << QStringLiteral("Телефон указан неверно: нужно 10–15 цифр, например +7 912 345-67-89.");
    if (!isValidEmail(m_email))
        problems << QStringLiteral("E-mail указан неверно, ожидается вид имя@домен.ру.");
    if (m_region.isEmpty())
        problems << QStringLiteral("Не указан регион.");
    if (m_nominationId <= 0)
        problems << QStringLiteral("Не указана номинация.");
    return problems;
}

bool Participant::isValidFullName(const QString& fullName)
{
    static const QRegularExpression re(
        QStringLiteral("^\\p{L}[\\p{L}'’.\\-]*( \\p{L}[\\p{L}'’.\\-]*)+$"));
    return re.match(fullName.simplified()).hasMatch();
}

bool Participant::isValidPhone(const QString& phone)
{
    if (phone.trimmed().isEmpty())
        return true;
    static const QRegularExpression allowed(QStringLiteral("^\\+?[0-9 ()\\-]+$"));
    if (!allowed.match(phone).hasMatch())
        return false;
    int digits = 0;
    for (const QChar ch : phone)
        if (ch.isDigit())
            ++digits;
    return digits >= 10 && digits <= 15;
}

bool Participant::isValidEmail(const QString& email)
{
    if (email.trimmed().isEmpty())
        return true;
    static const QRegularExpression re(QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$"));
    return re.match(email.trimmed()).hasMatch();
}

}  // namespace sedm
