/**
 * @file Icons.h
 * @brief Значки интерфейса из ресурсов SVG с заданным цветом.
 */
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace sedm::ui {

/// Контурные значки (боковая панель, кнопки, состояние образцов).
/// Цвет линий задаётся при загрузке: в SVG используется «currentColor».
class Icons {
public:
    static QIcon icon(const QString& name, const QColor& color, int size = 24);
    static QPixmap pixmap(const QString& name, const QColor& color, int size, qreal dpr = 1.0);
    /// Значок приложения (логотип СЭДМ).
    static QIcon appIcon();
};

}  // namespace sedm::ui
