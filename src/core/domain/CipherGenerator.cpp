/**
 * @file CipherGenerator.cpp
 * @brief Алгоритм генерации уникального шифра.
 *
 * Пока свободных шифров больше половины, используется случайный выбор с
 * повтором при совпадении: вероятность повтора p = n/9000 < 0,5, среднее число
 * попыток 1/(1−p) < 2. При большей заполненности выбор делается сразу из списка
 * свободных шифров, чтобы время работы не росло.
 */
#include "CipherGenerator.h"
#include "DomainError.h"

#include <QRandomGenerator>
#include <QVector>

namespace sedm {

CipherGenerator::CipherGenerator(const QSet<QString>& usedCiphers)
    : m_used(usedCiphers)
{
}

QString CipherGenerator::next()
{
    if (remaining() <= 0)
        throw DomainError(QStringLiteral("Все %1 четырёхзначных шифров уже выданы.").arg(kCapacity));

    QRandomGenerator* rng = QRandomGenerator::system();
    QString cipher;
    if (m_used.size() < kCapacity / 2) {
        do {
            cipher = QString::number(kMinCipher + static_cast<int>(rng->bounded(kCapacity)));
        } while (m_used.contains(cipher));
    } else {
        QVector<int> free;
        free.reserve(remaining());
        for (int code = kMinCipher; code <= kMaxCipher; ++code)
            if (!m_used.contains(QString::number(code)))
                free.append(code);
        cipher = QString::number(free.at(static_cast<int>(rng->bounded(free.size()))));
    }
    m_used.insert(cipher);
    return cipher;
}

}  // namespace sedm
