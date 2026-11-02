/**
 * @file ParticipantsPage.cpp
 * @brief Реализация окна «Участники и номинации».
 */
#include "ParticipantsPage.h"
#include "Icons.h"
#include "ImportDialog.h"
#include "Messages.h"
#include "ParticipantDialog.h"
#include "RecordFilter.h"
#include "RegistryService.h"
#include "Theme.h"

#include <QHash>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

namespace {
QString askName(QWidget* parent, const QString& title, const QString& current, bool* ok)
{
    return QInputDialog::getText(parent, title, QStringLiteral("Название номинации:"), QLineEdit::Normal,
                                 current, ok);
}
}  // namespace

ParticipantsPage::ParticipantsPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Участники и номинации"), parent)
{
    auto* importButton = addAction(QStringLiteral("Импорт реестра…"), false, QStringLiteral("import"));
    auto* addButton = addAction(QStringLiteral("+ Добавить участника"), true);
    connect(importButton, &QPushButton::clicked, this, &ParticipantsPage::importRegistry);
    connect(addButton, &QPushButton::clicked, this, &ParticipantsPage::addParticipant);

    auto* splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);

    // --- справочник номинаций
    auto* nomCard = new QFrame;
    nomCard->setProperty("card", "true");
    auto* n = new QVBoxLayout(nomCard);
    n->setContentsMargins(14, 14, 14, 14);
    auto* nomTitle = new QLabel(QStringLiteral("Номинации"));
    QFont f = nomTitle->font();
    f.setBold(true);
    nomTitle->setFont(f);
    n->addWidget(nomTitle);
    m_nominations = new QListWidget;
    m_nominations->setAccessibleName(QStringLiteral("Справочник номинаций"));
    n->addWidget(m_nominations, 1);
    // кнопки в две строки: при ширине экрана 1366 в одну строку не помещаются
    auto* nomButtons = new QGridLayout;
    auto* nomAdd = new QPushButton(QStringLiteral("Добавить"));
    auto* nomRename = new QPushButton(QStringLiteral("Переименовать"));
    auto* nomRemove = new QPushButton(QStringLiteral("Удалить"));
    nomRemove->setProperty("kind", "danger");
    nomButtons->addWidget(nomAdd, 0, 0);
    nomButtons->addWidget(nomRename, 0, 1);
    nomButtons->addWidget(nomRemove, 1, 1);
    n->addLayout(nomButtons);
    nomCard->setMinimumWidth(240);
    nomCard->setMaximumWidth(320);
    splitter->addWidget(nomCard);

    // --- реестр участников
    auto* right = new QWidget;
    auto* r = new QVBoxLayout(right);
    r->setContentsMargins(0, 0, 0, 0);
    r->setSpacing(10);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Поиск по ФИО, телефону, e-mail или региону"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(ui::Icons::icon(QStringLiteral("search"), ui::palette::kMuted), QLineEdit::LeadingPosition);
    r->addWidget(m_search);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("ФИО участника"), QStringLiteral("Телефон"),
                                        QStringLiteral("E-mail"), QStringLiteral("Регион"),
                                        QStringLiteral("Номинация")});
    m_filter = new RecordFilter(this);
    m_filter->setSourceModel(m_model);
    m_table = new QTableView;
    m_table->setModel(m_filter);
    setupTable(m_table);
    r->addWidget(m_table, 1);
    auto* rowButtons = new QHBoxLayout;
    rowButtons->addStretch(1);
    auto* edit = new QPushButton(QStringLiteral("Изменить…"));
    auto* remove = new QPushButton(QStringLiteral("Удалить"));
    remove->setProperty("kind", "danger");
    rowButtons->addWidget(edit);
    rowButtons->addWidget(remove);
    r->addLayout(rowButtons);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    body()->addWidget(splitter, 1);

    connect(m_search, &QLineEdit::textChanged, m_filter, &RecordFilter::setText);
    connect(m_nominations, &QListWidget::currentRowChanged, this, [this]() {
        m_filter->setKey(selectedNominationId());
    });
    connect(m_table, &QTableView::doubleClicked, this, &ParticipantsPage::editParticipant);
    connect(edit, &QPushButton::clicked, this, &ParticipantsPage::editParticipant);
    connect(remove, &QPushButton::clicked, this, &ParticipantsPage::removeParticipant);
    connect(nomAdd, &QPushButton::clicked, this, &ParticipantsPage::addNomination);
    connect(nomRename, &QPushButton::clicked, this, &ParticipantsPage::renameNomination);
    connect(nomRemove, &QPushButton::clicked, this, &ParticipantsPage::removeNomination);
}

