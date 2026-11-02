/**
 * @file HoneySlider.h
 * @brief Ползунок оценки 0–100 с бегунком в форме медовой соты.
 */
#pragma once

#include <QWidget>
#include <optional>

namespace sedm::ui {

/// Ползунок оценки (п. 4.3.1.2 ТЗ: ввод целых оценок 0–100 ползунком и клавиатурой).
/// Вид согласован с заказчиком: шкала с медовой заливкой до бегунка, бегунок —
/// шестиугольная сота без контура и без цифр, с лёгкой тенью. Пока оценка не
/// поставлена, на шкале показан пунктирный контур соты.
///
/// Управление: щелчок или перетаскивание мышью (пальцем), клавиши «←/→» (±1),
/// «PgUp/PgDn» (±10), «Home/End» (0/100). Колесо мыши оценку не меняет, чтобы
/// случайная прокрутка страницы не исказила оценку.
class HoneySlider : public QWidget {
    Q_OBJECT
public:
    static constexpr int kMin = 0;
    static constexpr int kMax = 100;

    explicit HoneySlider(QWidget* parent = nullptr);

    std::optional<int> value() const { return m_value; }
    bool hasValue() const { return m_value.has_value(); }
    /// Установка значения программой (сигнал valueChanged не испускается).
    void setValue(std::optional<int> value);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return m_readOnly; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// Пользователь изменил оценку.
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    double unit() const;            ///< масштаб размеров по текущему шрифту
    QSizeF thumbSize() const;
    double trackLeft() const;       ///< центр бегунка при значении 0
    double trackRight() const;      ///< центр бегунка при значении 100
    int valueAt(double x) const;
    void userSet(int value);

    std::optional<int> m_value;
    bool m_readOnly = false;
    bool m_dragging = false;
};

}  // namespace sedm::ui
