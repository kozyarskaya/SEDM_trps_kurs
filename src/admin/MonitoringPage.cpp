/**
 * @file MonitoringPage.cpp
 * @brief Реализация окна «Мониторинг».
 */
#include "MonitoringPage.h"
#include "Database.h"
#include "DbError.h"
#include "MonitoringService.h"
#include "Theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

namespace {
constexpr int kProgressRole = Qt::UserRole + 10;   ///< доля 0…1 для полосы прогресса

/// Полоса хода оценивания в ячейке дерева (медовая заливка).
class ProgressDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QVariant ratio = index.data(kProgressRole);
        if (!ratio.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString text = opt.text;
        opt.text.clear();
        const QWidget* view = opt.widget;
        QStyle* style = view ? view->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, view);   // фон и выделение без текста
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect r = option.rect.adjusted(8, 0, -8, 0);
        const int barH = 10;
        const QRectF bar(r.left(), r.center().y() - barH / 2.0, r.width() * 0.55, barH);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0xEF, 0xE1, 0xC4));
        painter->drawRoundedRect(bar, barH / 2.0, barH / 2.0);
        painter->setBrush(ratio.toDouble() >= 1.0 ? ui::palette::kGreen : ui::palette::kHoney);
        painter->drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * qBound(0.0, ratio.toDouble(), 1.0), barH),
                                 barH / 2.0, barH / 2.0);
        painter->setPen(ui::palette::kText);
        painter->drawText(QRectF(bar.right() + 10, r.top(), r.width() - bar.width() - 10, r.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
    }
};

QLabel* summaryCard(QHBoxLayout* row, const QString& caption)
{
    auto* card = new QFrame;
    card->setProperty("card", "true");
    auto* l = new QVBoxLayout(card);
    l->setContentsMargins(16, 12, 16, 12);
    auto* c = new QLabel(caption);
    c->setProperty("role", "muted");
    auto* v = new QLabel(QStringLiteral("—"));
    v->setProperty("role", "big");
    l->addWidget(c);
    l->addWidget(v);
    row->addWidget(card, 1);
    return v;
}
}  // namespace

MonitoringPage::MonitoringPage(AdminContext& ctx, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Мониторинг хода дегустации"), parent)
{
    auto* update = addAction(QStringLiteral("Обновить"), false, QStringLiteral("refresh"));
    connect(update, &QPushButton::clicked, this, &MonitoringPage::refresh);

    auto* cards = new QHBoxLayout;
    cards->setSpacing(16);
    m_samples = summaryCard(cards, QStringLiteral("Образцов в корзинах"));
    m_evaluations = summaryCard(cards, QStringLiteral("Оценок выставлено"));
    m_teamsDone = summaryCard(cards, QStringLiteral("Команд завершили работу"));
    body()->addLayout(cards);

    m_tree = new QTreeWidget;
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({QStringLiteral("Команда / дегустатор"), QStringLiteral("Оценено образцов"),
                             QStringLiteral("Корзин завершено"), QStringLiteral("Последнее сохранение")});
    setupTable(m_tree);
    m_tree->setItemDelegateForColumn(1, new ProgressDelegate(m_tree));
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_tree->header()->resizeSection(1, 340);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setRootIsDecorated(true);
    body()->addWidget(m_tree, 1);

    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, &MonitoringPage::refresh);
}

void MonitoringPage::showEvent(QShowEvent* event)
{
    AdminPage::showEvent(event);
    m_timer.start();
}

void MonitoringPage::hideEvent(QHideEvent* event)
{
    AdminPage::hideEvent(event);
    m_timer.stop();
}

void MonitoringPage::refresh()
{
    MonitoringSnapshot snap;
    try {
        snap = MonitoringService(m_ctx.db).snapshot();
    } catch (const DbError& e) {
        // автоматическое обновление: без окна сообщения, причина — в подзаголовке
        setSubtitle(QStringLiteral("Нет данных: %1").arg(e.message()));
        if (e.kind() == DbError::Kind::Connection)
            m_ctx.db.ping();
        return;
    }
    m_samples->setText(QStringLiteral("%1 из %2").arg(snap.samplesInBaskets).arg(snap.samplesTotal));
    const int expected = snap.expectedTotal();
    m_evaluations->setText(expected > 0 ? QStringLiteral("%1 из %2 (%3 %)").arg(snap.evaluatedTotal()).arg(expected)
                                              .arg(qRound(100.0 * snap.evaluatedTotal() / expected))
                                        : QStringLiteral("—"));
    int done = 0;
    for (const TeamProgress& t : snap.teams)
        done += t.isFinished() ? 1 : 0;
    m_teamsDone->setText(QStringLiteral("%1 из %2").arg(done).arg(snap.teams.size()));

    // запоминаем свёрнутые команды, чтобы обновление не сбивало вид
    QSet<int> collapsed;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (!m_tree->topLevelItem(i)->isExpanded())
            collapsed.insert(m_tree->topLevelItem(i)->data(0, Qt::UserRole).toInt());
    m_tree->clear();
    for (const TeamProgress& t : snap.teams) {
        auto* team = new QTreeWidgetItem(m_tree);
        team->setText(0, QStringLiteral("%1 — корзин %2, образцов %3").arg(t.team.displayName()).arg(t.basketCount)
                             .arg(t.sampleCount));
        QFont bold = team->font(0);
        bold.setBold(true);
        team->setFont(0, bold);
        team->setData(0, Qt::UserRole, t.team.id());
        if (t.expectedTotal() > 0) {
            team->setText(1, QStringLiteral("%1 из %2").arg(t.evaluatedTotal()).arg(t.expectedTotal()));
            team->setData(1, kProgressRole, double(t.evaluatedTotal()) / t.expectedTotal());
        } else {
            team->setText(1, t.tasters.isEmpty() ? QStringLiteral("нет дегустаторов") : QStringLiteral("нет корзин"));
            team->setForeground(1, ui::palette::kAmber);
        }
        team->setText(2, t.isFinished() ? QStringLiteral("все завершены") : QString());
        for (const TasterProgressRow& r : t.tasters) {
            auto* taster = new QTreeWidgetItem(team);
            taster->setText(0, r.tasterName);
            taster->setText(1, QStringLiteral("%1 из %2").arg(r.samplesEvaluated).arg(r.samplesAssigned));
            if (r.samplesAssigned > 0)
                taster->setData(1, kProgressRole, double(r.samplesEvaluated) / r.samplesAssigned);
            taster->setText(2, QStringLiteral("%1 из %2").arg(r.basketsFinished).arg(r.basketsAssigned));
            taster->setText(3, r.lastSaved.isValid() ? r.lastSaved.toString(QStringLiteral("dd.MM HH:mm"))
                                                     : QStringLiteral("—"));
        }
        team->setExpanded(!collapsed.contains(t.team.id()));
    }
    m_tree->resizeColumnToContents(2);
    m_tree->resizeColumnToContents(3);
    setSubtitle(QStringLiteral("Обновлено в %1 · автоматически каждые 10 секунд")
                    .arg(snap.takenAt.toString(QStringLiteral("HH:mm:ss"))));
}

}  // namespace sedm::admin
