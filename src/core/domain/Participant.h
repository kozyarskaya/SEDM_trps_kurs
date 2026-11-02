/**
 * @file Participant.h
 * @brief Участник конкурса — владелец образцов мёда (запись реестра).
 */
#pragma once

#include <QString>
#include <QStringList>

namespace sedm {

/// Участник конкурса: ФИО, контакты, регион и заявленная номинация (п. 4.2.1 ТЗ).
class Participant {
public:
    Participant() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const QString& fullName() const { return m_fullName; }
    /// Задаёт ФИО (лишние пробелы удаляются); проверка — в validate().
    void setFullName(const QString& fullName) { m_fullName = fullName.simplified(); }

    const QString& phone() const { return m_phone; }
    void setPhone(const QString& phone) { m_phone = phone.simplified(); }

    const QString& email() const { return m_email; }
    void setEmail(const QString& email) { m_email = email.trimmed(); }

    const QString& region() const { return m_region; }
    void setRegion(const QString& region) { m_region = region.simplified(); }

    int nominationId() const { return m_nominationId; }
    void setNominationId(int id) { m_nominationId = id; }

    /// Фамилия — первое слово ФИО.
    QString surname() const;
    /// Сокращённое имя «Фамилия И. О.» для таблиц.
    QString shortName() const;

    /// Проверяет запись и возвращает список замечаний (пустой — запись верна).
    QStringList validate() const;

    static bool isValidFullName(const QString& fullName);
    static bool isValidPhone(const QString& phone);   ///< пустой телефон допустим
    static bool isValidEmail(const QString& email);   ///< пустой e-mail допустим

private:
    int m_id = 0;            ///< идентификатор в базе данных
    QString m_fullName;      ///< фамилия, имя, отчество
    QString m_phone;         ///< контактный телефон
    QString m_email;         ///< адрес электронной почты
    QString m_region;        ///< регион (субъект РФ)
    int m_nominationId = 0;  ///< заявленная номинация
};

}  // namespace sedm
