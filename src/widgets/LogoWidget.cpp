/**
 * @file LogoWidget.cpp
 * @brief Отрисовка логотипа из ресурса SVG.
 */
#include "LogoWidget.h"

#include <QPainter>

namespace sedm::ui {

LogoWidget::LogoWidget(bool onDark, int height, QWidget* parent)
    : QWidget(parent),
      m_renderer(onDark ? QStringLiteral(":/widgets/logo_dark.svg") : QStringLiteral(":/widgets/logo_light.svg")),
      m_height(height)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("Логотип СЭДМ"));
}

QSize LogoWidget::sizeHint() const
{
    return QSize(qRound(m_height * 56.0 / 48.0), m_height);
}

void LogoWidget::setLogoHeight(int height)
{
    m_height = height;
    updateGeometry();
    update();
}

void LogoWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QSize s = sizeHint();
    m_renderer.render(&p, QRectF((width() - s.width()) / 2.0, (height() - s.height()) / 2.0, s.width(), s.height()));
}

}  // namespace sedm::ui
