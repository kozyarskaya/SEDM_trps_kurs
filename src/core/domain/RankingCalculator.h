/**
 * @file RankingCalculator.h
 * @brief Расстановка мест в номинации по среднему баллу.
 */
#pragma once

#include "SampleResult.h"

#include <QVector>

namespace sedm {

/// Дегустатор — столбец ведомости номинации.
struct TasterColumn {
    int tasterId = 0;
    QString name;        ///< «Фамилия И. О.»
    int teamNumber = 0;  ///< номер команды
};

/// Итоги одной номинации: образцы по местам и дегустаторы, оценивавшие её образцы.
struct NominationResult {
    int nominationId = 0;
    QString name;                    ///< название номинации
    QVector<SampleResult> results;   ///< образцы: по местам, без мест — в конце
    QVector<TasterColumn> tasters;   ///< по номеру команды и ФИО
};

/// Распределение мест внутри одной номинации (решение заказчика В13):
/// образцы упорядочиваются по убыванию среднего балла (округлённого до сотых);
/// равные баллы получают одно место, следующее место не пропускается
/// («плотная» нумерация 1, 2, 2, 3). Образцы без полных оценок места не получают
/// и выводятся в конце в порядке регистрации.
class RankingCalculator {
public:
    /// Сортирует результаты одной номинации и проставляет места.
    static void assignPlaces(QVector<SampleResult>& nominationResults);
    /// Группирует результаты по номинациям (в порядке их следования во входных
    /// данных) и в каждой номинации распределяет места.
    static QVector<NominationResult> rankByNomination(const QVector<SampleResult>& all);
};

}  // namespace sedm
