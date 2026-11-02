/**
 * @file Messages.cpp
 * @brief Реализация окон сообщений.
 */
#include "Messages.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"
#include "Icons.h"
#include "Theme.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::ui {

namespace {

enum class Kind { Info, Warning, Error, Question };

/// Окно сообщения в оформлении программы: значок, текст, крупные кнопки.
/// Своё окно вместо QMessageBox: единый вид с темой Г1 и работа в
/// автоматических тестах интерфейса (платформа offscreen).
class MessageDialog : public QDialog {
public:
    MessageDialog(QWidget* parent, Kind kind, const QString& title, const QString& text,
                  const QStringList& buttons, int defaultButton, int escapeButton)
        : QDialog(parent), m_escape(escapeButton)
    {
        setWindowTitle(title);
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        setObjectName(QStringLiteral("messageDialog"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 22, 24, 18);
        layout->setSpacing(18);

        auto* body = new QHBoxLayout;
        body->setSpacing(16);
        static const char* icons[] = {"msg_info", "msg_warning", "msg_error", "msg_question"};
        const QColor colors[] = {palette::kBrown, palette::kAmber, palette::kRed, palette::kBrown};
        auto* icon = new QLabel;
        const int k = static_cast<int>(kind);
        icon->setPixmap(Icons::pixmap(QString::fromLatin1(icons[k]), colors[k], 40, devicePixelRatioF()));
        icon->setAlignment(Qt::AlignTop);
        body->addWidget(icon);
        auto* message = new QLabel(text);
        message->setObjectName(QStringLiteral("messageText"));
        message->setTextFormat(Qt::PlainText);
        message->setWordWrap(true);
        message->setMinimumWidth(360);
        message->setMaximumWidth(560);
        message->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->addWidget(message, 1);
        layout->addLayout(body);

        auto* row = new QHBoxLayout;
        row->addStretch(1);
        for (int i = 0; i < buttons.size(); ++i) {
            auto* b = new QPushButton(buttons[i]);
            b->setMinimumWidth(120);
            if (i == defaultButton) {
                b->setProperty("kind", "primary");
                b->setDefault(true);
                b->setFocus();
            } else {
                b->setAutoDefault(false);
            }
            connect(b, &QPushButton::clicked, this, [this, i]() { done(i + 1); });
            row->addWidget(b);
        }
        layout->addLayout(row);
    }

    /// Номер нажатой кнопки; −1 — окно закрыто без выбора.
    int run() { return exec() - 1; }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            done(m_escape >= 0 ? m_escape + 1 : 0);
            return;
        }
        QDialog::keyPressEvent(event);
    }

private:
    int m_escape;
};

int show(QWidget* parent, Kind kind, const QString& title, const QString& text, const QStringList& buttons,
         int defaultButton, int escapeButton)
{
    MessageDialog dlg(parent, kind, title, text, buttons, defaultButton, escapeButton);
    return dlg.run();
}

}  // namespace

void Messages::info(QWidget* parent, const QString& title, const QString& text)
{
    show(parent, Kind::Info, title, text, {QStringLiteral("Понятно")}, 0, 0);
}

void Messages::warning(QWidget* parent, const QString& title, const QString& text)
{
    show(parent, Kind::Warning, title, text, {QStringLiteral("Понятно")}, 0, 0);
}

void Messages::error(QWidget* parent, const QString& title, const QString& text)
{
    show(parent, Kind::Error, title, text, {QStringLiteral("Закрыть")}, 0, 0);
}

bool Messages::confirm(QWidget* parent, const QString& title, const QString& text,
                       const QString& yes, const QString& no, bool dangerous)
{
    return show(parent, dangerous ? Kind::Warning : Kind::Question, title, text, {yes, no},
                dangerous ? 1 : 0, 1) == 0;
}

int Messages::choose(QWidget* parent, const QString& title, const QString& text, const QStringList& buttons,
                     int defaultButton, int escapeButton)
{
    return show(parent, Kind::Question, title, text, buttons, defaultButton, escapeButton);
}

bool Messages::guard(QWidget* parent, Database& db, const std::function<void()>& action, const QString& title)
{
    return guard(parent, [&]() {
        try {
            action();
        } catch (const DbError& e) {
            // связь пропала (перезапуск сервера, сбой сети): переподключиться и повторить
            if (e.kind() != DbError::Kind::Connection || !db.ping())
                throw;
            action();
        }
    }, title);
}

bool Messages::guard(QWidget* parent, const std::function<void()>& action, const QString& title)
{
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        action();
        QApplication::restoreOverrideCursor();
        return true;
    } catch (const DomainError& e) {
        QApplication::restoreOverrideCursor();
        warning(parent, title, e.message());
    } catch (const DbError& e) {
        QApplication::restoreOverrideCursor();
        error(parent, e.kind() == DbError::Kind::Connection ? QStringLiteral("Нет связи с сервером") : title,
              e.message());
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        error(parent, title, QStringLiteral("Непредвиденная ошибка: %1").arg(QString::fromLocal8Bit(e.what())));
    }
    return false;
}

}  // namespace sedm::ui
