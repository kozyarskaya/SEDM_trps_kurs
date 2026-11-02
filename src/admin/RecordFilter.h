/**
 * @file RecordFilter.h
 * @brief Отбор строк таблиц администратора: поиск по тексту и по значению столбца.
 */
#pragma once

#include <QSortFilterProxyModel>

namespace sedm::admin {

/// Модель-фильтр: строка видна, если текст поиска встречается в любом столбце
/// и (при заданном отборе) значение Qt::UserRole + 1 первого столбца совпадает
/// с выбранным ключом (например, идентификатором номинации).
class RecordFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    static constexpr int kKeyRole = Qt::UserRole + 1;    ///< ключ отбора (номинация, корзина …)
    static constexpr int kSortRole = Qt::UserRole + 2;   ///< значение для сортировки (числа)

    explicit RecordFilter(QObject* parent = nullptr);
    void setText(const QString& text);
    /// key = 0 — без отбора; −1 — только строки с пустым ключом.
    void setKey(int key);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_text;
    int m_key = 0;
};

}  // namespace sedm::admin
