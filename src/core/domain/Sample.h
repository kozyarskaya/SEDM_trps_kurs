/**
 * @file Sample.h
 * @brief Образец мёда: сквозной порядковый номер, шифр, владелец, номинация, корзина.
 */
#pragma once

#include <QString>
#include <optional>

namespace sedm {

/// Образец мёда, зарегистрированный на конкурс (п. 4.3.1.1 ТЗ).
/// Дегустаторы видят только шифр — это обеспечивает анонимность (закрытая дегустация).
class Sample {
public:
    Sample() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int seqNumber() const { return m_seqNumber; }
    void setSeqNumber(int number) { m_seqNumber = number; }

    const QString& cipher() const { return m_cipher; }
    /// Задаёт шифр; допускается только четырёхзначное число 1000–9999 (DomainError).
    void setCipher(const QString& cipher);

    int participantId() const { return m_participantId; }
    void setParticipantId(int id) { m_participantId = id; }

    int nominationId() const { return m_nominationId; }
    void setNominationId(int id) { m_nominationId = id; }

    std::optional<int> basketId() const { return m_basketId; }
    void setBasketId(std::optional<int> basketId) { m_basketId = basketId; }
    bool isInBasket() const { return m_basketId.has_value(); }

    /// Проверяет формат шифра: ровно 4 цифры, первая не ноль.
    static bool isValidCipher(const QString& cipher);

private:
    int m_id = 0;                    ///< идентификатор в базе данных
    int m_seqNumber = 0;             ///< сквозной порядковый номер регистрации
    QString m_cipher;                ///< уникальный шифр (4 цифры)
    int m_participantId = 0;         ///< владелец образца
    int m_nominationId = 0;          ///< номинация образца
    std::optional<int> m_basketId;   ///< корзина (нет — образец не распределён)
};

}  // namespace sedm
