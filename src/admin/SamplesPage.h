/**
 * @file SamplesPage.h
 * @brief Окно «Образцы»: регистрация образцов, шифры, карточка образца (макет Б1).
 */
#pragma once

#include "AdminPage.h"
#include "Basket.h"
#include "Participant.h"
#include "Sample.h"

#include <QHash>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTableView;

namespace sedm::admin {

class RecordFilter;

/// Реестр образцов (п. 4.3.1.1 ТЗ): регистрация с автоматическим сквозным номером
/// и уникальным шифром, изменение и удаление, отбор по номинации и корзине,
/// добавление в корзину, печать списка шифров для подписи банок.
class SamplesPage : public AdminPage {
    Q_OBJECT
public:
    explicit SamplesPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("samples.html"); }

private:
    void registerSample();
    void registerForAll();
    void editSample();
    void removeSample();
    void addToBasket();
    void printList();
    void showCard();
    int selectedSampleId() const;

    QVector<Sample> m_samples;
    QHash<int, Participant> m_participants;
    QHash<int, QString> m_nominationNames;
    QHash<int, Basket> m_baskets;

    QLineEdit* m_search;
    QComboBox* m_nominationFilter;
    QComboBox* m_basketFilter;
    QStandardItemModel* m_model;
    RecordFilter* m_filter;
    QTableView* m_table;
    QLabel* m_cardNumber;
    QLabel* m_cardCipher;
    QLabel* m_cardOwner;
    QLabel* m_cardNomination;
    QLabel* m_cardBasket;
    QPushButton* m_toBasket;
    QPushButton* m_edit;
    QPushButton* m_remove;
};

}  // namespace sedm::admin
