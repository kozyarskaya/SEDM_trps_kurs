/**
 * @file MonitoringPage.h
 * @brief Окно «Мониторинг»: ход дегустации по командам и дегустаторам.
 */
#pragma once

#include "AdminPage.h"

#include <QTimer>

class QLabel;
class QTreeWidget;

namespace sedm::admin {

/// Мониторинг хода дегустации (п. 4.3.1.1 ТЗ): для каждой команды — число
/// оценённых образцов каждым дегустатором. Пока окно открыто, данные
/// обновляются автоматически каждые 10 секунд.
class MonitoringPage : public AdminPage {
    Q_OBJECT
public:
    static constexpr int kRefreshMs = 10000;   ///< период обновления

    explicit MonitoringPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("monitoring.html"); }

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QLabel* m_samples;
    QLabel* m_evaluations;
    QLabel* m_teamsDone;
    QTreeWidget* m_tree;
    QTimer m_timer;
};

}  // namespace sedm::admin
