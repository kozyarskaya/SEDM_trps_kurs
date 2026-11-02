/**
 * @file Icons.cpp
 * @brief Отрисовка значков SVG средствами QSvgRenderer.
 */
#include "Icons.h"
#include "Theme.h"

#include <QFile>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace sedm::ui {

QPixmap Icons::pixmap(const QString& name, const QColor& color, int size, qreal dpr)
{
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4").arg(name, color.name()).arg(size).arg(dpr);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QString svg = Theme::resourceText(QStringLiteral(":/widgets/icons/%1.svg").arg(name));
    svg.replace(QStringLiteral("currentColor"), color.name());
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pm(QSize(size, size) * dpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
    }
    pm.setDevicePixelRatio(dpr);
    cache.insert(key, pm);
    return pm;
}

QIcon Icons::icon(const QString& name, const QColor& color, int size)
{
    QIcon icon;
    icon.addPixmap(pixmap(name, color, size, 1.0));
    icon.addPixmap(pixmap(name, color, size, 2.0));
    return icon;
}

QIcon Icons::appIcon()
{
    QIcon icon;
    QSvgRenderer renderer(QStringLiteral(":/widgets/logo_light.svg"));
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        // логотип 56×48 вписывается в квадрат по ширине, по центру
        const qreal h = size * 48.0 / 56.0;
        renderer.render(&p, QRectF(0, (size - h) / 2, size, h));
        icon.addPixmap(pm);
    }
    return icon;
}

}  // namespace sedm::ui
