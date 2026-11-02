/**
 * @file TeamsPage.cpp
 * @brief Реализация окна «Команды и дегустаторы».
 */
#include "TeamsPage.h"
#include "BasketService.h"
#include "CardPrinter.h"
#include "Messages.h"
#include "RegistryService.h"
#include "SessionService.h"
#include "TeamService.h"
#include "Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

namespace {
QTableWidgetItem* cell(const QString& text, int id = 0)
{
    auto* item = new QTableWidgetItem(text);
    item->setData(Qt::UserRole, id);
    return item;
}

QDialogButtonBox* okCancel(QDialog* dlg, const QString& okText)
{
    auto* buttons = new QDialogButtonBox;
    auto* ok = buttons->addButton(okText, QDialogButtonBox::AcceptRole);
    ok->setProperty("kind", "primary");
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return buttons;
}
}  // namespace

TeamsPage::TeamsPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Команды и дегустаторы"), parent)
{
    auto* cards = addAction(QStringLiteral("Карточки дегустаторов…"), false, QStringLiteral("print"));
    auto* create = addAction(QStringLiteral("+ Создать команду"), true);
    connect(cards, &QPushButton::clicked, this, &TeamsPage::issueCards);
    connect(create, &QPushButton::clicked, this, &TeamsPage::createTeam);

    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    m_list = new QTableWidget;
    m_list->setColumnCount(4);
    m_list->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Название"),
                                       QStringLiteral("Дегустаторов"), QStringLiteral("Корзин")});
    setupTable(m_list);
    m_list->setSortingEnabled(false);
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->setMinimumWidth(360);
    row->addWidget(m_list, 2);

    m_editor = new QGroupBox(QStringLiteral("Команда"));
    auto* e = new QVBoxLayout(m_editor);
    e->setSpacing(10);
    auto* form = new QHBoxLayout;
    m_number = new QSpinBox;
    m_number->setRange(1, 100000);
    m_number->setMaximumWidth(110);
    m_name = new QLineEdit;
    m_name->setPlaceholderText(QStringLiteral("название (необязательно)"));
    auto* save = new QPushButton(QStringLiteral("Сохранить"));
    save->setProperty("kind", "primary");
    auto* remove = new QPushButton(QStringLiteral("Удалить команду"));
    remove->setProperty("kind", "danger");
    form->addWidget(new QLabel(QStringLiteral("Номер:")));
    form->addWidget(m_number);
    form->addWidget(new QLabel(QStringLiteral("Название:")));
    form->addWidget(m_name, 1);
    e->addLayout(form);
    // кнопки — отдельной строкой, чтобы полю названия хватало ширины
    auto* teamButtons = new QHBoxLayout;
    teamButtons->addWidget(save);
    teamButtons->addStretch(1);
    teamButtons->addWidget(remove);
    e->addLayout(teamButtons);

    auto* tastersTitle = new QLabel(QStringLiteral("Дегустаторы команды"));
    tastersTitle->setProperty("role", "subtitle");
    e->addWidget(tastersTitle);
    m_tasterTable = new QTableWidget;
    m_tasterTable->setColumnCount(2);
    m_tasterTable->setHorizontalHeaderLabels({QStringLiteral("ФИО"), QStringLiteral("Логин")});
    setupTable(m_tasterTable);
    m_tasterTable->setSortingEnabled(false);
    m_tasterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    e->addWidget(m_tasterTable, 2);
    auto* tasterButtons = new QHBoxLayout;
    auto* addTaster = new QPushButton(QStringLiteral("Добавить дегустатора…"));
    auto* renameTaster = new QPushButton(QStringLiteral("Изменить ФИО…"));
    auto* pin = new QPushButton(QStringLiteral("Новый PIN-код…"));
    auto* removeTaster = new QPushButton(QStringLiteral("Удалить"));
    removeTaster->setProperty("kind", "danger");
    tasterButtons->addWidget(addTaster);
    tasterButtons->addWidget(renameTaster);
    tasterButtons->addWidget(pin);
    tasterButtons->addStretch(1);
    tasterButtons->addWidget(removeTaster);
    e->addLayout(tasterButtons);

    auto* basketsTitle = new QLabel(QStringLiteral("Корзины команды"));
    basketsTitle->setProperty("role", "subtitle");
    e->addWidget(basketsTitle);
    m_basketTable = new QTableWidget;
    m_basketTable->setColumnCount(3);
    m_basketTable->setHorizontalHeaderLabels({QStringLiteral("Корзина"), QStringLiteral("Номинация"),
                                              QStringLiteral("Образцов")});
    setupTable(m_basketTable);
    m_basketTable->setSortingEnabled(false);
    m_basketTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    e->addWidget(m_basketTable, 1);
    auto* basketButtons = new QHBoxLayout;
    auto* assign = new QPushButton(QStringLiteral("Назначить корзины…"));
    auto* unassign = new QPushButton(QStringLiteral("Снять с команды"));
    basketButtons->addWidget(assign);
    basketButtons->addWidget(unassign);
    basketButtons->addStretch(1);
    e->addLayout(basketButtons);
    row->addWidget(m_editor, 3);
    body()->addLayout(row, 1);

    connect(m_list, &QTableWidget::itemSelectionChanged, this, &TeamsPage::showTeam);
    connect(save, &QPushButton::clicked, this, &TeamsPage::saveTeam);
    connect(remove, &QPushButton::clicked, this, &TeamsPage::removeTeam);
    connect(addTaster, &QPushButton::clicked, this, &TeamsPage::addTaster);
    connect(renameTaster, &QPushButton::clicked, this, &TeamsPage::renameTaster);
    connect(removeTaster, &QPushButton::clicked, this, &TeamsPage::removeTaster);
    connect(pin, &QPushButton::clicked, this, &TeamsPage::resetPin);
    connect(assign, &QPushButton::clicked, this, &TeamsPage::assignBaskets);
    connect(unassign, &QPushButton::clicked, this, &TeamsPage::unassignBasket);
}

