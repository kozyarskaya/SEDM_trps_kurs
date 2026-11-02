/**
 * @file BasketsPage.cpp
 * @brief Реализация окна «Корзины».
 */
#include "BasketsPage.h"
#include "BasketService.h"
#include "Messages.h"
#include "RegistryService.h"
#include "TeamService.h"
#include "Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
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
}  // namespace

BasketsPage::BasketsPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Корзины"), parent)
{
    auto* create = addAction(QStringLiteral("+ Создать корзину"), true);
    connect(create, &QPushButton::clicked, this, &BasketsPage::createBasket);

    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    m_list = new QTableWidget;
    m_list->setColumnCount(4);
    m_list->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Номинация"),
                                       QStringLiteral("Заполнено"), QStringLiteral("Команда")});
    setupTable(m_list);
    m_list->setSortingEnabled(false);
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->setMinimumWidth(380);
    row->addWidget(m_list, 2);

    m_editor = new QGroupBox(QStringLiteral("Корзина"));
    auto* e = new QVBoxLayout(m_editor);
    e->setSpacing(10);
    // Поля корзины в две колонки — больше места для списка образцов
    auto* form = new QGridLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    m_number = new QSpinBox;
    m_number->setRange(1, 100000);
    m_nomination = new QComboBox;
    m_desired = new QSpinBox;
    m_desired->setRange(1, Basket::kMaxDesiredCount);
    m_team = new QComboBox;
    form->addWidget(new QLabel(QStringLiteral("Номер:")), 0, 0);
    form->addWidget(m_number, 0, 1);
    form->addWidget(new QLabel(QStringLiteral("Образцов (желаемое):")), 0, 2);
    form->addWidget(m_desired, 0, 3);
    form->addWidget(new QLabel(QStringLiteral("Номинация:")), 1, 0);
    form->addWidget(m_nomination, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Команда:")), 1, 2);
    form->addWidget(m_team, 1, 3);
    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);
    e->addLayout(form);
    auto* editButtons = new QHBoxLayout;
    auto* save = new QPushButton(QStringLiteral("Сохранить изменения"));
    save->setProperty("kind", "primary");
    auto* remove = new QPushButton(QStringLiteral("Удалить корзину"));
    remove->setProperty("kind", "danger");
    editButtons->addWidget(save);
    editButtons->addStretch(1);
    editButtons->addWidget(remove);
    e->addLayout(editButtons);

    m_fillText = new QLabel;
    m_fill = new QProgressBar;
    e->addWidget(m_fillText);
    e->addWidget(m_fill);
    m_content = new QTableWidget;
    m_content->setColumnCount(3);
    m_content->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Шифр"), QStringLiteral("Участник")});
    setupTable(m_content);
    m_content->setSortingEnabled(false);
    e->addWidget(m_content, 1);
    auto* removeSample = new QPushButton(QStringLiteral("Убрать из корзины"));
    e->addWidget(removeSample, 0, Qt::AlignRight);

    // Пополнение корзины из выпадающего списка доступных образцов (п. 4.3.1.1 ТЗ)
    e->addWidget(new QLabel(QStringLiteral("Доступные образцы этой номинации:")));
    auto* addRow = new QHBoxLayout;
    m_pool = new QComboBox;
    m_pool->setMinimumWidth(200);
    m_add = new QPushButton(QStringLiteral("Добавить"));
    m_fillPool = new QPushButton(QStringLiteral("Заполнить из пула"));
    addRow->addWidget(m_pool, 1);
    addRow->addWidget(m_add);
    addRow->addWidget(m_fillPool);
    e->addLayout(addRow);
    row->addWidget(m_editor, 3);
    body()->addLayout(row, 1);

    connect(m_list, &QTableWidget::itemSelectionChanged, this, &BasketsPage::showBasket);
    connect(save, &QPushButton::clicked, this, &BasketsPage::saveBasket);
    connect(remove, &QPushButton::clicked, this, &BasketsPage::removeBasket);
    connect(removeSample, &QPushButton::clicked, this, &BasketsPage::removeSample);
    connect(m_add, &QPushButton::clicked, this, &BasketsPage::addSample);
    connect(m_fillPool, &QPushButton::clicked, this, &BasketsPage::fillFromPool);
}

