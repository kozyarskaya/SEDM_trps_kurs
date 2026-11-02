/**
 * @file CipherGenerator.h
 * @brief Генерация уникальных шифров образцов.
 */
#pragma once

#include <QSet>
#include <QString>

namespace sedm {

/// Генератор шифров образцов (решение заказчика В11): шифр — случайное
/// четырёхзначное число 1000–9999, не связанное с порядковым номером и владельцем;
/// уникальность проверяется по множеству уже выданных шифров (и дополнительно —
/// ограничением UNIQUE в базе данных). Используется криптостойкий генератор
/// случайных чисел системы, поэтому шифр нельзя предсказать.
class CipherGenerator {
public:
    static constexpr int kMinCipher = 1000;  ///< наименьший шифр
    static constexpr int kMaxCipher = 9999;  ///< наибольший шифр
    static constexpr int kCapacity = kMaxCipher - kMinCipher + 1;  ///< всего шифров (9000)

    /// usedCiphers — шифры, уже выданные образцам.
    explicit CipherGenerator(const QSet<QString>& usedCiphers = {});

    /// Выдаёт новый уникальный шифр (DomainError, если все 9000 шифров заняты).
    QString next();
    /// Сколько шифров ещё свободно.
    int remaining() const { return kCapacity - m_used.size(); }
    /// Отмечает шифр как занятый (например, шифр, сохранённый другим администратором).
    void markUsed(const QString& cipher) { m_used.insert(cipher); }

private:
    QSet<QString> m_used;  ///< занятые шифры
};

}  // namespace sedm
