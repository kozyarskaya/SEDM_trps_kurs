/**
 * @file AuditDialog.cpp
 * @brief Реализация окон журнала действий и смены пароля.
 */
#include "AuditDialog.h"
#include "AdminPage.h"
#include "AuditService.h"
#include "AuthService.h"
#include "Icons.h"
#include "Messages.h"
#include "RecordFilter.h"
#include "Theme.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

AuditDialog::AuditDialog(AuditService& audit, QWidget* parent)
    : QDialog(parent), m_audit(audit)
{
    setWindowTitle(QStringLiteral("Журнал действий"));
    resize(1100, 640);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(10);
    auto* top = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Поиск по пользователю, действию, подробностям"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(ui::Icons::icon(QStringLiteral("search"), ui::palette::kMuted), QLineEdit::LeadingPosition);
    auto* exportButton = new QPushButton(QStringLiteral("Выгрузить в CSV…"));
    exportButton->setProperty("kind", "primary");
    top->addWidget(m_search, 1);
    top->addWidget(exportButton);
    layout->addLayout(top);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("Дата и время"), QStringLiteral("Пользователь"),
                                        QStringLiteral("Роль"), QStringLiteral("Рабочее место"),
                                        QStringLiteral("Действие"), QStringLiteral("Подробности")});
    m_filter = new RecordFilter(this);
    m_filter->setSourceModel(m_model);
    m_table = new QTableView;
    m_table->setModel(m_filter);
    setupTable(m_table);
    layout->addWidget(m_table, 1);
    auto* buttons = new QDialogButtonBox;
    buttons->addButton(QStringLiteral("Закрыть"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_search, &QLineEdit::textChanged, m_filter, &RecordFilter::setText);
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportCsv(m_audit, this); });
    load();
}

void AuditDialog::load()
{
    QVector<AuditEntry> entries;
    if (!ui::Messages::guard(this, [&]() { entries = m_audit.list(kShownEntries); }))
        return;
    m_model->removeRows(0, m_model->rowCount());
    for (const AuditEntry& e : entries)
        m_model->appendRow({makeItem(e.at().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")), e.at()),
                            makeItem(e.actor()), makeItem(e.role()), makeItem(e.workstation()),
                            makeItem(e.action()), makeItem(e.details())});
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
}

void AuditDialog::exportCsv(AuditService& audit, QWidget* parent)
{
    const QString name = QStringLiteral("Журнал действий %1.csv").arg(QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy")));
    const QString path = QFileDialog::getSaveFileName(parent, QStringLiteral("Выгрузка журнала действий"), name,
                                                      QStringLiteral("Файл CSV (*.csv)"));
    if (path.isEmpty())
        return;
    int count = 0;
    if (ui::Messages::guard(parent, [&]() { count = audit.exportCsv(path); }, QStringLiteral("Журнал не выгружен")))
        ui::Messages::info(parent, QStringLiteral("Журнал выгружен"),
                           QStringLiteral("Записей: %1. Файл: %2 (UTF-8, разделитель «;»).")
                               .arg(count).arg(QDir::toNativeSeparators(path)));
}

ChangePasswordDialog::ChangePasswordDialog(AuthService& auth, QWidget* parent)
    : QDialog(parent), m_auth(auth)
{
    setWindowTitle(QStringLiteral("Смена пароля администратора"));
    setMinimumWidth(460);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);
    auto* form = new QFormLayout;
    m_current = new QLineEdit;
    m_new = new QLineEdit;
    m_repeat = new QLineEdit;
    for (QLineEdit* e : {m_current, m_new, m_repeat})
        e->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Текущий пароль:"), m_current);
    form->addRow(QStringLiteral("Новый пароль:"), m_new);
    form->addRow(QStringLiteral("Повторите пароль:"), m_repeat);
    layout->addLayout(form);
    auto* hint = new QLabel(QStringLiteral("Не менее %1 символов, хотя бы одна буква и одна цифра.")
                                .arg(AuthService::kMinPasswordLength));
    hint->setProperty("role", "muted");
    layout->addWidget(hint);
    auto* buttons = new QDialogButtonBox;
    auto* ok = buttons->addButton(QStringLiteral("Сменить пароль"), QDialogButtonBox::AcceptRole);
    ok->setProperty("kind", "primary");
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ChangePasswordDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ChangePasswordDialog::accept()
{
    if (m_new->text() != m_repeat->text()) {
        ui::Messages::warning(this, QStringLiteral("Пароли не совпадают"),
                              QStringLiteral("Новый пароль и его повтор различаются."));
        return;
    }
    if (ui::Messages::guard(this, [&]() { m_auth.changeAdminPassword(m_current->text(), m_new->text()); },
                            QStringLiteral("Пароль не изменён"))) {
        ui::Messages::info(this, QStringLiteral("Пароль изменён"), QStringLiteral("Новый пароль администратора сохранён."));
        QDialog::accept();
    }
}

}  // namespace sedm::admin
