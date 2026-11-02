/**
 * @file PasswordHasher.h
 * @brief Хэширование пароля администратора и PIN-кодов алгоритмом bcrypt.
 */
#pragma once

#include <QString>

namespace sedm {

/// Хэширование bcrypt (п. 4.4.6 ТЗ) на основе библиотеки crypt_blowfish.
/// В базе данных хранится только хэш вида «$2b$10$<соль><хэш>» (60 символов);
/// сам пароль или PIN-код нигде не сохраняется.
class PasswordHasher {
public:
    static constexpr int kDefaultCost = 10;  ///< коэффициент трудоёмкости (2^10 раундов)

    /// Вычисляет bcrypt-хэш секрета со случайной солью (16 байт из системного ГСЧ).
    static QString hash(const QString& secret, int cost = kDefaultCost);
    /// Проверяет секрет по сохранённому хэшу (сравнение за постоянное время).
    static bool verify(const QString& secret, const QString& storedHash);
};

}  // namespace sedm