QString TeamsPage::sessionText() const
{
    QString text;
    try {
        text = SessionService(m_ctx.db, m_ctx.audit).session().displayText();
    } catch (const std::exception&) {
    }
    return text;
}

void TeamsPage::refresh()
{
    QVector<Nomination> nominations;
    if (!run([&]() {
            TeamService teams(m_ctx.db, m_ctx.audit);
            m_teams = teams.teams();
            m_tasters = teams.allTasters();
            m_baskets = BasketService(m_ctx.db, m_ctx.audit).baskets();
            nominations = RegistryService(m_ctx.db, m_ctx.audit).nominations();
        }, false))
        return;
    m_nominations.clear();
    for (const Nomination& n : nominations)
        m_nominations.insert(n.id(), n.name());

    const int selected = selectedTeamId();
    const QSignalBlocker block(m_list);
    m_list->setRowCount(m_teams.size());
    for (int r = 0; r < m_teams.size(); ++r) {
        const Team& t = m_teams[r];
        int tasters = 0, baskets = 0;
        for (const Taster& s : m_tasters)
            tasters += s.teamId() == t.id() ? 1 : 0;
        for (const Basket& b : m_baskets)
            baskets += b.teamId() == t.id() ? 1 : 0;
        m_list->setItem(r, 0, cell(QString::number(t.number()), t.id()));
        m_list->setItem(r, 1, cell(t.name().isEmpty() ? QStringLiteral("—") : t.name()));
        auto* tCell = cell(QString::number(tasters));
        if (tasters == 0)
            tCell->setForeground(ui::palette::kAmber);
        m_list->setItem(r, 2, tCell);
        auto* bCell = cell(QString::number(baskets));
        if (baskets == 0)
            bCell->setForeground(ui::palette::kAmber);
        m_list->setItem(r, 3, bCell);
        if (t.id() == selected)
            m_list->selectRow(r);
    }
    m_list->resizeColumnsToContents();
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    if (m_list->selectedItems().isEmpty() && !m_teams.isEmpty())
        m_list->selectRow(0);
    setSubtitle(QStringLiteral("%1 команд · %2 дегустаторов").arg(m_teams.size()).arg(m_tasters.size()));
    showTeam();
}

int TeamsPage::selectedTeamId() const
{
    const auto items = m_list->selectedItems();
    return items.isEmpty() ? 0 : m_list->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
}

int TeamsPage::selectedTasterId() const
{
    const auto items = m_tasterTable->selectedItems();
    return items.isEmpty() ? 0 : m_tasterTable->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
}

