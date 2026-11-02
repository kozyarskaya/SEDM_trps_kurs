/**
 * @file SetupWizard.cpp
 * @brief Реализация мастера первичной настройки и окна входа администратора.
 */
#include "SetupWizard.h"
#include "AppPaths.h"
#include "AuditService.h"
#include "AuthService.h"
#include "Database.h"
#include "DbError.h"
#include "LogoWidget.h"
#include "Messages.h"
#include "SchemaInstaller.h"
#include "Theme.h"

#include <QApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

namespace {
/// Пароль роли приложения: 16 символов из букв и цифр (без похожих 0/O, 1/l).
QString generatePassword()
{
    static const QString alphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789");
    QString s;
    for (int i = 0; i < 16; ++i)
        s += alphabet[QRandomGenerator::system()->bounded(alphabet.size())];
    return s;
}

QWidget* page(const QString& title, const QString& text, QVBoxLayout** content)
{
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(12);
    auto* t = new QLabel(title);
    t->setProperty("role", "title");
    l->addWidget(t);
    if (!text.isEmpty()) {
        auto* d = new QLabel(text);
        d->setWordWrap(true);
        d->setProperty("role", "muted");
        l->addWidget(d);
    }
    *content = l;
    return w;
}
}  // namespace

SetupWizard::SetupWizard(Database& db, const ConnectionSettings& current, Start start, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle(QStringLiteral("Первичная настройка СЭДМ"));
    setMinimumSize(720, 620);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 18);
    layout->setSpacing(16);
    auto* head = new QHBoxLayout;
    head->addWidget(new ui::LogoWidget(false, 48));
    auto* name = new QLabel(QStringLiteral("<b>СЭДМ</b> — программная система экспертной дегустации мёда"));
    head->addWidget(name, 1);
    layout->addLayout(head);

    m_pages = new QStackedWidget;
    QVBoxLayout* c = nullptr;

    // 0. Выбор режима
    m_pages->addWidget(page(QStringLiteral("С чего начнём?"), QString(), &c));
    m_modeInstall = new QRadioButton(QStringLiteral("Первичная настройка сервера"));
    auto* installText = new QLabel(QStringLiteral("Сервер PostgreSQL 14 или новее установлен, база данных СЭДМ ещё "
                                                  "не создана. Понадобится пароль суперпользователя postgres."));
    auto* modeConnect = new QRadioButton(QStringLiteral("Подключиться к уже настроенной базе СЭДМ"));
    auto* connectText = new QLabel(QStringLiteral("База создана ранее (например, на другом компьютере администратора). "
                                                  "Понадобятся адрес сервера и пароль учётной записи приложения."));
    for (QLabel* l : {installText, connectText}) {
        l->setWordWrap(true);
        l->setProperty("role", "muted");
        l->setContentsMargins(28, 0, 0, 8);
    }
    m_modeInstall->setChecked(true);
    c->addWidget(m_modeInstall);
    c->addWidget(installText);
    c->addWidget(modeConnect);
    c->addWidget(connectText);
    c->addStretch(1);

    // 1. Первичная настройка сервера
    m_pages->addWidget(page(QStringLiteral("Первичная настройка сервера"),
                            QStringLiteral("Будут созданы учётная запись приложения (пароль по SCRAM-SHA-256), база "
                                           "данных и схема sedm. Учётной записи приложения будет доступна только "
                                           "схема sedm. Пароль суперпользователя нигде не сохраняется."), &c));
    auto* f1 = new QFormLayout;
    m_iHost = new QLineEdit(current.host);
    m_iPort = new QSpinBox;
    m_iPort->setRange(1, 65535);
    m_iPort->setValue(current.port);
    m_iSuper = new QLineEdit(QStringLiteral("postgres"));
    m_iSuperPass = new QLineEdit;
    m_iSuperPass->setEchoMode(QLineEdit::Password);
    m_iDatabase = new QLineEdit(current.database);
    m_iRole = new QLineEdit(current.user);
    m_iRolePass = new QLineEdit(generatePassword());
    auto* regenerate = new QPushButton(QStringLiteral("Другой"));
    auto* passRow = new QHBoxLayout;
    passRow->addWidget(m_iRolePass, 1);
    passRow->addWidget(regenerate);
    f1->addRow(QStringLiteral("Адрес сервера:"), m_iHost);
    f1->addRow(QStringLiteral("Порт:"), m_iPort);
    f1->addRow(QStringLiteral("Суперпользователь:"), m_iSuper);
    f1->addRow(QStringLiteral("Пароль суперпользователя:"), m_iSuperPass);
    f1->addRow(QStringLiteral("Имя базы данных:"), m_iDatabase);
    f1->addRow(QStringLiteral("Учётная запись приложения:"), m_iRole);
    f1->addRow(QStringLiteral("Пароль учётной записи:"), passRow);
    c->addLayout(f1);
    c->addStretch(1);
    connect(regenerate, &QPushButton::clicked, this, [this]() { m_iRolePass->setText(generatePassword()); });

    // 2. Подключение к настроенной базе
    m_pages->addWidget(page(QStringLiteral("Подключение к базе СЭДМ"), QString(), &c));
    auto* f2 = new QFormLayout;
    m_cHost = new QLineEdit(current.host);
    m_cPort = new QSpinBox;
    m_cPort->setRange(1, 65535);
    m_cPort->setValue(current.port);
    m_cDatabase = new QLineEdit(current.database);
    m_cUser = new QLineEdit(current.user);
    m_cPass = new QLineEdit(current.password);
    m_cPass->setEchoMode(QLineEdit::Password);
    f2->addRow(QStringLiteral("Адрес сервера:"), m_cHost);
    f2->addRow(QStringLiteral("Порт:"), m_cPort);
    f2->addRow(QStringLiteral("База данных:"), m_cDatabase);
    f2->addRow(QStringLiteral("Учётная запись приложения:"), m_cUser);
    f2->addRow(QStringLiteral("Пароль:"), m_cPass);
    c->addLayout(f2);
    c->addStretch(1);

    // 3. Пароль администратора
    m_pages->addWidget(page(QStringLiteral("Пароль администратора"),
                            QStringLiteral("Придумайте пароль для входа в программу администратора: не менее %1 "
                                           "символов, хотя бы одна буква и одна цифра. В базе данных хранится только "
                                           "его хэш (bcrypt).").arg(AuthService::kMinPasswordLength), &c));
    auto* f3 = new QFormLayout;
    m_admin1 = new QLineEdit;
    m_admin2 = new QLineEdit;
    m_admin1->setEchoMode(QLineEdit::Password);
    m_admin2->setEchoMode(QLineEdit::Password);
    f3->addRow(QStringLiteral("Пароль:"), m_admin1);
    f3->addRow(QStringLiteral("Повторите пароль:"), m_admin2);
    c->addLayout(f3);
    c->addStretch(1);

    // 4. Готово
    m_pages->addWidget(page(QStringLiteral("Настройка завершена"), QString(), &c));
    m_doneText = new QLabel;
    m_doneText->setWordWrap(true);
    m_doneText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    c->addWidget(m_doneText);
    c->addStretch(1);

    layout->addWidget(m_pages, 1);
    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(QStringLiteral("Отмена"));
    m_back = new QPushButton(QStringLiteral("← Назад"));
    m_next = new QPushButton(QStringLiteral("Далее →"));
    m_next->setProperty("kind", "primary");
    m_next->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addStretch(1);
    buttons->addWidget(m_back);
    buttons->addWidget(m_next);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_back, &QPushButton::clicked, this, &SetupWizard::back);
    connect(m_next, &QPushButton::clicked, this, &SetupWizard::next);

    goTo(start == Start::Install ? kInstall : start == Start::AdminPassword ? kPassword : kChoose);
}

