/**
 * @file ResultService.cpp
 * @brief Реализация подведения итогов.
 */
#include "ResultService.h"
#include "AuditService.h"
#include "ReportRepository.h"
#include "ResultExporter.h"
#include "SessionRepository.h"

#include <QFileInfo>

namespace sedm {

ResultService::ResultService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

QVector<NominationResult> ResultService::compute() const
{
    return RankingCalculator::rankByNomination(ReportRepository(m_db).results());
}

void ResultService::exportXlsx(const QString& path)
{
    const QVector<NominationResult> results = compute();
    ResultExporter::write(path, SessionRepository(m_db).load(), results);
    int samples = 0;
    for (const NominationResult& n : results)
        samples += n.results.size();
    m_audit.log(QStringLiteral("Экспорт итоговой ведомости"),
                QStringLiteral("Файл «%1»: номинаций %2, образцов %3")
                    .arg(QFileInfo(path).fileName()).arg(results.size()).arg(samples));
}

}  // namespace sedm