void BasketsPage::refresh()
{
    QVector<Nomination> nominations;
    QVector<Team> teams;
    QVector<Participant> participants;
    if (!run([&]() {
            m_baskets = BasketService(m_ctx.db, m_ctx.audit).baskets();
            RegistryService reg(m_ctx.db, m_ctx.audit);
            nominations = reg.nominations();
            participants = reg.participants();
            teams = TeamService(m_ctx.db, m_ctx.audit).teams();
        }, false))
        return;
    m_nominations.clear();
    for (const Nomination& n : nominations)
        m_nominations.insert(n.id(), n.name());
    m_teams.clear();
    for (const Team& t : teams)
        m_teams.insert(t.id(), t);
    m_participants.clear();
    for (const Participant& p : participants)
        m_participants.insert(p.id(), p);

    {
        const QSignalBlocker b1(m_nomination);
        m_nomination->clear();
        for (const Nomination& n : nominations)
            m_nomination->addItem(n.name(), n.id());
        const QSignalBlocker b2(m_team);
        m_team->clear();
        m_team->addItem(QStringLiteral("— не назначена —"), 0);
        for (const Team& t : teams)
            m_team->addItem(t.displayName(), t.id());
    }

    const int selected = selectedBasketId();
    const QSignalBlocker block(m_list);
    m_list->setRowCount(m_baskets.size());
    int unassigned = 0;
    for (int r = 0; r < m_baskets.size(); ++r) {
        const Basket& b = m_baskets[r];
        unassigned += b.teamId() ? 0 : 1;
        m_list->setItem(r, 0, cell(QString::number(b.number()), b.id()));
        m_list->setItem(r, 1, cell(m_nominations.value(b.nominationId())));
        auto* fill = cell(QStringLiteral("%1 из %2").arg(b.sampleCount()).arg(b.desiredCount()));
        if (!b.isFull())
            fill->setForeground(ui::palette::kAmber);
        m_list->setItem(r, 2, fill);
        const Team t = b.teamId() ? m_teams.value(*b.teamId()) : Team();
        auto* team = cell(b.teamId() ? (t.name().isEmpty() ? QStringLiteral("№ %1").arg(t.number())
                                                           : QStringLiteral("№ %1 «%2»").arg(t.number()).arg(t.name()))
                                     : QStringLiteral("не назначена"));
        if (!b.teamId())
            team->setForeground(ui::palette::kAmber);
        m_list->setItem(r, 3, team);
        if (b.id() == selected)
            m_list->selectRow(r);
    }
    m_list->resizeColumnsToContents();
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    if (m_list->selectedItems().isEmpty() && !m_baskets.isEmpty())
        m_list->selectRow(0);
    setSubtitle(QStringLiteral("%1 корзин · %2 не назначены командам").arg(m_baskets.size()).arg(unassigned));
    showBasket();
}

int BasketsPage::selectedBasketId() const
{
    const auto items = m_list->selectedItems();
    return items.isEmpty() ? 0 : m_list->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
}

void BasketsPage::showBasket()
{
    const int id = selectedBasketId();
    const Basket* basket = nullptr;
    for (const Basket& b : m_baskets)
        if (b.id() == id)
            basket = &b;
    m_editor->setEnabled(basket);
    if (!basket) {
        m_editor->setTitle(QStringLiteral("Корзина не выбрана"));
        m_content->setRowCount(0);
        m_pool->clear();
        return;
    }
    m_editor->setTitle(QStringLiteral("Корзина № %1").arg(basket->number()));
    m_number->setValue(basket->number());
    m_nomination->setCurrentIndex(m_nomination->findData(basket->nominationId()));
    m_nomination->setEnabled(basket->sampleCount() == 0);   // номинация меняется только у пустой корзины
    m_desired->setValue(basket->desiredCount());
    m_team->setCurrentIndex(qMax(0, m_team->findData(basket->teamId().value_or(0))));
    m_fillText->setText(QStringLiteral("Заполнено %1 из %2").arg(basket->sampleCount()).arg(basket->desiredCount()));
    m_fill->setRange(0, basket->desiredCount());
    m_fill->setValue(basket->sampleCount());

    QVector<Sample> content;
    QVector<Sample> pool;
    run([&]() {
        BasketService service(m_ctx.db, m_ctx.audit);
        content = service.basketSamples(id);
        pool = service.availableSamples(basket->nominationId());
    }, false);
    m_content->setRowCount(content.size());
    for (int r = 0; r < content.size(); ++r) {
        m_content->setItem(r, 0, cell(QString::number(content[r].seqNumber()), content[r].id()));
        m_content->setItem(r, 1, cell(content[r].cipher()));
        m_content->setItem(r, 2, cell(m_participants.value(content[r].participantId()).shortName()));
    }
    m_content->resizeColumnsToContents();
    m_pool->clear();
    for (const Sample& s : pool)
        m_pool->addItem(QStringLiteral("%1 — № %2, %3").arg(s.cipher()).arg(s.seqNumber())
                            .arg(m_participants.value(s.participantId()).shortName()), s.id());
    const bool canAdd = !pool.isEmpty() && !basket->isFull();
    m_pool->setEnabled(canAdd);
    m_add->setEnabled(canAdd);
    m_fillPool->setEnabled(canAdd);
    if (pool.isEmpty())
        m_pool->addItem(QStringLiteral("нет свободных образцов этой номинации"));
}

