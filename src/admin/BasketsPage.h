/**
 * @file BasketsPage.h
 * @brief Окно «Корзины»: создание корзин, состав, назначение командам.
 */
#pragma once

#include "AdminPage.h"
#include "Basket.h"
#include "Participant.h"
#include "Sample.h"
#include "Team.h"

#include <QHash>

class QComboBox;
class QGroupBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace sedm::admin {

/// Корзины (п. 4.3.1.1 ТЗ): номер, номинация, желаемое количество образцов;
/// состав пополняется из выпадающего списка доступных образцов той же номинации
/// с контролем количества; удалённый из корзины образец возвращается в пул;
/// корзина назначается одной команде.
class BasketsPage : public AdminPage {
    Q_OBJECT
public:
    explicit BasketsPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("baskets.html"); }

private:
    void createBasket();
    void showBasket();
    void saveBasket();
    void removeBasket();
    void addSample();
    void removeSample();
    void fillFromPool();
    int selectedBasketId() const;

    QVector<Basket> m_baskets;
    QHash<int, QString> m_nominations;
    QHash<int, Team> m_teams;
    QHash<int, Participant> m_participants;

    QTableWidget* m_list;
    QGroupBox* m_editor;
    QSpinBox* m_number;
    QComboBox* m_nomination;
    QSpinBox* m_desired;
    QComboBox* m_team;
    QLabel* m_fillText;
    QProgressBar* m_fill;
    QTableWidget* m_content;
    QComboBox* m_pool;
    QPushButton* m_add;
    QPushButton* m_fillPool;
};

}  // namespace sedm::admin
