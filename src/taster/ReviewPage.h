/**
 * @file ReviewPage.h
 * @brief Окно «Мои оценки» — просмотр своих оценок завершённых корзин (только чтение).
 */
#pragma once

#include "TastingService.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace sedm::taster {

/// Состояние «Просмотр оценок» диаграммы состояний: после завершения корзины
/// (или при повторном входе, если он разрешён) дегустатор видит свои оценки,
/// но изменить их не может (решение заказчика В14).
class ReviewPage : public QWidget {
    Q_OBJECT
public:
    explicit ReviewPage(QWidget* parent = nullptr);

    /// canGoBack — есть незавершённые корзины (кнопка «К списку корзин»).
    void open(TastingService* service, Database* db, const QVector<TasterBasketRow>& baskets,
              int basketId, bool canGoBack);

signals:
    void backRequested();
    void logoutRequested();

private:
    void showBasket(int index);

    TastingService* m_service = nullptr;
    Database* m_db = nullptr;
    QVector<TasterBasketRow> m_baskets;
    QLabel* m_person;
    QComboBox* m_basket;
    QLabel* m_summary;
    QTableWidget* m_table;
    QPushButton* m_back;
};

}  // namespace sedm::taster
