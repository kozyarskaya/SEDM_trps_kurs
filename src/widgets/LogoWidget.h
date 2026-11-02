/**
 * @file LogoWidget.h
 * @brief Логотип СЭДМ — медовая сота с пчёлкой (одинаковый на всех экранах).
 */
#pragma once

#include <QSvgRenderer>
#include <QWidget>

namespace sedm::ui {

/// Логотип в двух вариантах: для тёмного фона (светлый контур) и для светлого.
class LogoWidget : public QWidget {
    Q_OBJECT
public:
    /// height — высота логотипа в пикселях (ширина — по пропорциям 56×48).
    explicit LogoWidget(bool onDark, int height = 43, QWidget* parent = nullptr);
    QSize sizeHint() const override;
    void setLogoHeight(int height);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSvgRenderer m_renderer;
    int m_height;
};

}  // namespace sedm::ui
