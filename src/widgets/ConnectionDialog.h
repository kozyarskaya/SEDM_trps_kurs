/**
 * @file ConnectionDialog.h
 * @brief Окно параметров подключения к серверу базы данных СЭДМ.
 */
#pragma once

#include "ConnectionSettings.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QSpinBox;

namespace sedm::ui {

/// Параметры подключения клиента к серверу PostgreSQL (п. 4.1.2–4.1.3 ТЗ):
/// адрес, порт, база, учётная запись приложения и её пароль. Кнопка
/// «Проверить» пробует подключиться и сообщает результат понятным текстом.
class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectionDialog(const ConnectionSettings& current, QWidget* parent = nullptr);
    ConnectionSettings settings() const;

private slots:
    void testConnection();
    void accept() override;

private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QLineEdit* m_database;
    QLineEdit* m_user;
    QLineEdit* m_password;
    QLabel* m_status;
};

}  // namespace sedm::ui
