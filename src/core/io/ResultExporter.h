/**
 * @file ResultExporter.h
 * @brief Итоговая ведомость дегустации в формате XLSX (п. 4.3.1.1, 4.3.5 ТЗ).
 */
#pragma once

#include "RankingCalculator.h"
#include "TastingSession.h"

#include <QString>
#include <QVector>

namespace sedm {

/// Формирует книгу Excel из двух листов (решение заказчика В13):
/// «Итоги» — по номинациям: место, шифр, ФИО и регион участника, итоговый балл
/// каждого дегустатора, средний балл образца;
/// «Детализация» — каждая оценка по четырём критериям с примечанием.
class ResultExporter {
public:
    /// DomainError, если файл не удалось записать (например, открыт в Excel).
    static void write(const QString& path, const TastingSession& session,
                      const QVector<NominationResult>& nominations);
};

}  // namespace sedm