void TeamsPage::showTeam()
{
    const int id = selectedTeamId();
    const Team* team = nullptr;
    for (const Team& t : m_teams)
        if (t.id() == id)
            team = &t;
    m_editor->setEnabled(team);
    m_tasterTable->setRowCount(0);
    m_basketTable->setRowCount(0);
    if (!team) {
        m_editor->setTitle(QStringLiteral("Команда не выбрана"));
        return;
    }
    m_editor->setTitle(team->displayName());
    m_number->setValue(team->number());
    m_name->setText(team->name());
    for (const Taster& t : m_tasters)
        if (t.teamId() == id) {
            const int r = m_tasterTable->rowCount();
            m_tasterTable->insertRow(r);
            m_tasterTable->setItem(r, 0, cell(t.fullName(), t.id()));
            m_tasterTable->setItem(r, 1, cell(t.login()));
        }
    m_tasterTable->resizeColumnsToContents();
    m_tasterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (const Basket& b : m_baskets)
        if (b.teamId() == id) {
            const int r = m_basketTable->rowCount();
            m_basketTable->insertRow(r);
            m_basketTable->setItem(r, 0, cell(QStringLiteral("№ %1").arg(b.number()), b.id()));
            m_basketTable->setItem(r, 1, cell(m_nominations.value(b.nominationId())));
            m_basketTable->setItem(r, 2, cell(QStringLiteral("%1 из %2").arg(b.sampleCount()).arg(b.desiredCount())));
        }
    m_basketTable->resizeColumnsToContents();
    m_basketTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void TeamsPage::createTeam()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Новая команда"));
    dlg.setMinimumWidth(560);
    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);
    auto* form = new QFormLayout;
    auto* number = new QSpinBox;
    number->setRange(1, 100000);
    int suggested = 1;
    run([&]() { suggested = TeamService(m_ctx.db, m_ctx.audit).suggestedNumber(); }, false);
    number->setValue(suggested);
    auto* name = new QLineEdit;
    name->setPlaceholderText(QStringLiteral("необязательно, например «Эксперты»"));
    form->addRow(QStringLiteral("Номер команды:"), number);
    form->addRow(QStringLiteral("Название:"), name);
    layout->addLayout(form);
    auto* hint = new QLabel(QStringLiteral("Дегустаторы — по одному в строке (фамилия или ФИО русскими буквами). "
                                           "Перечень можно оставить пустым и дополнить позже. Логин дегустатора — "
                                           "его фамилия; PIN-код программа выдаст сама."));
    hint->setWordWrap(true);
    hint->setProperty("role", "muted");
    layout->addWidget(hint);
    auto* names = new QPlainTextEdit;
    names->setPlaceholderText(QStringLiteral("Иванова Мария Петровна\nСоколов Пётр Ильич"));
    names->setMinimumHeight(160);
    layout->addWidget(names);
    layout->addWidget(okCancel(&dlg, QStringLiteral("Создать команду")));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QVector<TasterCredentials> cards;
    Team created;
    if (!run([&]() {
            cards = TeamService(m_ctx.db, m_ctx.audit)
                        .createTeam(number->value(), name->text(), TeamService::parseNames(names->toPlainText()),
                                    &created);
        }))
        return;
    refresh();
    for (int r = 0; r < m_list->rowCount(); ++r)
        if (m_list->item(r, 0)->data(Qt::UserRole).toInt() == created.id())
            m_list->selectRow(r);
    if (!cards.isEmpty()) {
        CredentialsDialog credentials(cards, sessionText(), this);
        credentials.exec();
    }
}

void TeamsPage::saveTeam()
{
    const int id = selectedTeamId();
    if (id != 0 && run([&]() { TeamService(m_ctx.db, m_ctx.audit).updateTeam(id, m_number->value(), m_name->text()); }))
        refresh();
}

void TeamsPage::removeTeam()
{
    const int id = selectedTeamId();
    if (id == 0)
        return;
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление команды"),
                               QStringLiteral("Удалить «%1» вместе с учётными записями её дегустаторов? Корзины "
                                              "команды останутся без назначения.").arg(m_editor->title()),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { TeamService(m_ctx.db, m_ctx.audit).removeTeam(id); }))
        refresh();
}

void TeamsPage::addTaster()
{
    const int teamId = selectedTeamId();
    if (teamId == 0)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Новый дегустатор"),
                                               QStringLiteral("Фамилия или ФИО дегустатора:"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok)
        return;
    TasterCredentials card;
    if (run([&]() { card = TeamService(m_ctx.db, m_ctx.audit).addTaster(teamId, name); })) {
        refresh();
        CredentialsDialog credentials({card}, sessionText(), this);
        credentials.exec();
    }
}

void TeamsPage::renameTaster()
{
    const int id = selectedTasterId();
    if (id == 0)
        return;
    const QString current = m_tasterTable->item(m_tasterTable->currentRow(), 0)->text();
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Изменение ФИО"),
                                               QStringLiteral("ФИО дегустатора (логин не меняется):"),
                                               QLineEdit::Normal, current, &ok);
    if (ok && name != current && run([&]() { TeamService(m_ctx.db, m_ctx.audit).renameTaster(id, name); }))
        refresh();
}

void TeamsPage::removeTaster()
{
    const int id = selectedTasterId();
    if (id == 0)
        return;
    const QString name = m_tasterTable->item(m_tasterTable->currentRow(), 0)->text();
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление дегустатора"),
                               QStringLiteral("Удалить учётную запись дегустатора «%1»?").arg(name),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { TeamService(m_ctx.db, m_ctx.audit).removeTaster(id); }))
        refresh();
}

