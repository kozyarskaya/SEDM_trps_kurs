/**
 * @file PinGenerator.h
 * @brief Генерация PIN-кодов дегустаторов.
 */
#pragma once

#include <QString>

namespace sedm {

/// PIN-код дегустатора — 4 цифры (удобно для возрастных пользователей).
/// Простые комбинации (0000, 1111, 1234, 4321 …) не выдаются.
class PinGenerator {
public:
    static constexpr int kPinLength = 4;  ///< длина PIN-кода

    /// Случайный нетривиальный PIN-код из криптостойкого генератора.
    static QString generate();
    /// Ровно kPinLength цифр.
    static bool isWellFormed(const QString& pin);
    /// Все цифры одинаковы или идут подряд по возрастанию/убыванию.
    static bool isTrivial(const QString& pin);
};

}  // namespace sedm
