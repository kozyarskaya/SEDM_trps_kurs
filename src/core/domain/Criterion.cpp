/**
 * @file Criterion.cpp
 * @brief Названия критериев оценки.
 */
#include "Criterion.h"

namespace sedm {

QString criterionTitle(Criterion criterion)
{
    switch (criterion) {
    case Criterion::Appearance:  return QStringLiteral("Вид и цвет");
    case Criterion::Consistency: return QStringLiteral("Консистенция");
    case Criterion::Aroma:       return QStringLiteral("Запах и аромат");
    case Criterion::Taste:       return QStringLiteral("Вкус");
    }
    return QString();
}

}  // namespace sedm
