/**
 * @file UiTestUtil.h
 * @brief Помощники автоматических тестов интерфейса: ответы на окна, поиск кнопок, снимки.
 */
#pragma once

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <QtTest>
#include <functional>
#include <utility>

namespace sedm::test {

/// Видимая кнопка с заданным текстом внутри root (nullptr — не найдена).
inline QPushButton* buttonWithText(QWidget* root, const QString& text)
{
    for (QPushButton* b : root->findChildren<QPushButton*>())
        if (b->text() == text && b->isVisible())
            return b;
    return nullptr;
}

/// Действие с модальным окном: подготовить поля (необязательно) и нажать кнопку.
struct DialogStep {
    QString button;                          ///< текст кнопки
    std::function<void(QWidget*)> prepare;   ///< заполнение полей окна
};

/// Отвечает на модальные окна программы по порядку шагов (окна сообщений и диалоги).
class DialogAnswerer {
public:
    explicit DialogAnswerer(QVector<DialogStep> steps) : m_steps(std::move(steps))
    {
        m_timer.setInterval(40);
        QObject::connect(&m_timer, &QTimer::timeout, [this]() { step(); });
        m_timer.start();
    }
    DialogAnswerer(std::initializer_list<QString> buttons)
        : DialogAnswerer([&]() {
              QVector<DialogStep> steps;
              for (const QString& b : buttons)
                  steps.append({b, nullptr});
              return steps;
          }())
    {
    }
    bool done() const { return m_steps.isEmpty(); }
    /// Тексты показанных окон сообщений (по порядку).
    const QStringList& seen() const { return m_seen; }

private:
    void step()
    {
        QWidget* dialog = QApplication::activeModalWidget();
        if (!dialog || m_steps.isEmpty() || m_busy)
            return;
        QPushButton* button = buttonWithText(dialog, m_steps.first().button);
        if (!button)
            return;
        m_busy = true;
        const DialogStep current = m_steps.takeFirst();
        if (auto* text = dialog->findChild<QLabel*>(QStringLiteral("messageText")))
            m_seen << text->text();
        else
            m_seen << dialog->windowTitle();
        if (current.prepare)
            current.prepare(dialog);
        // Нажатие — из очереди событий, а не из обработчика таймера: если кнопка
        // откроет следующее окно, таймер сможет ответить и на него (Qt не вызывает
        // обработчик того же таймера повторно, пока он не завершился).
        QTimer::singleShot(0, button, [this, button]() {
            m_busy = false;
            button->click();
        });
    }

    QVector<DialogStep> m_steps;
    QStringList m_seen;
    QTimer m_timer;
    bool m_busy = false;
};

/// Снимок окна в каталог SEDM_SCREENSHOT_DIR (если переменная задана).
inline void screenshot(QWidget* window, const QString& name)
{
    QTest::qWait(150);   // отложенные события компоновки
    const QString dir = qEnvironmentVariable("SEDM_SCREENSHOT_DIR");
    if (!dir.isEmpty() && window)
        window->grab().save(QDir(dir).filePath(name + QStringLiteral(".png")));
}

}  // namespace sedm::test
