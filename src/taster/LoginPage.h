/**
 * @file LoginPage.h
 * @brief Окно входа дегустатора: фамилия, PIN-код, цифровая клавиатура.
 */
#pragma once

#include "AuthService.h"

#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace sedm::taster {

class PinPad;

/// Состояния «Вход» и «Вход заблокирован» диаграммы состояний интерфейса
/// дегустатора: проверка логина и PIN-кода, блокировка после пяти ошибок.
class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget* parent = nullptr);

    /// Показ даты и времени сеанса (entry / показать дату и время сеанса).
    void setSessionText(const QString& text);
    /// Подготовка к новому входу: поля очищаются, фокус — в поле фамилии.
    void reset();
    void showError(const QString& text);
    /// Состояние «Вход заблокирован»: обратный отсчёт до снятия блокировки.
    void lockFor(int seconds);

signals:
    /// Пользователь нажал «Войти» (фамилия и четыре цифры PIN-кода введены).
    void loginRequested(const QString& login, const QString& pin);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void submit();
    void appendDigit(QChar digit);
    void tick();
    void showMessage(const QString& text);

    QLabel* m_session;
    QLineEdit* m_login;
    QLineEdit* m_pin;
    PinPad* m_pad;
    QPushButton* m_enter;
    QLabel* m_message;
    QTimer m_lockTimer;
    int m_lockLeft = 0;
};

}  // namespace sedm::taster
