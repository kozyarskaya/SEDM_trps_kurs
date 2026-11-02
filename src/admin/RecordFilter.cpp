/**
 * @file RecordFilter.cpp
 * @brief Реализация отбора строк таблиц.
 */
#include "RecordFilter.h"

namespace sedm::admin {

RecordFilter::RecordFilter(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(kSortRole);
    setSortLocaleAware(true);
}

void RecordFilter::setText(const QString& text)
{
    m_text = text.simplified();
    invalidateFilter();
}

void RecordFilter::setKey(int key)
{
    m_key = key;
    invalidateFilter();
}

bool RecordFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QAbstractItemModel* model = sourceModel();
    if (m_key != 0) {
        const QVariant key = model->index(sourceRow, 0, sourceParent).data(kKeyRole);
        if (m_key == -1 ? (key.isValid() && key.toInt() != 0) : key.toInt() != m_key)
            return false;
    }
    if (m_text.isEmpty())
        return true;
    for (int c = 0; c < model->columnCount(sourceParent); ++c)
        if (model->index(sourceRow, c, sourceParent).data().toString().contains(m_text, Qt::CaseInsensitive))
            return true;
    return false;
}

}  // namespace sedm::admin