void ParticipantsPage::refresh()
{
    RegistryService reg(m_ctx.db, m_ctx.audit);
    QVector<Nomination> nominations;
    QVector<Participant> participants;
    if (!run([&]() {
            nominations = reg.nominations();
            participants = reg.participants();
        }, false))
        return;
    QHash<int, QString> nominationName;
    QHash<int, int> perNomination;
    for (const Nomination& nm : nominations)
        nominationName.insert(nm.id(), nm.name());
    for (const Participant& p : participants)
        ++perNomination[p.nominationId()];

    const int currentNomination = selectedNominationId();
    {
        const QSignalBlocker block(m_nominations);
        m_nominations->clear();
        auto* all = new QListWidgetItem(QStringLiteral("Все номинации (%1)").arg(participants.size()));
        all->setData(Qt::UserRole, 0);
        m_nominations->addItem(all);
        for (const Nomination& nm : nominations) {
            auto* item = new QListWidgetItem(QStringLiteral("%1 (%2)").arg(nm.name()).arg(perNomination.value(nm.id())));
            item->setData(Qt::UserRole, nm.id());
            m_nominations->addItem(item);
            if (nm.id() == currentNomination)
                m_nominations->setCurrentItem(item);
        }
        if (!m_nominations->currentItem())
            m_nominations->setCurrentRow(0);
    }
    m_filter->setKey(selectedNominationId());

    m_model->removeRows(0, m_model->rowCount());
    for (const Participant& p : participants) {
        QList<QStandardItem*> row = {makeItem(p.fullName()), makeItem(p.phone()), makeItem(p.email()),
                                     makeItem(p.region()), makeItem(nominationName.value(p.nominationId()))};
        row[0]->setData(p.id(), Qt::UserRole);
        row[0]->setData(p.nominationId(), RecordFilter::kKeyRole);
        m_model->appendRow(row);
    }
    m_table->resizeColumnsToContents();
    setSubtitle(QStringLiteral("%1 участников · %2 номинаций").arg(participants.size()).arg(nominations.size()));
}

int ParticipantsPage::selectedParticipantId() const
{
    const QModelIndex index = m_table->currentIndex();
    if (!index.isValid())
        return 0;
    return m_filter->mapToSource(index).siblingAtColumn(0).data(Qt::UserRole).toInt();
}

int ParticipantsPage::selectedNominationId() const
{
    const QListWidgetItem* item = m_nominations->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

void ParticipantsPage::focusNominations()
{
    m_nominations->setFocus();
}

void ParticipantsPage::importRegistry()
{
    RegistryService reg(m_ctx.db, m_ctx.audit);
    ImportDialog dlg(reg, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
        emit dataChanged();
    }
}

void ParticipantsPage::addParticipant()
{
    RegistryService reg(m_ctx.db, m_ctx.audit);
    Participant p;
    p.setNominationId(selectedNominationId());
    QVector<Nomination> nominations;
    if (!run([&]() { nominations = reg.nominations(); }, false))
        return;
    ParticipantDialog dlg(p, nominations, [&](const Participant& value) { reg.addParticipant(value); }, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
        emit dataChanged();
    }
}

void ParticipantsPage::editParticipant()
{
    const int id = selectedParticipantId();
    if (id == 0) {
        ui::Messages::info(this, QStringLiteral("Участник не выбран"), QStringLiteral("Выберите участника в таблице."));
        return;
    }
    RegistryService reg(m_ctx.db, m_ctx.audit);
    std::optional<Participant> p;
    QVector<Nomination> nominations;
    if (!run([&]() {
            p = reg.participant(id);
            nominations = reg.nominations();
        }, false) || !p)
        return;
    ParticipantDialog dlg(*p, nominations, [&](const Participant& value) { reg.updateParticipant(value); }, this);
    if (dlg.exec() == QDialog::Accepted) {
        refresh();
        emit dataChanged();
    }
}

void ParticipantsPage::removeParticipant()
{
    const int id = selectedParticipantId();
    if (id == 0)
        return;
    const QString name = m_filter->mapToSource(m_table->currentIndex()).siblingAtColumn(0).data().toString();
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление участника"),
                               QStringLiteral("Удалить участника «%1» из реестра?").arg(name),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { RegistryService(m_ctx.db, m_ctx.audit).removeParticipant(id); }))
        refresh();
}

void ParticipantsPage::addNomination()
{
    bool ok = false;
    const QString name = askName(this, QStringLiteral("Новая номинация"), QString(), &ok);
    if (!ok)
        return;
    if (run([&]() { RegistryService(m_ctx.db, m_ctx.audit).addNomination(name); }))
        refresh();
}

void ParticipantsPage::renameNomination()
{
    const int id = selectedNominationId();
    if (id == 0) {
        ui::Messages::info(this, QStringLiteral("Номинация не выбрана"),
                           QStringLiteral("Выберите номинацию в списке слева."));
        return;
    }
    const QString current = m_nominations->currentItem()->text().section(QStringLiteral(" ("), 0, 0);
    bool ok = false;
    const QString name = askName(this, QStringLiteral("Переименование номинации"), current, &ok);
    if (!ok || name == current)
        return;
    if (run([&]() { RegistryService(m_ctx.db, m_ctx.audit).renameNomination(id, name); }))
        refresh();
}

void ParticipantsPage::removeNomination()
{
    const int id = selectedNominationId();
    if (id == 0)
        return;
    const QString name = m_nominations->currentItem()->text().section(QStringLiteral(" ("), 0, 0);
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление номинации"),
                               QStringLiteral("Удалить номинацию «%1» из справочника?").arg(name),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { RegistryService(m_ctx.db, m_ctx.audit).removeNomination(id); }))
        refresh();
}

}  // namespace sedm::admin