void TeamsPage::resetPin()
{
    const int id = selectedTasterId();
    if (id == 0)
        return;
    const QString name = m_tasterTable->item(m_tasterTable->currentRow(), 0)->text();
    if (!ui::Messages::confirm(this, QStringLiteral("Новый PIN-код"),
                               QStringLiteral("Выдать дегустатору «%1» новый PIN-код? Прежний перестанет действовать, "
                                              "блокировка входа будет снята.").arg(name),
                               QStringLiteral("Выдать"), QStringLiteral("Отмена")))
        return;
    QVector<TasterCredentials> cards;
    if (run([&]() { cards = TeamService(m_ctx.db, m_ctx.audit).resetPins({id}); })) {
        CredentialsDialog credentials(cards, sessionText(), this);
        credentials.exec();
    }
}

void TeamsPage::issueCards()
{
    if (m_tasters.isEmpty()) {
        ui::Messages::info(this, QStringLiteral("Нет дегустаторов"),
                           QStringLiteral("Сначала создайте команды и внесите дегустаторов."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Карточки дегустаторов"));
    dlg.resize(640, 560);
    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 20, 24, 18);
    auto* hint = new QLabel(QStringLiteral("Отметьте дегустаторов, которым нужны карточки. PIN-коды хранятся в виде "
                                           "хэша, поэтому для печати карточек выдаются новые PIN-коды; прежние "
                                           "перестанут действовать."));
    hint->setWordWrap(true);
    hint->setProperty("role", "warn");
    layout->addWidget(hint);
    auto* tree = new QTreeWidget;
    tree->setHeaderHidden(true);
    for (const Team& t : m_teams) {
        auto* teamItem = new QTreeWidgetItem(tree, {t.displayName()});
        teamItem->setFlags(teamItem->flags() | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
        teamItem->setCheckState(0, Qt::Checked);
        for (const Taster& s : m_tasters)
            if (s.teamId() == t.id()) {
                auto* item = new QTreeWidgetItem(teamItem, {QStringLiteral("%1 (логин %2)").arg(s.fullName(), s.login())});
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(0, Qt::Checked);
                item->setData(0, Qt::UserRole, s.id());
            }
    }
    tree->expandAll();
    layout->addWidget(tree, 1);
    layout->addWidget(okCancel(&dlg, QStringLiteral("Выдать PIN-коды и показать карточки")));
    if (dlg.exec() != QDialog::Accepted)
        return;
    QVector<int> ids;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        for (int k = 0; k < tree->topLevelItem(i)->childCount(); ++k) {
            const QTreeWidgetItem* item = tree->topLevelItem(i)->child(k);
            if (item->checkState(0) == Qt::Checked)
                ids << item->data(0, Qt::UserRole).toInt();
        }
    if (ids.isEmpty())
        return;
    QVector<TasterCredentials> cards;
    if (run([&]() { cards = TeamService(m_ctx.db, m_ctx.audit).resetPins(ids); })) {
        CredentialsDialog credentials(cards, sessionText(), this);
        credentials.exec();
    }
}

void TeamsPage::assignBaskets()
{
    const int teamId = selectedTeamId();
    if (teamId == 0)
        return;
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Назначение корзин команде"));
    dlg.resize(520, 480);
    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 20, 24, 18);
    auto* hint = new QLabel(QStringLiteral("Корзины, ещё не назначенные ни одной команде:"));
    layout->addWidget(hint);
    auto* list = new QListWidget;
    for (const Basket& b : m_baskets)
        if (!b.teamId()) {
            auto* item = new QListWidgetItem(QStringLiteral("Корзина № %1 · %2 · образцов %3")
                                                 .arg(b.number()).arg(m_nominations.value(b.nominationId()))
                                                 .arg(b.sampleCount()), list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            item->setData(Qt::UserRole, b.id());
        }
    if (list->count() == 0) {
        ui::Messages::info(this, QStringLiteral("Нет свободных корзин"),
                           QStringLiteral("Все корзины уже назначены командам."));
        return;
    }
    layout->addWidget(list, 1);
    layout->addWidget(okCancel(&dlg, QStringLiteral("Назначить")));
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (run([&]() {
            BasketService service(m_ctx.db, m_ctx.audit);
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->checkState() == Qt::Checked)
                    service.assignTeam(list->item(i)->data(Qt::UserRole).toInt(), teamId);
        }))
        refresh();
}

void TeamsPage::unassignBasket()
{
    const auto items = m_basketTable->selectedItems();
    if (items.isEmpty())
        return;
    const int basketId = m_basketTable->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
    if (run([&]() { BasketService(m_ctx.db, m_ctx.audit).assignTeam(basketId, std::nullopt); }))
        refresh();
}

}  // namespace sedm::admin
