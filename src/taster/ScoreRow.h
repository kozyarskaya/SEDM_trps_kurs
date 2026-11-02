/**
 * @file ScoreRow.h
 * @brief Строка ввода оценки по одному критерию: ползунок-сота, «−», число, «+».
 */
#pragma once

#include "Criterion.h"

#include <QFrame>
#include <optional>

class QLabel;
class QLineEdit;
class QPushButton;

namespace sedm::ui {
class HoneySlider;
}

namespace sedm::taster {

/// Оценка по критерию вводится ползунком (мышью или пальцем), клавишами
/// стрелок, кнопками «−»/«+» (по 1 баллу) или числом с клавиатуры (0–100).
/// Все способы ввода согласованы между собой.
class ScoreRow : public QFrame {
    Q_OBJECT
public:
    explicit ScoreRow(Criterion criterion, QWidget* parent = nullptr);

    Criterion criterion() const { return m_criterion; }
    std::optional<int> value() const { return m_value; }
    /// Показ сохранённой оценки (сигнал valueChanged не испускается).
    void setValue(std::optional<int> value);
    void setReadOnly(bool readOnly);

signals:
    void valueChanged(Criterion criterion, int value);

private:
    void userSet(int value);
    void showValue();

    Criterion m_criterion;
    std::optional<int> m_value;
    QLabel* m_title;
    ui::HoneySlider* m_slider;
    QPushButton* m_minus;
    QLineEdit* m_edit;
    QPushButton* m_plus;
};

}  // namespace sedm::taster
