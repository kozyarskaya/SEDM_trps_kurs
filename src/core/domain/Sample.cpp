/**
 * @file Sample.cpp
 * @brief Реализация образца мёда.
 */
#include "Sample.h"
#include "DomainError.h"

#include <QRegularExpression>

namespace sedm {

void Sample::setCipher(const QString& cipher)
{
    if (!isValidCipher(cipher))
        throw DomainError(QStringLiteral("Шифр «%1» недопустим: нужно четырёхзначное число от 1000 до 9999.")
                              .arg(cipher));
    m_cipher = cipher;
}

bool Sample::isValidCipher(const QString& cipher)
{
    static const QRegularExpression re(QStringLiteral("^[1-9][0-9]{3}$"));
    return re.match(cipher).hasMatch();
}

}  // namespace sedm
