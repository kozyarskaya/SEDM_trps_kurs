/**
 * @file BasketListPage.h
 * @brief Окно «Корзины команды»: корзины, назначенные команде, и их состояние.
 */
#pragma once

#include "ReportRepository.h"

#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;

namespace sedm::taster {

/// Состояние «Выбор корзины» диаграммы состояний: карточки корзин команды —
/// номер, номинация, сколько образцов оценено; завершённые корзины отмечены.
class BasketListPage : public QWidget {
    Q_OBJECT
public:
    explicit BasketListPage(QWidget* parent = nullptr);

    /// title — «Команда 2 «Эксперты»», person — ФИО дегустатора, session — строка сеанса.
    void setHeader(const QString& title, const QString& person, const QString& session);
    void setBaskets(const QVector<TasterBasketRow>& baskets);

signals:
    void basketChosen(int basketId, bool finished);
    void refreshRequested();
    void reviewRequested();
    void logoutRequested();

private:
    QLabel* m_title;
    QLabel* m_person;
    QLabel* m_session;
    QLabel* m_empty;
    QWidget* m_cards;
    QGridLayout* m_grid;
    QPushButton* m_review;
};

}  // namespace sedm::taster