void SetupWizard::goTo(Page page)
{
    if (page == kDone) {
        const ConnectionSettings& s = m_db.settings();
        QString text = QStringLiteral("Программа подключена к базе «%1» на сервере %2:%3.")
                           .arg(s.database, s.host).arg(s.port);
        if (m_installed)
            text += QStringLiteral("\n\nПароль учётной записи приложения «%1»: %2\n\nОн понадобится при настройке "
                                   "компьютеров дегустаторов (окно «Подключение к серверу»). Запишите его; на этом "
                                   "компьютере он сохранён в защищённом виде.").arg(s.user, m_appPassword);
        m_doneText->setText(text);
    }
    m_page = page;
    m_pages->setCurrentIndex(page);
    m_back->setVisible(page == kInstall || page == kConnect);
    m_next->setText(page == kInstall ? QStringLiteral("Настроить сервер")
                    : page == kConnect ? QStringLiteral("Подключиться")
                    : page == kPassword ? QStringLiteral("Сохранить пароль")
                    : page == kDone ? QStringLiteral("Начать работу")
                                    : QStringLiteral("Далее →"));
}

void SetupWizard::back()
{
    goTo(kChoose);
}

void SetupWizard::next()
{
    switch (m_page) {
    case kChoose:
        goTo(m_modeInstall->isChecked() ? kInstall : kConnect);
        break;
    case kInstall:
        if (install())
            finish();
        break;
    case kConnect:
        if (connectExisting())
            finish();
        break;
    case kPassword:
        if (createAdmin())
            goTo(kDone);
        break;
    case kDone:
        accept();
        break;
    }
}

bool SetupWizard::install()
{
    ConnectionSettings super;
    super.host = m_iHost->text().trimmed();
    super.port = m_iPort->value();
    super.user = m_iSuper->text().trimmed();
    super.password = m_iSuperPass->text();
    super.database = QStringLiteral("postgres");
    ConnectionSettings app = super;
    app.user = m_iRole->text().trimmed();
    app.password = m_iRolePass->text();
    app.database = m_iDatabase->text().trimmed();
    if (super.password.isEmpty() || app.password.size() < 8 || !app.isComplete()) {
        ui::Messages::warning(this, QStringLiteral("Заполните поля"),
                              QStringLiteral("Укажите пароль суперпользователя, имя базы, учётную запись приложения "
                                             "и её пароль (не короче 8 символов)."));
        return false;
    }
    const bool ok = ui::Messages::guard(this, [&]() {
        SchemaInstaller::install(super, app);
        m_db.open(app);
        app.save(*AppPaths::settings());   // пароль приложения — в защищённом виде (DPAPI)
    }, QStringLiteral("Сервер не настроен"));
    if (ok) {
        m_appPassword = app.password;
        m_installed = true;
    }
    return ok;
}

