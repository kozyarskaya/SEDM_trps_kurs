/**
 * @file LoginPage.cpp
 * @brief Реализация окна входа дегустатора.
 */
#include "LoginPage.h"
#include "LogoWidget.h"
#include "PinGenerator.h"
#include "PinPad.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::taster {

LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 24, 24, 24);
    outer->addStretch(1);

    // Карточка входа в две колонки: поля слева, цифровая клавиатура справа —
    // помещается на экране 1366×768 без прокрутки
    auto* card = new QFrame;
    card->setProperty("card", "true");
    card->setMaximumWidth(980);
    auto* columns = new QHBoxLayout(card);
    columns->setContentsMargins(36, 28, 36, 28);
    columns->setSpacing(36);
    auto* layout = new QVBoxLayout;
    layout->setSpacing(12);
    columns->addLayout(layout, 1);

    auto* head = new QHBoxLayout;
    head->setSpacing(16);
    head->addWidget(new ui::LogoWidget(false, 64));
    auto* title = new QLabel(QStringLiteral("Экспертная\nдегустация мёда"));
    title->setProperty("role", "title");
    head->addWidget(title, 1);
    layout->addLayout(head);
    m_session = new QLabel;
    m_session->setProperty("role", "muted");
    m_session->setWordWrap(true);
    layout->addWidget(m_session);
    layout->addSpacing(6);

    auto* loginLabel = new QLabel(QStringLiteral("Фамилия"));
    layout->addWidget(loginLabel);
    m_login = new QLineEdit;
    m_login->setObjectName(QStringLiteral("loginField"));
    m_login->setPlaceholderText(QStringLiteral("например, Иванова"));
    m_login->setAccessibleName(QStringLiteral("Фамилия (логин)"));
    loginLabel->setBuddy(m_login);
    layout->addWidget(m_login);

    auto* pinLabel = new QLabel(QStringLiteral("PIN-код (4 цифры)"));
    layout->addWidget(pinLabel);
    m_pin = new QLineEdit;
    m_pin->setObjectName(QStringLiteral("pinField"));
    m_pin->setEchoMode(QLineEdit::Password);
    m_pin->setMaxLength(PinGenerator::kPinLength);
    m_pin->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,4}")), m_pin));
    m_pin->setAccessibleName(QStringLiteral("PIN-код"));
    pinLabel->setBuddy(m_pin);
    layout->addWidget(m_pin);
    layout->addSpacing(6);

    m_enter = new QPushButton(QStringLiteral("Войти"));
    m_enter->setProperty("kind", "primary");
    m_enter->setProperty("big", true);
    m_enter->setDefault(true);
    layout->addWidget(m_enter);

    m_message = new QLabel;
    m_message->setProperty("role", "error");
    m_message->setWordWrap(true);
    m_message->hide();
    layout->addWidget(m_message);
    layout->addStretch(1);

    m_pad = new PinPad;
    m_pad->setMinimumSize(300, 300);
    columns->addWidget(m_pad, 1);

    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addStretch(1);

    connect(m_pad, &PinPad::digitPressed, this, &LoginPage::appendDigit);
    connect(m_pad, &PinPad::backspacePressed, this, [this]() { m_pin->backspace(); });
    connect(m_pad, &PinPad::clearPressed, m_pin, &QLineEdit::clear);
    connect(m_enter, &QPushButton::clicked, this, &LoginPage::submit);
    connect(m_login, &QLineEdit::returnPressed, this, [this]() { m_pin->setFocus(); });
    connect(m_pin, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(m_login, &QLineEdit::textEdited, m_message, &QLabel::hide);
    connect(m_pin, &QLineEdit::textEdited, m_message, &QLabel::hide);
    m_lockTimer.setInterval(1000);
    connect(&m_lockTimer, &QTimer::timeout, this, &LoginPage::tick);
}

void LoginPage::setSessionText(const QString& text)
{
    m_session->setText(text);
}

void LoginPage::reset()
{
    // следующий дегустатор за этим компьютером не видит фамилию предыдущего
    m_login->clear();
    m_pin->clear();
    m_message->hide();
    m_login->setFocus();
}

void LoginPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_login->text().isEmpty())
        m_login->setFocus();
    else
        m_pin->setFocus();
}

void LoginPage::appendDigit(QChar digit)
{
    if (m_pin->text().size() < PinGenerator::kPinLength)
        m_pin->setText(m_pin->text() + digit);
    m_message->hide();
}

void LoginPage::keyPressEvent(QKeyEvent* event)
{
    // цифры с клавиатуры попадают в PIN-код, даже если фокус не в поле
    const QString t = event->text();
    if (t.size() == 1 && t[0].isDigit() && !m_login->hasFocus() && !m_pin->hasFocus()) {
        appendDigit(t[0]);
        return;
    }
    QWidget::keyPressEvent(event);
}

void LoginPage::submit()
{
    if (m_lockLeft > 0)
        return;
    const QString login = m_login->text().simplified();
    if (login.isEmpty()) {
        showError(QStringLiteral("Введите вашу фамилию."));
        m_login->setFocus();
        return;
    }
    if (!PinGenerator::isWellFormed(m_pin->text())) {
        showError(QStringLiteral("PIN-код — четыре цифры. Наберите его на клавиатуре."));
        m_pin->setFocus();
        return;
    }
    emit loginRequested(login, m_pin->text());
}

void LoginPage::showMessage(const QString& text)
{
    // высота по числу строк текста: иначе в карточке входа видна только часть сообщения
    m_message->setText(text);
    m_message->setMinimumHeight(m_message->heightForWidth(qMax(m_message->width(), 300)));
    m_message->show();
}

void LoginPage::showError(const QString& text)
{
    showMessage(text);
    m_pin->clear();
}

void LoginPage::lockFor(int seconds)
{
    m_lockLeft = qMax(1, seconds);
    m_login->setEnabled(false);
    m_pin->setEnabled(false);
    m_pad->setEnabled(false);
    m_enter->setEnabled(false);
    tick();
    m_lockTimer.start();
}

void LoginPage::tick()
{
    if (m_lockLeft <= 0) {
        m_lockTimer.stop();
        m_login->setEnabled(true);
        m_pin->setEnabled(true);
        m_pad->setEnabled(true);
        m_enter->setEnabled(true);
        m_message->hide();
        m_pin->setFocus();
        return;
    }
    showMessage(QStringLiteral("Вход временно заблокирован после пяти неверных попыток.\n"
                               "Повторите через %1 с.").arg(m_lockLeft));
    --m_lockLeft;
}

}  // namespace sedm::taster
