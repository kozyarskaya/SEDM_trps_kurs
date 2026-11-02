/**
 * @file BasketListPage.cpp
 * @brief Реализация окна «Корзины команды».
 */
#include "BasketListPage.h"
#include "Icons.h"
#include "LogoWidget.h"
#include "Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::taster {

BasketListPage::BasketListPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Тёмная шапка: логотип, команда, дегустатор, кнопки
    auto* header = new QFrame;
    header->setObjectName(QStringLiteral("header"));
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(22, 14, 22, 14);
    h->setSpacing(16);
    h->addWidget(new ui::LogoWidget(true, 43));
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    m_title = new QLabel;
    m_title->setProperty("role", "title");
    m_person = new QLabel;
    m_person->setProperty("role", "muted");
    titles->addWidget(m_title);
    titles->addWidget(m_person);
    h->addLayout(titles, 1);
    auto* refresh = new QPushButton(QStringLiteral("Обновить"));
    refresh->setProperty("kind", "onDark");
    refresh->setIcon(ui::Icons::icon(QStringLiteral("refresh"), ui::palette::kOnDark));
    m_review = new QPushButton(QStringLiteral("Мои оценки"));
    m_review->setProperty("kind", "onDark");
    auto* logout = new QPushButton(QStringLiteral("Выйти"));
    logout->setProperty("kind", "onDark");
    logout->setIcon(ui::Icons::icon(QStringLiteral("logout"), ui::palette::kOnDark));
    h->addWidget(refresh);
    h->addWidget(m_review);
    h->addWidget(logout);
    layout->addWidget(header);

    auto* body = new QWidget;
    auto* b = new QVBoxLayout(body);
    b->setContentsMargins(26, 20, 26, 20);
    b->setSpacing(14);
    auto* heading = new QLabel(QStringLiteral("Выберите корзину для оценивания"));
    heading->setProperty("role", "title");
    b->addWidget(heading);
    m_session = new QLabel;
    m_session->setProperty("role", "muted");
    b->addWidget(m_session);
    m_empty = new QLabel(QStringLiteral("Вашей команде пока не назначены корзины.\n"
                                        "Обратитесь к администратору и нажмите «Обновить»."));
    m_empty->setProperty("role", "warn");
    m_empty->hide();
    b->addWidget(m_empty);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_cards = new QWidget;
    m_grid = new QGridLayout(m_cards);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(16);
    scroll->setWidget(m_cards);
    b->addWidget(scroll, 1);
    layout->addWidget(body, 1);

    connect(refresh, &QPushButton::clicked, this, &BasketListPage::refreshRequested);
    connect(m_review, &QPushButton::clicked, this, &BasketListPage::reviewRequested);
    connect(logout, &QPushButton::clicked, this, &BasketListPage::logoutRequested);
}

void BasketListPage::setHeader(const QString& title, const QString& person, const QString& session)
{
    m_title->setText(title);
    m_person->setText(person);
    m_session->setText(session);
}

void BasketListPage::setBaskets(const QVector<TasterBasketRow>& baskets)
{
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_empty->setVisible(baskets.isEmpty());
    bool anyFinished = false;
    const int columns = 3;
    for (int i = 0; i < baskets.size(); ++i) {
        const TasterBasketRow& r = baskets[i];
        anyFinished = anyFinished || r.finished;
        auto* card = new QPushButton;
        card->setProperty("basket", true);
        card->setProperty("finished", r.finished);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setAccessibleName(QStringLiteral("Корзина № %1, %2").arg(r.basket.number()).arg(r.nominationName));
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(18, 14, 18, 14);
        v->setSpacing(6);
        auto* number = new QLabel(QStringLiteral("Корзина № %1").arg(r.basket.number()));
        number->setProperty("role", "big");
        auto* nomination = new QLabel(r.nominationName);
        nomination->setProperty("role", "criterion");
        const int total = r.basket.sampleCount();
        auto* bar = new QProgressBar;
        bar->setRange(0, qMax(1, total));
        bar->setValue(r.evaluated);
        auto* status = new QLabel;
        if (r.finished) {
            status->setText(QStringLiteral("Завершена · оценено %1 из %2 — просмотр").arg(r.evaluated).arg(total));
            status->setProperty("role", "ok");
        } else if (r.evaluated == 0) {
            status->setText(QStringLiteral("Не начата · образцов: %1").arg(total));
            status->setProperty("role", "muted");
        } else {
            status->setText(QStringLiteral("Оценено %1 из %2").arg(r.evaluated).arg(total));
            status->setProperty("role", "warn");
        }
        for (QLabel* l : {number, nomination, status})
            l->setAttribute(Qt::WA_TransparentForMouseEvents);
        bar->setAttribute(Qt::WA_TransparentForMouseEvents);
        v->addWidget(number);
        v->addWidget(nomination);
        v->addStretch(1);
        v->addWidget(bar);
        v->addWidget(status);
        const int id = r.basket.id();
        const bool finished = r.finished;
        connect(card, &QPushButton::clicked, this, [this, id, finished]() { emit basketChosen(id, finished); });
        m_grid->addWidget(card, i / columns, i % columns);
    }
    m_grid->setRowStretch(m_grid->rowCount(), 1);
    m_review->setVisible(anyFinished);
}

}  // namespace sedm::taster
