/**
 * @file ReviewPage.cpp
 * @brief Реализация окна «Мои оценки».
 */
#include "ReviewPage.h"
#include "Icons.h"
#include "LogoWidget.h"
#include "Messages.h"
#include "Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::taster {

ReviewPage::ReviewPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame;
    header->setObjectName(QStringLiteral("header"));
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(22, 14, 22, 14);
    h->setSpacing(16);
    h->addWidget(new ui::LogoWidget(true, 43));
    auto* titles = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("Мои оценки"));
    title->setProperty("role", "title");
    m_person = new QLabel;
    m_person->setProperty("role", "muted");
    titles->addWidget(title);
    titles->addWidget(m_person);
    h->addLayout(titles, 1);
    m_back = new QPushButton(QStringLiteral("← К списку корзин"));
    m_back->setProperty("kind", "onDark");
    auto* logout = new QPushButton(QStringLiteral("Выйти"));
    logout->setProperty("kind", "onDark");
    logout->setIcon(ui::Icons::icon(QStringLiteral("logout"), ui::palette::kOnDark));
    h->addWidget(m_back);
    h->addWidget(logout);
    layout->addWidget(header);

    auto* body = new QVBoxLayout;
    body->setContentsMargins(26, 20, 26, 20);
    body->setSpacing(12);
    auto* pick = new QHBoxLayout;
    auto* pickLabel = new QLabel(QStringLiteral("Корзина:"));
    m_basket = new QComboBox;
    m_basket->setMinimumWidth(360);
    pickLabel->setBuddy(m_basket);
    pick->addWidget(pickLabel);
    pick->addWidget(m_basket);
    pick->addStretch(1);
    body->addLayout(pick);
    m_summary = new QLabel;
    m_summary->setProperty("role", "muted");
    m_summary->setWordWrap(true);
    body->addWidget(m_summary);

    m_table = new QTableWidget;
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Шифр"), criterionTitle(Criterion::Appearance),
                                        criterionTitle(Criterion::Consistency), criterionTitle(Criterion::Aroma),
                                        criterionTitle(Criterion::Taste), QStringLiteral("Итоговый балл"),
                                        QStringLiteral("Примечание")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setWordWrap(true);
    body->addWidget(m_table, 1);
    auto* note = new QLabel(QStringLiteral("Дегустация этой корзины завершена — оценки изменить нельзя."));
    note->setProperty("role", "muted");
    body->addWidget(note);
    layout->addLayout(body, 1);

    connect(m_basket, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReviewPage::showBasket);
    connect(m_back, &QPushButton::clicked, this, &ReviewPage::backRequested);
    connect(logout, &QPushButton::clicked, this, &ReviewPage::logoutRequested);
}

void ReviewPage::open(TastingService* service, Database* db, const QVector<TasterBasketRow>& baskets,
                      int basketId, bool canGoBack)
{
    m_service = service;
    m_db = db;
    m_baskets.clear();
    for (const TasterBasketRow& r : baskets)
        if (r.finished)
            m_baskets.append(r);
    m_person->setText(service->taster().fullName());
    m_back->setVisible(canGoBack);
    const QSignalBlocker block(m_basket);
    m_basket->clear();
    int current = 0;
    for (int i = 0; i < m_baskets.size(); ++i) {
        m_basket->addItem(QStringLiteral("Корзина № %1 · %2").arg(m_baskets[i].basket.number())
                              .arg(m_baskets[i].nominationName));
        if (m_baskets[i].basket.id() == basketId)
            current = i;
    }
    m_basket->setCurrentIndex(current);
    showBasket(current);
}

void ReviewPage::showBasket(int index)
{
    m_table->setRowCount(0);
    if (index < 0 || index >= m_baskets.size()) {
        m_summary->setText(QStringLiteral("Завершённых корзин пока нет."));
        return;
    }
    BasketSheet sheet;
    if (!ui::Messages::guard(this, *m_db, [&]() { sheet = m_service->openBasket(m_baskets[index].basket.id()); }))
        return;
    const QLocale ru(QLocale::Russian);
    m_table->setRowCount(sheet.samples.size());
    double sum = 0;
    int complete = 0;
    for (int row = 0; row < sheet.samples.size(); ++row) {
        const Evaluation e = sheet.savedFor(sheet.samples[row].id());
        auto* cipher = new QTableWidgetItem(sheet.samples[row].cipher());
        QFont bold = cipher->font();
        bold.setBold(true);
        cipher->setFont(bold);
        m_table->setItem(row, 0, cipher);
        for (int c = 0; c < kCriteriaCount; ++c) {
            const auto v = e.score(kAllCriteria[static_cast<size_t>(c)]);
            auto* item = new QTableWidgetItem(v ? QString::number(*v) : QStringLiteral("—"));
            item->setTextAlignment(Qt::AlignCenter);
            m_table->setItem(row, 1 + c, item);
        }
        const auto total = e.total();
        auto* t = new QTableWidgetItem(total ? ru.toString(*total, 'f', 2) : QStringLiteral("—"));
        t->setTextAlignment(Qt::AlignCenter);
        t->setFont(bold);
        m_table->setItem(row, 5, t);
        m_table->setItem(row, 6, new QTableWidgetItem(e.note()));
        if (total) {
            sum += *total;
            ++complete;
        }
    }
    m_table->resizeRowsToContents();
    m_summary->setText(QStringLiteral("Образцов: %1. Средний из ваших итоговых баллов: %2.")
                           .arg(sheet.samples.size())
                           .arg(complete > 0 ? ru.toString(sum / complete, 'f', 2) : QStringLiteral("—")));
}

}  // namespace sedm::taster
