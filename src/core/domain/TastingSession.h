/**
 * @file TastingSession.h
 * @brief Конфигурация сеанса дегустации.
 */
#pragma once

#include <QDateTime>
#include <QString>

namespace sedm {

/// Сеанс дегустации (п. 4.2.1 ТЗ): название, дата и время проведения (выводятся на
/// экране дегустатора и в шапке ведомости) и разрешение повторного входа после
/// завершения — тогда дегустатор может только просматривать свои оценки.
class TastingSession {
public:
    TastingSession() = default;

    const QString& title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title.simplified(); }

    const QDateTime& startsAt() const { return m_startsAt; }
    void setStartsAt(const QDateTime& time) { m_startsAt = time; }

    bool allowReentry() const { return m_allowReentry; }
    void setAllowReentry(bool allow) { m_allowReentry = allow; }

    /// Может ли дегустатор войти: если все его корзины завершены — только при разрешении.
    bool canTasterEnter(bool allBasketsFinished) const;
    /// Строка для экрана: «Дегустация мёда · 18 октября 2026 г., 10:00».
    QString displayText() const;

private:
    QString m_title = QStringLiteral("Дегустация мёда");  ///< название
    QDateTime m_startsAt;                                 ///< дата и время проведения
    bool m_allowReentry = false;                          ///< повторный вход после завершения
};

}  // namespace sedm
