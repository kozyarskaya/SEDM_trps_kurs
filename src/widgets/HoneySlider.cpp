/**
 * @file HoneySlider.cpp
 * @brief Отрисовка и управление ползунком-сотой.
 *
 * Геометрия повторяет утверждённый макет при шрифте 18 px: высота 60, шкала
 * высотой 16 с отступами по 18 (половина ширины бегунка), бегунок 36×40.
 * Все размеры пропорциональны высоте шрифта, поэтому ползунок крупнеет вместе
 * с текстом по кнопке «A+».
 */
#include "HoneySlider.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace sedm::ui {

namespace {
/// Шестиугольник соты в координатах макета (поле 48×54), масштабированный в rect.
QPainterPath hexagon(const QRectF& rect, double inset)
{
    const double sx = rect.width() / 48.0;
    const double sy = rect.height() / 54.0;
    // вершины: верх, правый верх, правый низ, низ, левый низ, левый верх
    const double a = 2 + inset * 7, b = 14 + inset * 3.5, c = 40 - inset * 3.5, d = 52 - inset * 7;
    const double l = 3 + inset * 6, r = 45 - inset * 6;
    QPainterPath path;
    path.moveTo(rect.left() + 24 * sx, rect.top() + a * sy);
    path.lineTo(rect.left() + r * sx, rect.top() + b * sy);
    path.lineTo(rect.left() + r * sx, rect.top() + c * sy);
    path.lineTo(rect.left() + 24 * sx, rect.top() + d * sy);
    path.lineTo(rect.left() + l * sx, rect.top() + c * sy);
    path.lineTo(rect.left() + l * sx, rect.top() + b * sy);
    path.closeSubpath();
    return path;
}
}  // namespace

