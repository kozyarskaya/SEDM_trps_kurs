/**
 * @file MonitoringService.h
 * @brief Мониторинг хода дегустации по командам и дегустаторам.
 */
#pragma once

#include "ReportRepository.h"
#include "Team.h"

#include <QDateTime>
#include <QVector>

namespace sedm {

class Database;

/// Ход дегустации одной команды.
struct TeamProgress {
    Team team;
    int basketCount = 0;                 ///< назначено корзин
    int sampleCount = 0;                 ///< образцов в этих корзинах
    QVector<TasterProgressRow> tasters;  ///< по каждому дегустатору
    int evaluatedTotal() const;          ///< оценено всеми дегустаторами (полностью)
    int expectedTotal() const;           ///< образцов × дегустаторов
    bool isFinished() const;             ///< все дегустаторы завершили все корзины
};

/// Снимок хода дегустации для окна мониторинга (обновляется раз в 10 секунд).
struct MonitoringSnapshot {
    QDateTime takenAt;
    QVector<TeamProgress> teams;
    int samplesTotal = 0;        ///< зарегистрировано образцов
    int samplesInBaskets = 0;    ///< распределено по корзинам
    int basketsTotal = 0;
    int basketsUnassigned = 0;   ///< корзин без команды
    int evaluatedTotal() const;
    int expectedTotal() const;
};

/// Сервис мониторинга (п. 4.3.1.1 ТЗ): для каждой команды — число оценённых
/// образцов каждым дегустатором. Данные — несколькими сводными запросами.
class MonitoringService {
public:
    explicit MonitoringService(Database& db) : m_db(db) {}
    MonitoringSnapshot snapshot() const;
    /// Сводка проекта и этапы подготовки дегустации (боковая панель администратора).
    ProjectSummary summary() const;

private:
    Database& m_db;
};

}  // namespace sedm
