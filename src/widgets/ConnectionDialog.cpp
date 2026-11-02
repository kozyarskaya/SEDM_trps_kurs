/**
 * @file ConnectionDialog.cpp
 * @brief Реализация окна параметров подключения.
 */
#include "ConnectionDialog.h"
#include "Database.h"
#include "DbError.h"
#include "SchemaInstaller.h"
#include "Theme.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace sedm::ui {

ConnectionDialog::ConnectionDialog(const ConnectionSettings& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Подключение к серверу базы данных"));
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);

    auto* intro = new QLabel(QStringLiteral(
        "Укажите сервер PostgreSQL, на котором размещена база данных СЭДМ, и учётную запись "
        "приложения. Эти сведения сообщает администратор дегустации."));
    intro->setWordWrap(true);
    intro->setProperty("role", "muted");
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    m_host = new QLineEdit(current.host);
    m_host->setPlaceholderText(QStringLiteral("например, 192.168.1.10 или localhost"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(current.port);
    m_database = new QLineEdit(current.database);
    m_user = new QLineEdit(current.user);
    m_password = new QLineEdit(current.password);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Адрес сервера:"), m_host);
    form->addRow(QStringLiteral("Порт:"), m_port);
    form->addRow(QStringLiteral("База данных:"), m_database);
    form->addRow(QStringLiteral("Учётная запись:"), m_user);
    form->addRow(QStringLiteral("Пароль:"), m_password);
    layout->addLayout(form);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QDialogButtonBox;
    auto* test = buttons->addButton(QStringLiteral("Проверить подключение"), QDialogButtonBox::ActionRole);
    auto* save = buttons->addButton(QStringLiteral("Сохранить"), QDialogButtonBox::AcceptRole);
    save->setProperty("kind", "primary");
    save->setDefault(true);
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(test, &QPushButton::clicked, this, &ConnectionDialog::testConnection);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ConnectionSettings ConnectionDialog::settings() const
{
    ConnectionSettings s;
    s.host = m_host->text().trimmed();
    s.port = m_port->value();
    s.database = m_database->text().trimmed();
    s.user = m_user->text().trimmed();
    s.password = m_password->text();
    return s;
}

void ConnectionDialog::testConnection()
{
    const ConnectionSettings s = settings();
    if (!s.isComplete()) {
        Theme::setProp(m_status, "role", "error");
        m_status->setText(QStringLiteral("Заполните все поля."));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString text;
    bool ok = false;
    try {
        Database db(QStringLiteral("sedm_connection_test"));
        db.open(s);
        ok = SchemaInstaller::isInstalled(db);
        text = ok ? QStringLiteral("Подключение установлено, база данных СЭДМ найдена.")
                  : QStringLiteral("Подключение установлено, но схема данных СЭДМ в этой базе не создана. "
                                   "Выполните первичную настройку в программе администратора.");
    } catch (const DbError& e) {
        text = e.message();
    }
    QApplication::restoreOverrideCursor();
    Theme::setProp(m_status, "role", ok ? "ok" : "error");
    m_status->setText(text);
}

void ConnectionDialog::accept()
{
    if (!settings().isComplete()) {
        Theme::setProp(m_status, "role", "error");
        m_status->setText(QStringLiteral("Заполните все поля."));
        return;
    }
    QDialog::accept();
}

}  // namespace sedm::ui
