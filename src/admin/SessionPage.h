/**
 * @file SessionPage.h
 * @brief Окно «Сеанс дегустации»: название, дата и время, повторный вход, подготовка.
 */
#pragma once

#include "AdminPage.h"

class QCheckBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QVBoxLayout;

namespace sedm::admin {

/// Конфигурация сеанса (п. 4.2.1 ТЗ): дата и время проведения, разрешение
/// повторного входа дегустаторов после завершения (только просмотр, решение В14),
/// и перечень этапов подготовки дегустации с отметками выполнения.
class SessionPage : public AdminPage {
    Q_OBJECT
public:
    explicit SessionPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("session.html"); }

private:
    void save();
    void updatePreview();

    QLineEdit* m_title;
    QCheckBox* m_hasDate;
    QDateTimeEdit* m_startsAt;
    QCheckBox* m_reentry;
    QLabel* m_preview;
    QVBoxLayout* m_steps;
};

}  // namespace sedm::admin
