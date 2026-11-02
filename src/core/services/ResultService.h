/**
 * @file ResultService.h
 * @brief Подведение итогов: средние баллы, места в номинациях, экспорт ведомости.
 */
#pragma once

#include "RankingCalculator.h"

#include <QString>
#include <QVector>

namespace sedm {

class Database;
class AuditService;

/// Сервис итогов (п. 4.3.1.1, 4.3.5 ТЗ). Средний балл образца — по всем
/// сохранённым оценкам, где заполнены все четыре критерия; места — плотная
/// нумерация 1, 2, 2, 3 (решение заказчика В13).
class ResultService {
public:
    ResultService(Database& db, AuditService& audit);

    /// Итоги по номинациям (номинации по алфавиту).
    QVector<NominationResult> compute() const;
    /// Итоговая ведомость XLSX (DomainError, если файл не записать).
    void exportXlsx(const QString& path);

private:
    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
