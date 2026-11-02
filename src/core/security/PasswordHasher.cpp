/**
 * @file PasswordHasher.cpp
 * @brief Обёртка над crypt_blowfish (bcrypt).
 */
#include "PasswordHasher.h"

#include <QByteArray>
#include <QRandomGenerator>
#include <stdexcept>

extern "C" {
#include "crypt_blowfish.h"
}

namespace sedm {

namespace {
constexpr int kSaltBytes = 16;     // соль bcrypt — 128 бит
constexpr int kSettingSize = 30;   // «$2b$10$» + 22 символа соли + '\0'
constexpr int kHashSize = 61;      // 60 символов хэша + '\0'
}  // namespace

QString PasswordHasher::hash(const QString& secret, int cost)
{
    char random[kSaltBytes];
    for (char& b : random)
        b = static_cast<char>(QRandomGenerator::system()->bounded(256));

    char setting[kSettingSize];
    if (!_crypt_gensalt_blowfish_rn("$2b$", static_cast<unsigned long>(cost), random, kSaltBytes,
                                    setting, kSettingSize))
        throw std::runtime_error("bcrypt: не удалось сформировать соль");

    const QByteArray key = secret.toUtf8();
    char output[kHashSize];
    if (!_crypt_blowfish_rn(key.constData(), setting, output, kHashSize))
        throw std::runtime_error("bcrypt: не удалось вычислить хэш");
    return QString::fromLatin1(output);
}

bool PasswordHasher::verify(const QString& secret, const QString& storedHash)
{
    if (storedHash.size() != kHashSize - 1)
        return false;
    const QByteArray key = secret.toUtf8();
    const QByteArray setting = storedHash.toLatin1();
    char output[kHashSize];
    if (!_crypt_blowfish_rn(key.constData(), setting.constData(), output, kHashSize))
        return false;
    // Сравнение без раннего выхода, чтобы время не зависело от совпадающих символов
    unsigned char diff = 0;
    for (int i = 0; i < kHashSize - 1; ++i)
        diff |= static_cast<unsigned char>(output[i] ^ setting[i]);
    return diff == 0;
}

}  // namespace sedm
