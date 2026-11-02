/**
 * @file SessionPage.cpp
 * @brief Реализация окна «Сеанс дегустации».
 */
#include "SessionPage.h"
#include "Icons.h"
#include "MonitoringService.h"
#include "SessionService.h"
#include "Theme.h"

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

SessionPage::SessionPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Сеанс дегустации"), parent)
{
    setSubtitle(QStringLiteral("Дата и время выводятся на экране дегустатора и в шапке итоговой ведомости"));
    auto* row = new QHBoxLayout;
    row->setSpacing(16);

    auto* card = new QFrame;
    card->setProperty("card", "true");
    auto* c = new QVBoxLayout(card);
    c->setContentsMargins(20, 18, 20, 18);
    c->setSpacing(12);
    auto* form = new QFormLayout;
    form->setSpacing(10);
    m_title = new QLineEdit;
    m_hasDate = new QCheckBox(QStringLiteral("указать дату и время"));
    m_startsAt = new QDateTimeEdit;
    m_startsAt->setCalendarPopup(true);
    m_startsAt->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm"));
    auto* dateRow = new QHBoxLayout;
    dateRow->addWidget(m_hasDate);
    dateRow->addWidget(m_startsAt, 1);
    form->addRow(QStringLiteral("Название:"), m_title);
    form->addRow(QStringLiteral("Дата и время:"), dateRow);
    c->addLayout(form);
    m_reentry = new QCheckBox(QStringLiteral("Разрешить повторный вход дегустаторов после завершения всех корзин "
                                             "(только просмотр своих оценок)"));
    c->addWidget(m_reentry);
    auto* save = new QPushButton(QStringLiteral("Сохранить настройки сеанса"));
    save->setProperty("kind", "primary");
    c->addWidget(save, 0, Qt::AlignLeft);

    auto* previewCard = new QFrame;
    previewCard->setProperty("card", "dark");
    auto* p = new QVBoxLayout(previewCard);
    p->setContentsMargins(18, 14, 18, 14);
    auto* previewCaption = new QLabel(QStringLiteral("Так сеанс увидят дегустаторы"));
    previewCaption->setProperty("role", "muted");
    m_preview = new QLabel;
    m_preview->setWordWrap(true);
    QFont f = m_preview->font();
    f.setPixelSize(18);
    m_preview->setFont(f);
    p->addWidget(previewCaption);
    p->addWidget(m_preview);
    c->addWidget(previewCard);
    c->addStretch(1);
    row->addWidget(card, 3);

    auto* stepsCard = new QFrame;
    stepsCard->setProperty("card", "true");
    auto* s = new QVBoxLayout(stepsCard);
    s->setContentsMargins(20, 18, 20, 18);
    auto* stepsTitle = new QLabel(QStringLiteral("Подготовка дегустации"));
    QFont bold = stepsTitle->font();
    bold.setBold(true);
    stepsTitle->setFont(bold);
    s->addWidget(stepsTitle);
    m_steps = new QVBoxLayout;
    m_steps->setSpacing(8);
    s->addLayout(m_steps);
    s->addStretch(1);
    row->addWidget(stepsCard, 2);
    body()->addLayout(row, 1);

    connect(save, &QPushButton::clicked, this, &SessionPage::save);
    connect(m_title, &QLineEdit::textChanged, this, &SessionPage::updatePreview);
    connect(m_startsAt, &QDateTimeEdit::dateTimeChanged, this, &SessionPage::updatePreview);
    connect(m_hasDate, &QCheckBox::toggled, this, [this](bool on) {
        m_startsAt->setEnabled(on);
        updatePreview();
    });
}

void SessionPage::updatePreview()
{
    TastingSession s;
    s.setTitle(m_title->text());
    if (m_hasDate->isChecked())
        s.setStartsAt(m_startsAt->dateTime());
    m_preview->setText(s.displayText());
}

void SessionPage::refresh()
{
    TastingSession session;
    ProjectSummary summary;
    if (!run([&]() {
            session = SessionService(m_ctx.db, m_ctx.audit).session();
            summary = MonitoringService(m_ctx.db).summary();
        }, false))
        return;
    m_title->setText(session.title());
    m_hasDate->setChecked(session.startsAt().isValid());
    m_startsAt->setEnabled(session.startsAt().isValid());
    m_startsAt->setDateTime(session.startsAt().isValid() ? session.startsAt()
                                                         : QDateTime(QDate::currentDate().addDays(1), QTime(10, 0)));
    m_reentry->setChecked(session.allowReentry());
    updatePreview();

    while (QLayoutItem* item = m_steps->takeAt(0)) {
        if (QLayout* l = item->layout()) {
            while (QLayoutItem* inner = l->takeAt(0)) {
                delete inner->widget();
                delete inner;
            }
        }
        delete item->widget();
        delete item;
    }
    const struct {
        bool done;
        QString text;
    } steps[] = {
        {summary.participants > 0, QStringLiteral("Реестр участников загружен (%1)").arg(summary.participants)},
        {summary.samples > 0, QStringLiteral("Образцы зарегистрированы и зашифрованы (%1)").arg(summary.samples)},
        {summary.samples > 0 && summary.unassignedSamples == 0,
         QStringLiteral("Образцы распределены по корзинам (не распределено: %1)").arg(summary.unassignedSamples)},
        {summary.teams > 0 && summary.tasters > 0,
         QStringLiteral("Команды созданы (%1), дегустаторов: %2").arg(summary.teams).arg(summary.tasters)},
        {summary.baskets > 0 && summary.unassignedBaskets == 0,
         QStringLiteral("Корзины назначены командам (без команды: %1)").arg(summary.unassignedBaskets)},
        {summary.sessionDateSet, QStringLiteral("Указаны дата и время сеанса")},
    };
    for (const auto& step : steps) {
        auto* line = new QHBoxLayout;
        auto* icon = new QLabel;
        icon->setPixmap(ui::Icons::pixmap(step.done ? QStringLiteral("check") : QStringLiteral("empty"),
                                          step.done ? ui::palette::kGreen : ui::palette::kBorderStrong, 22,
                                          devicePixelRatioF()));
        auto* text = new QLabel(step.text);
        text->setWordWrap(true);
        if (!step.done)
            text->setProperty("role", "muted");
        line->addWidget(icon, 0, Qt::AlignTop);
        line->addWidget(text, 1);
        m_steps->addLayout(line);
    }
}

void SessionPage::save()
{
    TastingSession s;
    s.setTitle(m_title->text());
    if (m_hasDate->isChecked())
        s.setStartsAt(m_startsAt->dateTime());
    s.setAllowReentry(m_reentry->isChecked());
    if (run([&]() { SessionService(m_ctx.db, m_ctx.audit).saveSession(s); }))
        refresh();
}

}  // namespace sedm::admin
