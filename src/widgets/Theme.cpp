/**
 * @file Theme.cpp
 * @brief Загрузка таблицы стилей и подстановка масштабированных размеров.
 */
#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QRegularExpression>
#include <QStyle>
#include <QVariant>
#include <QWidget>
#include <QtMath>

namespace sedm::ui {

QString Theme::resourceText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll());
}

QString Theme::styleSheet(int basePx, double scale, const QString& extraQss)
{
    QString qss = resourceText(QStringLiteral(":/widgets/theme.qss")) + QLatin1Char('\n') + extraQss;
    qss.replace(QStringLiteral("{{base}}"), QString::number(qRound(basePx * scale)));
    // {{N}} — размер в пикселях при масштабе 1: умножается на коэффициент
    static const QRegularExpression token(QStringLiteral("\\{\\{(\\d+)\\}\\}"));
    QString out;
    out.reserve(qss.size());
    int last = 0;
    auto it = token.globalMatch(qss);
    while (it.hasNext()) {
        const auto m = it.next();
        out += qss.midRef(last, m.capturedStart() - last);
        out += QString::number(qMax(1, qRound(m.captured(1).toInt() * scale)));
        last = m.capturedEnd();
    }
    out += qss.midRef(last);
    return out;
}

void Theme::apply(QApplication& app, int basePx, double scale, const QString& extraQss)
{
    app.setStyle(QStringLiteral("Fusion"));   // одинаковый вид на всех версиях Windows
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(qRound(basePx * scale));
    app.setFont(font);
    app.setStyleSheet(styleSheet(basePx, scale, extraQss));
}

void Theme::setProp(QWidget* widget, const char* name, const QVariant& value)
{
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}  // namespace sedm::ui
