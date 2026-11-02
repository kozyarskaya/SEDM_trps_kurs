/**
 * @file Taster.h
 * @brief Эксперт-дегустатор — член дегустационной команды.
 */
#pragma once

#include "Account.h"

namespace sedm {

/// Дегустатор: ФИО, логин (фамилия кириллицей), хэш PIN-кода, команда (п. 4.2.1 ТЗ).
class Taster : public Account {
public:
    Taster() = default;

    QString roleName() const override { return QStringLiteral("дегустатор"); }

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    int teamId() const { return m_teamId; }
    void setTeamId(int teamId) { m_teamId = teamId; }

    const QString& fullName() const { return m_fullName; }
    void setFullName(const QString& fullName) { m_fullName = fullName.simplified(); }

    /// Фамилия — первое слово ФИО.
    QString surname() const;
    /// «Фамилия И. О.» для таблиц мониторинга и ведомости.
    QString shortName() const;

private:
    int m_id = 0;       ///< идентификатор в базе данных
    int m_teamId = 0;   ///< команда дегустатора
    QString m_fullName; ///< фамилия, имя, отчество
};

}  // namespace sedm