bool SetupWizard::connectExisting()
{
    ConnectionSettings app;
    app.host = m_cHost->text().trimmed();
    app.port = m_cPort->value();
    app.database = m_cDatabase->text().trimmed();
    app.user = m_cUser->text().trimmed();
    app.password = m_cPass->text();
    bool installed = false;
    const bool ok = ui::Messages::guard(this, [&]() {
        m_db.open(app);
        installed = SchemaInstaller::isInstalled(m_db);
        if (installed)
            app.save(*AppPaths::settings());
    }, QStringLiteral("Нет подключения"));
    if (ok && !installed)
        ui::Messages::warning(this, QStringLiteral("База СЭДМ не найдена"),
                              QStringLiteral("Подключение установлено, но схема данных СЭДМ в этой базе не создана. "
                                             "Выберите «Первичная настройка сервера»."));
    return ok && installed;
}

void SetupWizard::finish()
{
    // после подключения: если пароль администратора ещё не задан — задать его
    bool exists = true;
    ui::Messages::guard(this, [&]() {
        AuditService audit(m_db, AppPaths::pendingAuditFile());
        exists = AuthService(m_db, audit).adminExists();
    });
    if (!exists) {
        goTo(kPassword);
        return;
    }
    goTo(kDone);
}

bool SetupWizard::createAdmin()
{
    if (m_admin1->text() != m_admin2->text()) {
        ui::Messages::warning(this, QStringLiteral("Пароли не совпадают"), QStringLiteral("Повторите пароль точно."));
        return false;
    }
    return ui::Messages::guard(this, [&]() {
        AuditService audit(m_db, AppPaths::pendingAuditFile());
        AuthService(m_db, audit).createAdmin(m_admin1->text());
    }, QStringLiteral("Пароль не сохранён"));
}

AdminLoginDialog::AdminLoginDialog(AuthService& auth, QWidget* parent)
    : QDialog(parent), m_auth(auth)
{
    setWindowTitle(QStringLiteral("Вход администратора — СЭДМ"));
    setMinimumWidth(460);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 26, 32, 22);
    layout->setSpacing(12);
    auto* head = new QHBoxLayout;
    head->addWidget(new ui::LogoWidget(false, 56));
    auto* title = new QLabel(QStringLiteral("Программа\nадминистратора"));
    title->setProperty("role", "title");
    head->addWidget(title, 1);
    layout->addLayout(head);
    auto* label = new QLabel(QStringLiteral("Пароль администратора"));
    m_password = new QLineEdit;
    m_password->setEchoMode(QLineEdit::Password);
    label->setBuddy(m_password);
    layout->addWidget(label);
    layout->addWidget(m_password);
    m_message = new QLabel;
    m_message->setProperty("role", "error");
    m_message->setWordWrap(true);
    m_message->hide();
    layout->addWidget(m_message);
    m_enter = new QPushButton(QStringLiteral("Войти"));
    m_enter->setProperty("kind", "primary");
    m_enter->setDefault(true);
    layout->addWidget(m_enter);
    auto* hint = new QLabel(QStringLiteral("Если пароль утерян, восстановление доступа описано в руководстве "
                                           "системного программиста."));
    hint->setProperty("role", "caption");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &AdminLoginDialog::tick);
    connect(m_enter, &QPushButton::clicked, this, &AdminLoginDialog::tryLogin);
}

void AdminLoginDialog::tryLogin()
{
    AuthService::AdminLogin result;
    if (!ui::Messages::guard(this, [&]() { result = m_auth.loginAdmin(m_password->text()); }))
        return;
    if (result.status == LoginStatus::Ok) {
        accept();
        return;
    }
    m_password->clear();
    if (result.status == LoginStatus::Locked) {
        m_lockLeft = result.secondsLeft;
        m_enter->setEnabled(false);
        m_password->setEnabled(false);
        tick();
        m_timer->start();
        return;
    }
    m_message->setText(QStringLiteral("Неверный пароль. После пяти ошибок подряд вход блокируется на 60 секунд."));
    m_message->show();
}

void AdminLoginDialog::tick()
{
    if (m_lockLeft <= 0) {
        m_timer->stop();
        m_enter->setEnabled(true);
        m_password->setEnabled(true);
        m_password->setFocus();
        m_message->hide();
        return;
    }
    m_message->setText(QStringLiteral("Вход временно заблокирован после пяти неверных попыток. Повторите через %1 с.")
                           .arg(m_lockLeft));
    m_message->show();
    --m_lockLeft;
}

}  // namespace sedm::admin
