/**
 * @file Theme.h
 * @brief Оформление интерфейса СЭДМ: светлая «медовая» тема (вариант Г1).
 */
#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QVariant;
class QWidget;

namespace sedm::ui {

/// Цвета палитры Г1 (используются при собственной отрисовке элементов).
namespace palette {
const QColor kBackground(0xFB, 0xF7, 0xEE);   ///< фон окон
const QColor kText(0x2B, 0x21, 0x18);         ///< основной текст
const QColor kMuted(0x5E, 0x4B, 0x37);        ///< вторичный текст
const QColor kHoney(0xE8, 0xA3, 0x17);        ///< акцент (кнопки, выделение)
const QColor kHoneyLight(0xFC, 0xEF, 0xD2);   ///< светло-медовый
const QColor kBrown(0x3B, 0x2A, 0x1A);        ///< тёмно-коричневый (панели)
const QColor kOnDark(0xFB, 0xF3, 0xE4);       ///< светлый текст на тёмном
const QColor kBorder(0xE3, 0xD6, 0xBF);       ///< рамки
const QColor kBorderStrong(0xC9, 0xB4, 0x8F); ///< рамки полей
const QColor kGreen(0x2E, 0x6B, 0x30);        ///< «оценён», «подключено»
const QColor kAmber(0x8A, 0x4B, 0x00);        ///< «частично», предупреждения
const QColor kRed(0x8E, 0x1F, 0x1F);          ///< ошибки, удаление
}  // namespace palette

/// Тема оформления: таблица стилей Qt (QSS) из ресурсов с подстановкой размеров.
class Theme {
public:
    /// basePx — базовый размер шрифта в пикселях (администратор 14, дегустатор 18);
    /// scale — коэффициент масштаба (кнопки «A+»/«A−»); extraQss — дополнительные
    /// стили программы в том же формате {{N}}.
    static void apply(QApplication& app, int basePx, double scale = 1.0, const QString& extraQss = QString());
    /// Таблица стилей с подставленными размерами.
    static QString styleSheet(int basePx, double scale, const QString& extraQss = QString());
    /// Текст ресурса (например, «:/widgets/theme.qss»).
    static QString resourceText(const QString& path);
    /// Меняет динамическое свойство элемента (role, kind …) и обновляет его оформление.
    static void setProp(QWidget* widget, const char* name, const QVariant& value);
};

}  // namespace sedm::ui
