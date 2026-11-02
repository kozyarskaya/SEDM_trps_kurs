/**
 * @file Messages.h
 * @brief Окна сообщений на русском языке и единая обработка ошибок действий.
 */
#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class QWidget;

namespace sedm {
class Database;
}

namespace sedm::ui {

/// Сообщения пользователю (п. 4.3.2 ТЗ: интерфейс на русском языке).
class Messages {
public:
    static void info(QWidget* parent, const QString& title, const QString& text);
    static void warning(QWidget* parent, const QString& title, const QString& text);
    static void error(QWidget* parent, const QString& title, const QString& text);
    /// Вопрос «да/нет»; dangerous — подтверждаемое действие необратимо (кнопка «Нет» по умолчанию).
    static bool confirm(QWidget* parent, const QString& title, const QString& text,
                        const QString& yes = QStringLiteral("Да"), const QString& no = QStringLiteral("Нет"),
                        bool dangerous = false);
    /// Выбор из нескольких вариантов; возвращает номер кнопки (−1 — окно закрыто).
    static int choose(QWidget* parent, const QString& title, const QString& text, const QStringList& buttons,
                      int defaultButton = 0, int escapeButton = -1);

    /// Выполняет действие; ошибки правил (DomainError) и СУБД (DbError) показывает
    /// понятным сообщением (п. 4.4.1–4.4.2 ТЗ). Возвращает true, если ошибок не было.
    static bool guard(QWidget* parent, const std::function<void()>& action,
                      const QString& title = QStringLiteral("Действие не выполнено"));
    /// То же; при потере связи с сервером выполняется переподключение и одна повторная попытка.
    static bool guard(QWidget* parent, Database& db, const std::function<void()>& action,
                      const QString& title = QStringLiteral("Действие не выполнено"));
};

}  // namespace sedm::ui