HoneySlider::HoneySlider(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

double HoneySlider::unit() const
{
    return qMax(0.6, fontMetrics().height() / 24.0);   // 24 px — высота строки шрифта 18 px
}

QSizeF HoneySlider::thumbSize() const
{
    return QSizeF(36 * unit(), 40 * unit());
}

double HoneySlider::trackLeft() const
{
    return thumbSize().width() / 2;
}

double HoneySlider::trackRight() const
{
    return width() - thumbSize().width() / 2;
}

QSize HoneySlider::sizeHint() const
{
    return QSize(qRound(320 * unit()), qRound(60 * unit()));
}

QSize HoneySlider::minimumSizeHint() const
{
    return QSize(qRound(160 * unit()), qRound(60 * unit()));
}

void HoneySlider::setValue(std::optional<int> value)
{
    if (value)
        value = qBound(kMin, *value, kMax);
    if (value == m_value)
        return;
    m_value = value;
    setAccessibleDescription(m_value ? QString::number(*m_value) : QStringLiteral("оценка не поставлена"));
    update();
}

void HoneySlider::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    setCursor(readOnly ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

int HoneySlider::valueAt(double x) const
{
    const double span = qMax(1.0, trackRight() - trackLeft());
    return qBound(kMin, qRound((x - trackLeft()) / span * (kMax - kMin)) + kMin, kMax);
}

void HoneySlider::userSet(int value)
{
    value = qBound(kMin, value, kMax);
    if (m_value == value)
        return;
    setValue(value);
    emit valueChanged(value);
}

void HoneySlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double u = unit();
    const double trackH = 16 * u;
    const double top = (height() - trackH) / 2;
    const bool active = isEnabled() && !m_readOnly;

    // Шкала с лёгкой внутренней тенью
    const QRectF track(trackLeft(), top, trackRight() - trackLeft(), trackH);
    QPainterPath trackPath;
    trackPath.addRoundedRect(track.adjusted(-trackH / 2, 0, trackH / 2, 0), trackH / 2, trackH / 2);
    p.fillPath(trackPath, active ? QColor(0xEF, 0xE1, 0xC4) : QColor(0xF1, 0xEA, 0xDB));
    p.save();
    p.setClipPath(trackPath);
    p.setPen(QPen(QColor(59, 42, 26, 55), 1.2 * u));
    p.drawLine(QPointF(track.left() - trackH / 2, top + 0.8 * u), QPointF(track.right() + trackH / 2, top + 0.8 * u));
    p.restore();

    const QSizeF ts = thumbSize();
    if (!m_value) {
        // Оценка не поставлена: пунктирная сота в начале шкалы
        const QRectF thumb(trackLeft() - ts.width() / 2, (height() - ts.height()) / 2, ts.width(), ts.height());
        QPen dashed(palette::kBorderStrong, 1.6 * u, Qt::DashLine);
        dashed.setDashPattern({3, 2.5});
        p.setPen(dashed);
        p.setBrush(QColor(0xFF, 0xFF, 0xFF, 200));
        p.drawPath(hexagon(thumb.adjusted(u, u, -u, -u), 0));
    } else {
        const double ratio = double(*m_value - kMin) / (kMax - kMin);
        const double cx = trackLeft() + (trackRight() - trackLeft()) * ratio;
        // Медовая заливка шкалы до центра бегунка
        if (cx > track.left()) {
            QPainterPath fill;
            fill.addRoundedRect(QRectF(track.left() - trackH / 2, top, cx - track.left() + trackH / 2, trackH),
                                trackH / 2, trackH / 2);
            QLinearGradient g(track.topLeft(), QPointF(cx, top));
            g.setColorAt(0, active ? QColor(0xF7, 0xCF, 0x6B) : QColor(0xEA, 0xDB, 0xB5));
            g.setColorAt(1, active ? QColor(0xE8, 0xA3, 0x17) : QColor(0xD9, 0xBE, 0x7E));
            p.fillPath(fill, g);
        }
        // Бегунок-сота: тень, внешний и внутренний шестиугольники
        const QRectF thumb(cx - ts.width() / 2, (height() - ts.height()) / 2, ts.width(), ts.height());
        p.setPen(Qt::NoPen);
        p.fillPath(hexagon(thumb.translated(0, 1.6 * u), 0), QColor(59, 42, 26, 45));
        p.fillPath(hexagon(thumb.translated(0, 0.8 * u), 0), QColor(59, 42, 26, 60));
        p.fillPath(hexagon(thumb, 0), active ? QColor(0xE0, 0x9A, 0x10) : QColor(0xCD, 0xB2, 0x7A));
        p.fillPath(hexagon(thumb, 1), active ? QColor(0xF5, 0xC4, 0x51) : QColor(0xE6, 0xD4, 0xA4));
    }

    // Рамка фокуса клавиатуры
    if (hasFocus()) {
        p.setPen(QPen(palette::kBrown, 2 * u));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(u, u, -u, -u), 8 * u, 8 * u);
    }
}

void HoneySlider::mousePressEvent(QMouseEvent* event)
{
    if (m_readOnly || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    userSet(valueAt(event->localPos().x()));
}

void HoneySlider::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging)
        userSet(valueAt(event->localPos().x()));
}

void HoneySlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        userSet(valueAt(event->localPos().x()));
        m_dragging = false;
    }
}

void HoneySlider::keyPressEvent(QKeyEvent* event)
{
    if (m_readOnly) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int current = m_value.value_or(kMin);
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        userSet(current - 1);
        break;
    case Qt::Key_Right:
    case Qt::Key_Up:
        userSet(m_value ? current + 1 : kMin + 1);
        break;
    case Qt::Key_PageDown:
        userSet(current - 10);
        break;
    case Qt::Key_PageUp:
        userSet(current + 10);
        break;
    case Qt::Key_Home:
        userSet(kMin);
        break;
    case Qt::Key_End:
        userSet(kMax);
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void HoneySlider::wheelEvent(QWheelEvent* event)
{
    event->ignore();   // прокрутка — странице, а не оценке
}

void HoneySlider::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
}

void HoneySlider::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

void HoneySlider::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();   // размеры ползунка следуют за размером шрифта
}

}  // namespace sedm::ui