void BasketsPage::createBasket()
{
    if (m_nominations.isEmpty()) {
        ui::Messages::info(this, QStringLiteral("Нет номинаций"),
                           QStringLiteral("Сначала добавьте номинации и зарегистрируйте образцы."));
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Новая корзина"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(24, 20, 24, 18);
    auto* form = new QFormLayout;
    auto* number = new QSpinBox;
    number->setRange(1, 100000);
    int suggested = 1;
    run([&]() { suggested = BasketService(m_ctx.db, m_ctx.audit).suggestedNumber(); }, false);
    number->setValue(suggested);
    auto* nomination = new QComboBox;
    for (auto it = m_nominations.constBegin(); it != m_nominations.constEnd(); ++it)
        nomination->addItem(it.value(), it.key());
    nomination->model()->sort(0);
    auto* desired = new QSpinBox;
    desired->setRange(1, Basket::kMaxDesiredCount);
    desired->setValue(6);
    form->addRow(QStringLiteral("Номер корзины:"), number);
    form->addRow(QStringLiteral("Номинация:"), nomination);
    form->addRow(QStringLiteral("Желаемое количество образцов:"), desired);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox;
    auto* ok = buttons->addButton(QStringLiteral("Создать"), QDialogButtonBox::AcceptRole);
    ok->setProperty("kind", "primary");
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    Basket created;
    if (run([&]() {
            created = BasketService(m_ctx.db, m_ctx.audit)
                          .createBasket(number->value(), nomination->currentData().toInt(), desired->value());
        })) {
        refresh();
        for (int r = 0; r < m_list->rowCount(); ++r)
            if (m_list->item(r, 0)->data(Qt::UserRole).toInt() == created.id())
                m_list->selectRow(r);
    }
}

void BasketsPage::saveBasket()
{
    const int id = selectedBasketId();
    if (id == 0)
        return;
    const int teamId = m_team->currentData().toInt();
    if (run([&]() {
            BasketService service(m_ctx.db, m_ctx.audit);
            service.updateBasket(id, m_number->value(), m_nomination->currentData().toInt(), m_desired->value());
            service.assignTeam(id, teamId > 0 ? std::optional<int>(teamId) : std::nullopt);
        }))
        refresh();
}

void BasketsPage::removeBasket()
{
    const int id = selectedBasketId();
    if (id == 0)
        return;
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление корзины"),
                               QStringLiteral("Удалить корзину № %1? Её образцы вернутся в пул доступных.")
                                   .arg(m_number->value()),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { BasketService(m_ctx.db, m_ctx.audit).removeBasket(id); }))
        refresh();
}

void BasketsPage::addSample()
{
    const int basketId = selectedBasketId();
    const int sampleId = m_pool->currentData().toInt();
    if (basketId == 0 || sampleId == 0)
        return;
    if (run([&]() { BasketService(m_ctx.db, m_ctx.audit).addSample(basketId, sampleId); }))
        refresh();
}

void BasketsPage::removeSample()
{
    const int basketId = selectedBasketId();
    const auto items = m_content->selectedItems();
    if (basketId == 0 || items.isEmpty())
        return;
    const int sampleId = m_content->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
    if (run([&]() { BasketService(m_ctx.db, m_ctx.audit).removeSample(basketId, sampleId); }))
        refresh();
}

void BasketsPage::fillFromPool()
{
    const int basketId = selectedBasketId();
    if (basketId == 0)
        return;
    int added = 0;
    if (run([&]() { added = BasketService(m_ctx.db, m_ctx.audit).fillFromPool(basketId); })) {
        refresh();
        ui::Messages::info(this, QStringLiteral("Корзина заполнена"),
                           QStringLiteral("Добавлено образцов из пула: %1.").arg(added));
    }
}

}  // namespace sedm::admin
