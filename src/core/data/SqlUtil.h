/**
 * @file SqlUtil.h
 * @brief Вспомогательные преобразования значений между C++ и SQL (NULL, необязательные поля).
 */
#pragma once

#include <QString>
#include <QVariant>
#include <optional>

namespace sedm::sql {

/// std::optional<int> -> QVariant (пусто -> NULL типа integer).
inline QVariant toVariant(const std::optional<int>& value)
{
    return value ? QVariant(*value) : QVariant(QVariant::Int);
}

/// Текст для столбца NOT NULL: пустая строка Qt (null QString) передаётся как '', а не NULL.
inline QVariant text(const QString& value)
{
    return value.isNull() ? QVariant(QStringLiteral("")) : QVariant(value);
}

/// QVariant (возможно NULL) -> std::optional<int>.
inline std::optional<int> optInt(const QVariant& value)
{
    if (value.isNull())
        return std::nullopt;
    return value.toInt();
}

/// Заключает идентификатор в кавычки PostgreSQL ("имя").
inline QString quoteIdent(const QString& name)
{
    QString s = name;
    s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + s + QLatin1Char('"');
}

/// Заключает строку в апострофы PostgreSQL ('текст'); standard_conforming_strings = on.
inline QString quoteLiteral(const QString& text)
{
    QString s = text;
    s.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + s + QLatin1Char('\'');
}

}  // namespace sedm::sql
