/**
 * @file SampleRepository.h
 * @brief Хранение образцов мёда (таблица sample).
 */
#pragma once

#include "Sample.h"

#include <QSet>
#include <QVector>
#include <optional>

namespace sedm {

class Database;

/// Доступ к образцам. Сквозной порядковый номер выдаёт последовательность БД,
/// шифр — CipherGenerator (уникальность дополнительно гарантирует ограничение UNIQUE).
class SampleRepository {
public:
    explicit SampleRepository(Database& db) : m_db(db) {}

    QVector<Sample> list() const;                       ///< по порядковому номеру
    std::optional<Sample> byId(int id) const;
    QSet<QString> usedCiphers() const;                  ///< все выданные шифры
    void insert(Sample& sample) const;                  ///< задаёт id и порядковый номер
    void update(const Sample& sample) const;            ///< владелец и номинация
    void remove(int id) const;
    void setBasket(int sampleId, std::optional<int> basketId) const;
    QVector<Sample> available(int nominationId) const;  ///< не распределённые образцы номинации
    QVector<Sample> inBasket(int basketId) const;
    bool hasEvaluations(int sampleId) const;
    int count() const;
    int unassignedCount() const;                        ///< образцы без корзины

private:
    Database& m_db;
};

}  // namespace sedm
