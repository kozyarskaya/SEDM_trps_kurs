/**
 * @file DomainError.h
 * @brief Исключение «нарушено правило предметной области».
 *
 * Бросается классами предметной области, когда действие пользователя нарушает
 * требование ТЗ (например, добавление образца сверх желаемого количества в
 * корзину). Текст сообщения предназначен для показа пользователю.
 */
#pragma once

#include <QString>
#include <stdexcept>

namespace sedm {

/// Нарушение правила предметной области; сообщение — на русском для пользователя.
class DomainError : public std::runtime_error {
public:
    explicit DomainError(const QString& message);
    /// Текст сообщения в кодировке Unicode.
    QString message() const;
};

}  // namespace sedm
