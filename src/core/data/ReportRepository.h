/**
 * @file ReportRepository.h
 * @brief Сводные запросы: ход дегустации, корзины дегустатора, данные итоговой ведомости.
 */
#pragma once

#include "Basket.h"
#include "SampleResult.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace sedm {

class Database;

/// Строка мониторинга хода дегустации — один дегустатор (п. 4.3.1.1 ТЗ).
struct TasterProgressRow {
    int tasterId = 0;
    QString tasterName;       ///< «Фамилия И. О.»
    int teamNumber = 0;
    QString teamName;
    int samplesAssigned = 0;  ///< образцов в корзинах команды
    int samplesEvaluated = 0; ///< из них оценено по всем четырём критериям
    int basketsAssigned = 0;  ///< корзин у команды
    int basketsFinished = 0;  ///< из них завершено дегустатором
    QDateTime lastSaved;      ///< время последнего сохранения оценок
};

/// Корзина в списке дегустатора: номинация, число образцов, оценено, завершена ли.
struct TasterBasketRow {
    Basket basket;
    QString nominationName;
    int evaluated = 0;        ///< образцов с полной оценкой этого дегустатора
    bool finished = false;    ///< дегустация корзины завершена дегустатором
};

/// Сводка проекта для боковой панели администратора: объёмы данных и
/// выполнение шести этапов подготовки дегустации.
struct ProjectSummary {
    int participants = 0;
    int nominations = 0;
    int samples = 0;
    int unassignedSamples = 0;   ///< образцов вне корзин
    int baskets = 0;
    int unassignedBaskets = 0;   ///< корзин без команды
    int teams = 0;
    int tasters = 0;
    int evaluations = 0;
    int auditEntries = 0;
    bool sessionDateSet = false;
    QString sessionTitle;

    static constexpr int kStages = 6;
    /// Выполнено этапов: реестр, образцы, распределение по корзинам, команды,
    /// назначение корзин, дата сеанса.
    int stagesDone() const;
};

/// Доступ к сводным данным. Каждый метод — один SQL-запрос, чтобы время отклика
/// не росло с числом записей (п. 4.1.5–4.1.6 ТЗ).
class ReportRepository {
public:
    explicit ReportRepository(Database& db) : m_db(db) {}

    ProjectSummary summary() const;
    QVector<TasterProgressRow> tasterProgress() const;
    QVector<TasterBasketRow> basketsForTaster(int tasterId) const;
    /// Все образцы с владельцами, номинациями и оценками всех дегустаторов.
    QVector<SampleResult> results() const;

private:
    Database& m_db;
};

}  // namespace sedm
