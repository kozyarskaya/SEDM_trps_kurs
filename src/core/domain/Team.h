/**
 * @file Team.h
 * @brief Дегустационная команда (комиссия).
 */
#pragma once

#include <QString>

namespace sedm {

/// Команда дегустаторов: номер (целое число) и необязательное название (п. 4.2.1 ТЗ).
class Team {
public:
    static constexpr int kMaxNameLength = 100;  ///< предельная длина названия

    Team() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int number() const { return m_number; }
    /// Номер команды — положительное целое (DomainError).
    void setNumber(int number);

    const QString& name() const { return m_name; }
    /// Название необязательно; слишком длинное отклоняется (DomainError).
    void setName(const QString& name);

    /// Название для экрана: «Команда 2» или «Команда 2 «Эксперты»».
    QString displayName() const;

private:
    int m_id = 0;      ///< идентификатор в базе данных
    int m_number = 0;  ///< номер команды
    QString m_name;    ///< название (может быть пустым)
};

}  // namespace sedm
