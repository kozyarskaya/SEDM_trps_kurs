/**
 * @file Criterion.h
 * @brief Критерии органолептической оценки мёда и границы шкалы оценок.
 *
 * Программа: СЭДМ — программная система экспертной дегустации мёда.
 * Разработчик: Козярская С. И., гр. ИУ6-73Б, МГТУ им. Н.Э. Баумана, 2026.
 */
#pragma once

#include <QString>
#include <array>

namespace sedm {

/// Критерий оценки образца мёда (п. 4.3.1.2 ТЗ).
enum class Criterion {
    Appearance = 0,   ///< вид и цвет
    Consistency = 1,  ///< консистенция
    Aroma = 2,        ///< запах и аромат
    Taste = 3         ///< вкус
};

constexpr int kCriteriaCount = 4;  ///< количество критериев оценки
constexpr int kMinScore = 0;       ///< наименьшая допустимая оценка
constexpr int kMaxScore = 100;     ///< наибольшая допустимая оценка

/// Все критерии в порядке вывода на экран и в ведомость.
constexpr std::array<Criterion, kCriteriaCount> kAllCriteria = {
    Criterion::Appearance, Criterion::Consistency, Criterion::Aroma, Criterion::Taste};

/// Возвращает название критерия для интерфейса и ведомости («Вид и цвет» и т. д.).
QString criterionTitle(Criterion criterion);

/// Проверяет, что оценка лежит в шкале [kMinScore; kMaxScore].
constexpr bool isValidScore(int value) { return value >= kMinScore && value <= kMaxScore; }

}  // namespace sedm
