/**
 * @file SamplesPage.cpp
 * @brief Реализация окна «Образцы».
 */
#include "SamplesPage.h"
#include "BasketService.h"
#include "Icons.h"
#include "Messages.h"
#include "RecordFilter.h"
#include "RegistryService.h"
#include "SampleService.h"
#include "Theme.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSet>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextDocument>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>

namespace sedm::admin {

namespace {

/// Окно выбора владельца и номинации образца (регистрация и изменение).
class SampleDialog : public QDialog {
public:
    SampleDialog(const QString& title, const QVector<Participant>& participants, const QVector<Nomination>& nominations,
                 int participantId, int nominationId, QWidget* parent)
        : QDialog(parent)
    {
        setWindowTitle(title);
        setMinimumWidth(560);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 18);
        layout->setSpacing(14);
        auto* hint = new QLabel(QStringLiteral("Порядковый номер и шифр образцу присваиваются автоматически. "
                                               "Дегустаторы видят только шифр."));
        hint->setWordWrap(true);
        hint->setProperty("role", "muted");
        layout->addWidget(hint);
        auto* form = new QFormLayout;
        m_owner = new QComboBox;
        m_owner->setEditable(true);
        m_owner->setInsertPolicy(QComboBox::NoInsert);
        for (const Participant& p : participants) {
            m_owner->addItem(QStringLiteral("%1 — %2").arg(p.fullName(), p.region()), p.id());
            m_ownerNomination.insert(p.id(), p.nominationId());
        }
        m_owner->completer()->setCompletionMode(QCompleter::PopupCompletion);
        m_owner->completer()->setFilterMode(Qt::MatchContains);
        m_nomination = new QComboBox;
        for (const Nomination& n : nominations)
            m_nomination->addItem(n.name(), n.id());
        form->addRow(QStringLiteral("Участник (владелец):"), m_owner);
        form->addRow(QStringLiteral("Номинация:"), m_nomination);
        layout->addLayout(form);
        auto* buttons = new QDialogButtonBox;
        auto* ok = buttons->addButton(QStringLiteral("Сохранить"), QDialogButtonBox::AcceptRole);
        ok->setProperty("kind", "primary");
        buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        // номинация по умолчанию — заявленная участником
        connect(m_owner, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            const int nomination = m_ownerNomination.value(m_owner->currentData().toInt());
            const int idx = m_nomination->findData(nomination);
            if (idx >= 0)
                m_nomination->setCurrentIndex(idx);
        });
        const int ownerIdx = m_owner->findData(participantId);
        m_owner->setCurrentIndex(ownerIdx >= 0 ? ownerIdx : 0);
        if (nominationId > 0) {
            const int idx = m_nomination->findData(nominationId);
            if (idx >= 0)
                m_nomination->setCurrentIndex(idx);
        }
    }
    int participantId() const { return m_owner->currentData().toInt(); }
    int nominationId() const { return m_nomination->currentData().toInt(); }

private:
    QComboBox* m_owner;
    QComboBox* m_nomination;
    QHash<int, int> m_ownerNomination;
};

QLabel* cardValue(QVBoxLayout* layout, const QString& caption)
{
    auto* c = new QLabel(caption);
    c->setProperty("role", "caption");
    auto* v = new QLabel;
    v->setWordWrap(true);
    layout->addWidget(c);
    layout->addWidget(v);
    return v;
}

}  // namespace

