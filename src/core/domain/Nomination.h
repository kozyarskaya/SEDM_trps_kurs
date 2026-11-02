/**
 * @file Nomination.h
 * @brief Номинация конкурса (элемент справочника номинаций).
 */
#pragma once

#include <QString>

namespace sedm {

/// Номинация конкурса, например «Липовый» или «Гречишный».
class Nomination {
public:
    static constexpr int kMaxNameLength = 100;  ///< предельная длина названия

    Nomination() = default;
    Nomination(int id, const QString& name);

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    const QString& name() const { return m_name; }
    /// Задаёт название; пустое или слишком длинное отклоняется (DomainError).
    void setName(const QString& name);

    /// Приводит название к единому виду: без лишних пробелов, с заглавной буквы.
    static QString normalizedName(const QString& raw);

private:
    int m_id = 0;     ///< идентификатор в базе данных (0 — ещё не сохранена)
    QString m_name;   ///< название номинации
};

}  // namespace sedm
