/**
 * @file DemoSeeder.h
 * @brief Заполнение базы демонстрационными данными конкурса (для показа и испытаний).
 */
#pragma once

#include "TeamService.h"

#include <QVector>

namespace sedm {

class Database;
class AuditService;

/// Параметры демонстрационного конкурса.
struct DemoOptions {
    int participants = 40;     ///< участников (у каждого — один образец)
    int teams = 4;             ///< дегустационных команд
    int tastersPerTeam = 3;    ///< дегустаторов в команде
    int basketSize = 6;        ///< образцов в корзине
    bool evaluate = false;     ///< выставить оценки всех образцов (испытание производительности)
    quint32 seed = 2026;       ///< начальное значение генератора (повторяемые данные)
};

/// Итог заполнения: данные для карточек дегустаторов и объём данных.
struct DemoResult {
    QString adminPassword;
    QVector<TasterCredentials> credentials;
    int samples = 0;
    int baskets = 0;
    int evaluations = 0;
};

/// Заполняет пустую базу СЭДМ правдоподобными данными через прикладные сервисы:
/// номинации, участники, образцы, команды с дегустаторами, корзины, сеанс.
class DemoSeeder {
public:
    static DemoResult seed(Database& db, AuditService& audit, const DemoOptions& options);
};

}  // namespace sedm