SamplesPage::SamplesPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Образцы мёда"), parent)
{
    auto* print = addAction(QStringLiteral("Печать списка"), false, QStringLiteral("print"));
    auto* forAll = addAction(QStringLiteral("Образцы для всех участников…"));
    auto* add = addAction(QStringLiteral("+ Зарегистрировать образец"), true);
    connect(print, &QPushButton::clicked, this, &SamplesPage::printList);
    connect(forAll, &QPushButton::clicked, this, &SamplesPage::registerForAll);
    connect(add, &QPushButton::clicked, this, &SamplesPage::registerSample);

    auto* filters = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Поиск по шифру или участнику"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(ui::Icons::icon(QStringLiteral("search"), ui::palette::kMuted), QLineEdit::LeadingPosition);
    m_nominationFilter = new QComboBox;
    m_basketFilter = new QComboBox;
    m_basketFilter->addItem(QStringLiteral("все"), 0);
    m_basketFilter->addItem(QStringLiteral("не распределены"), -1);
    filters->addWidget(m_search, 1);
    filters->addWidget(new QLabel(QStringLiteral("Номинация:")));
    filters->addWidget(m_nominationFilter);
    filters->addWidget(new QLabel(QStringLiteral("Корзина:")));
    filters->addWidget(m_basketFilter);
    body()->addLayout(filters);

    auto* row = new QHBoxLayout;
    row->setSpacing(16);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("№"), QStringLiteral("Шифр"), QStringLiteral("Участник"),
                                        QStringLiteral("Регион"), QStringLiteral("Номинация"), QStringLiteral("Корзина")});
    m_filter = new RecordFilter(this);
    m_filter->setSourceModel(m_model);
    m_table = new QTableView;
    m_table->setModel(m_filter);
    setupTable(m_table);
    row->addWidget(m_table, 1);

    // Карточка выбранного образца
    auto* card = new QFrame;
    card->setProperty("card", "true");
    card->setFixedWidth(300);
    auto* c = new QVBoxLayout(card);
    c->setContentsMargins(18, 18, 18, 18);
    c->setSpacing(6);
    m_cardNumber = new QLabel(QStringLiteral("Выберите образец"));
    m_cardNumber->setProperty("role", "muted");
    c->addWidget(m_cardNumber);
    auto* cipherRow = new QHBoxLayout;
    auto* cipherCaption = new QLabel(QStringLiteral("шифр"));
    cipherCaption->setProperty("role", "muted");
    m_cardCipher = new QLabel(QStringLiteral("—"));
    m_cardCipher->setProperty("role", "big");
    cipherRow->addWidget(cipherCaption, 0, Qt::AlignBaseline);
    cipherRow->addWidget(m_cardCipher, 0, Qt::AlignBaseline);
    cipherRow->addStretch(1);
    c->addLayout(cipherRow);
    m_cardOwner = cardValue(c, QStringLiteral("Участник"));
    m_cardNomination = cardValue(c, QStringLiteral("Номинация"));
    m_cardBasket = cardValue(c, QStringLiteral("Корзина"));
    c->addStretch(1);
    m_toBasket = new QPushButton(QStringLiteral("Добавить в корзину…"));
    m_toBasket->setProperty("kind", "primary");
    c->addWidget(m_toBasket);
    auto* buttons = new QHBoxLayout;
    m_edit = new QPushButton(QStringLiteral("Изменить"));
    m_remove = new QPushButton(QStringLiteral("Удалить"));
    m_remove->setProperty("kind", "danger");
    buttons->addWidget(m_edit);
    buttons->addWidget(m_remove);
    c->addLayout(buttons);
    row->addWidget(card);
    body()->addLayout(row, 1);

    connect(m_search, &QLineEdit::textChanged, m_filter, &RecordFilter::setText);
    connect(m_nominationFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(m_basketFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refresh(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &SamplesPage::showCard);
    connect(m_table, &QTableView::doubleClicked, this, &SamplesPage::editSample);
    connect(m_toBasket, &QPushButton::clicked, this, &SamplesPage::addToBasket);
    connect(m_edit, &QPushButton::clicked, this, &SamplesPage::editSample);
    connect(m_remove, &QPushButton::clicked, this, &SamplesPage::removeSample);
}

void SamplesPage::refresh()
{
    QVector<Nomination> nominations;
    QVector<Participant> participants;
    QVector<Basket> baskets;
    if (!run([&]() {
            RegistryService reg(m_ctx.db, m_ctx.audit);
            nominations = reg.nominations();
            participants = reg.participants();
            m_samples = SampleService(m_ctx.db, m_ctx.audit).samples();
            baskets = BasketService(m_ctx.db, m_ctx.audit).baskets();
        }, false))
        return;
    m_participants.clear();
    for (const Participant& p : participants)
        m_participants.insert(p.id(), p);
    m_nominationNames.clear();
    for (const Nomination& n : nominations)
        m_nominationNames.insert(n.id(), n.name());
    m_baskets.clear();
    for (const Basket& b : baskets)
        m_baskets.insert(b.id(), b);

    // список номинаций в отборе (с сохранением выбора)
    const int chosenNomination = m_nominationFilter->currentData().toInt();
    {
        const QSignalBlocker block(m_nominationFilter);
        m_nominationFilter->clear();
        m_nominationFilter->addItem(QStringLiteral("все"), 0);
        for (const Nomination& n : nominations)
            m_nominationFilter->addItem(n.name(), n.id());
        const int idx = m_nominationFilter->findData(chosenNomination);
        m_nominationFilter->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    const int nominationKey = m_nominationFilter->currentData().toInt();
    const bool onlyFree = m_basketFilter->currentData().toInt() == -1;

    const int selected = selectedSampleId();
    m_model->removeRows(0, m_model->rowCount());
    int unassigned = 0;
    for (const Sample& s : m_samples) {
        unassigned += s.isInBasket() ? 0 : 1;
        if ((nominationKey != 0 && s.nominationId() != nominationKey) || (onlyFree && s.isInBasket()))
            continue;
        const Participant owner = m_participants.value(s.participantId());
        const QString basket = s.isInBasket() ? QStringLiteral("№ %1").arg(m_baskets.value(*s.basketId()).number())
                                              : QStringLiteral("не распределён");
        QList<QStandardItem*> row = {makeItem(QString::number(s.seqNumber()), s.seqNumber()),
                                     makeItem(s.cipher()), makeItem(owner.shortName()), makeItem(owner.region()),
                                     makeItem(m_nominationNames.value(s.nominationId())),
                                     makeItem(basket, s.isInBasket() ? m_baskets.value(*s.basketId()).number() : 0)};
        QFont bold = row[1]->font();
        bold.setBold(true);
        row[1]->setFont(bold);
        if (!s.isInBasket()) {
            row[5]->setForeground(ui::palette::kAmber);
            row[5]->setFont(bold);
        }
        row[0]->setData(s.id(), Qt::UserRole);
        m_model->appendRow(row);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->sortByColumn(0, Qt::AscendingOrder);
    // восстановить выбор
    for (int r = 0; r < m_filter->rowCount(); ++r)
        if (m_filter->index(r, 0).data(Qt::UserRole).toInt() == selected)
            m_table->selectRow(r);
    setSubtitle(QStringLiteral("%1 образцов · %2 ещё не распределены по корзинам").arg(m_samples.size()).arg(unassigned));
    showCard();
}

int SamplesPage::selectedSampleId() const
{
    const QModelIndex index = m_table->currentIndex();
    return index.isValid() ? index.siblingAtColumn(0).data(Qt::UserRole).toInt() : 0;
}

void SamplesPage::showCard()
{
    const int id = selectedSampleId();
    const Sample* sample = nullptr;
    for (const Sample& s : m_samples)
        if (s.id() == id)
            sample = &s;
    m_toBasket->setEnabled(sample && !sample->isInBasket());
    m_edit->setEnabled(sample);
    m_remove->setEnabled(sample);
    if (!sample) {
        m_cardNumber->setText(QStringLiteral("Выберите образец в таблице"));
        m_cardCipher->clear();
        m_cardOwner->clear();
        m_cardNomination->clear();
        m_cardBasket->clear();
        return;
    }
    m_cardNumber->setText(QStringLiteral("Образец № %1").arg(sample->seqNumber()));
    m_cardCipher->setText(sample->cipher());
    m_cardOwner->setText(m_participants.value(sample->participantId()).fullName());
    m_cardNomination->setText(m_nominationNames.value(sample->nominationId()));
    if (sample->isInBasket()) {
        m_cardBasket->setText(QStringLiteral("№ %1").arg(m_baskets.value(*sample->basketId()).number()));
        ui::Theme::setProp(m_cardBasket, "role", QString());
    } else {
        m_cardBasket->setText(QStringLiteral("не распределён"));
        ui::Theme::setProp(m_cardBasket, "role", "warn");
    }
}

void SamplesPage::registerSample()
{
    if (m_participants.isEmpty()) {
        ui::Messages::info(this, QStringLiteral("Нет участников"),
                           QStringLiteral("Сначала внесите участников в реестр (окно «Участники и номинации»)."));
        return;
    }
    QVector<Participant> participants = m_participants.values().toVector();
    std::sort(participants.begin(), participants.end(), [](const Participant& a, const Participant& b) {
        return a.fullName().localeAwareCompare(b.fullName()) < 0;
    });
    QVector<Nomination> nominations;
    run([&]() { nominations = RegistryService(m_ctx.db, m_ctx.audit).nominations(); }, false);
    SampleDialog dlg(QStringLiteral("Регистрация образца"), participants, nominations, 0, 0, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    Sample created;
    if (!run([&]() { created = SampleService(m_ctx.db, m_ctx.audit).registerSample(dlg.participantId(), dlg.nominationId()); }))
        return;
    refresh();
    ui::Messages::info(this, QStringLiteral("Образец зарегистрирован"),
                       QStringLiteral("Образцу присвоены порядковый номер %1 и шифр %2.\n"
                                      "Подпишите банку этим шифром.").arg(created.seqNumber()).arg(created.cipher()));
}

void SamplesPage::registerForAll()
{
    QSet<int> withSamples;
    for (const Sample& s : m_samples)
        withSamples.insert(s.participantId());
    QVector<int> ids;
    for (const Participant& p : m_participants)
        if (!withSamples.contains(p.id()))
            ids << p.id();
    if (ids.isEmpty()) {
        ui::Messages::info(this, QStringLiteral("Образцы для всех участников"),
                           QStringLiteral("У каждого участника уже есть зарегистрированный образец."));
        return;
    }
    if (!ui::Messages::confirm(this, QStringLiteral("Образцы для всех участников"),
                               QStringLiteral("Зарегистрировать по одному образцу для %1 участников без образцов "
                                              "(в заявленных ими номинациях)?").arg(ids.size()),
                               QStringLiteral("Зарегистрировать"), QStringLiteral("Отмена")))
        return;
    QVector<Sample> created;
    if (run([&]() { created = SampleService(m_ctx.db, m_ctx.audit).registerForParticipants(ids); })) {
        refresh();
        ui::Messages::info(this, QStringLiteral("Образцы зарегистрированы"),
                           QStringLiteral("Зарегистрировано образцов: %1. Шифры — в таблице и в списке для печати.")
                               .arg(created.size()));
    }
}

void SamplesPage::editSample()
{
    const int id = selectedSampleId();
    const Sample* sample = nullptr;
    for (const Sample& s : m_samples)
        if (s.id() == id)
            sample = &s;
    if (!sample)
        return;
    QVector<Participant> participants = m_participants.values().toVector();
    std::sort(participants.begin(), participants.end(), [](const Participant& a, const Participant& b) {
        return a.fullName().localeAwareCompare(b.fullName()) < 0;
    });
    QVector<Nomination> nominations;
    run([&]() { nominations = RegistryService(m_ctx.db, m_ctx.audit).nominations(); }, false);
    SampleDialog dlg(QStringLiteral("Образец № %1, шифр %2").arg(sample->seqNumber()).arg(sample->cipher()),
                     participants, nominations, sample->participantId(), sample->nominationId(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (run([&]() { SampleService(m_ctx.db, m_ctx.audit).changeSample(id, dlg.participantId(), dlg.nominationId()); }))
        refresh();
}

void SamplesPage::removeSample()
{
    const int id = selectedSampleId();
    if (id == 0)
        return;
    const QString cipher = m_table->currentIndex().siblingAtColumn(1).data().toString();
    if (!ui::Messages::confirm(this, QStringLiteral("Удаление образца"),
                               QStringLiteral("Удалить образец с шифром %1? Порядковый номер повторно не выдаётся.")
                                   .arg(cipher),
                               QStringLiteral("Удалить"), QStringLiteral("Отмена"), true))
        return;
    if (run([&]() { SampleService(m_ctx.db, m_ctx.audit).removeSample(id); }))
        refresh();
}

void SamplesPage::addToBasket()
{
    const int id = selectedSampleId();
    const Sample* sample = nullptr;
    for (const Sample& s : m_samples)
        if (s.id() == id)
            sample = &s;
    if (!sample)
        return;
    QStringList items;
    QVector<int> ids;
    for (const Basket& b : m_baskets)
        if (b.nominationId() == sample->nominationId() && !b.isFull()) {
            items << QStringLiteral("Корзина № %1 — %2 из %3").arg(b.number()).arg(b.sampleCount()).arg(b.desiredCount());
            ids << b.id();
        }
    if (items.isEmpty()) {
        ui::Messages::info(this, QStringLiteral("Нет подходящих корзин"),
                           QStringLiteral("Нет корзин номинации «%1» со свободными местами. Создайте корзину в окне "
                                          "«Корзины».").arg(m_nominationNames.value(sample->nominationId())));
        return;
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, QStringLiteral("Добавить в корзину"),
                                                 QStringLiteral("Корзина для образца %1:").arg(sample->cipher()),
                                                 items, 0, false, &ok);
    if (!ok)
        return;
    const int basketId = ids[items.indexOf(chosen)];
    if (run([&]() { BasketService(m_ctx.db, m_ctx.audit).addSample(basketId, id); }))
        refresh();
}

void SamplesPage::printList()
{
    // Список шифров для подписи банок: порядковый номер, шифр, номинация, корзина
    QString html = QStringLiteral("<h2>Список образцов мёда</h2><table border='1' cellspacing='0' cellpadding='4' width='100%'>"
                                  "<tr><th>№</th><th>Шифр</th><th>Участник</th><th>Номинация</th><th>Корзина</th></tr>");
    for (const Sample& s : m_samples)
        html += QStringLiteral("<tr><td>%1</td><td><b>%2</b></td><td>%3</td><td>%4</td><td>%5</td></tr>")
                    .arg(s.seqNumber()).arg(s.cipher())
                    .arg(m_participants.value(s.participantId()).shortName().toHtmlEscaped(),
                         m_nominationNames.value(s.nominationId()).toHtmlEscaped(),
                         s.isInBasket() ? QStringLiteral("№ %1").arg(m_baskets.value(*s.basketId()).number())
                                        : QStringLiteral("—"));
    html += QStringLiteral("</table>");
    QTextDocument doc;
    doc.setHtml(html);
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QStringLiteral("Печать списка образцов"));
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [&doc](QPrinter* p) { doc.print(p); });
    preview.exec();
}

}  // namespace sedm::admin
