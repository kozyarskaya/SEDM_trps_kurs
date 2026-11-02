/**
 * @file AdminPage.cpp
 * @brief Реализация основы страниц администратора.
 */
#include "AdminPage.h"
#include "Icons.h"
#include "Messages.h"
#include "RecordFilter.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItem>
#include <QTableView>
#include <QTreeView>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

AdminPage::AdminPage(AdminContext& ctx, const QString& title, QWidget* parent)
    : QWidget(parent), m_ctx(ctx), m_title(title)
{
    setObjectName(QStringLiteral("page"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(4);
    auto* heading = new QLabel(title);
    heading->setProperty("role", "title");
    m_subtitle = new QLabel;
    m_subtitle->setProperty("role", "subtitle");
    titles->addWidget(heading);
    titles->addWidget(m_subtitle);
    header->addLayout(titles, 1);
    m_actions = new QHBoxLayout;
    m_actions->setSpacing(8);
    header->addLayout(m_actions);
    layout->addLayout(header);

    m_body = new QVBoxLayout;
    m_body->setSpacing(12);
    layout->addLayout(m_body, 1);
}

void AdminPage::setSubtitle(const QString& text)
{
    m_subtitle->setText(text);
}

QPushButton* AdminPage::addAction(const QString& text, bool primary, const QString& icon)
{
    auto* b = new QPushButton(text);
    if (primary)
        b->setProperty("kind", "primary");
    if (!icon.isEmpty())
        b->setIcon(ui::Icons::icon(icon, primary ? QColor(0x24, 0x1A, 0x0E) : ui::palette::kText));
    m_actions->addWidget(b, 0, Qt::AlignBottom);
    return b;
}

bool AdminPage::run(const std::function<void()>& action, bool changesData)
{
    const bool ok = ui::Messages::guard(this, action);
    if (ok && changesData)
        emit dataChanged();
    return ok;
}

void setupTable(QAbstractItemView* view)
{
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setAlternatingRowColors(true);
    if (auto* table = qobject_cast<QTableView*>(view)) {
        table->verticalHeader()->hide();
        table->verticalHeader()->setDefaultSectionSize(table->fontMetrics().height() + 16);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setHighlightSections(false);
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setShowGrid(false);
        table->setSortingEnabled(true);
        table->setWordWrap(false);
    }
    if (auto* tree = qobject_cast<QTreeView*>(view)) {
        tree->header()->setStretchLastSection(true);
        tree->setUniformRowHeights(true);
    }
}

QStandardItem* makeItem(const QString& text, const QVariant& sortValue)
{
    auto* item = new QStandardItem(text);
    item->setData(sortValue, RecordFilter::kSortRole);
    item->setEditable(false);
    return item;
}

QStandardItem* makeItem(const QString& text)
{
    return makeItem(text, text);
}

}  // namespace sedm::admin
