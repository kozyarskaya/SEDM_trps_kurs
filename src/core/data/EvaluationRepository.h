/**
 * @file EvaluationRepository.h
 * @brief Хранение оценок, черновиков (контрольных точек) и завершения корзин.
 */
#pragma once

#include "Evaluation.h"

#include <QSet>
#include <QVector>

namespace sedm {

class Database;

/// Доступ к оценкам дегустаторов (таблица evaluation), черновикам оценок
/// (score_draft, п. 4.4.4 ТЗ) и отметкам о завершении корзин (basket_progress).
class EvaluationRepository {
public:
    explicit EvaluationRepository(Database& db) : m_db(db) {}

    QVector<Evaluation> forTaster(int tasterId) const;
    QVector<Evaluation> forTasterBasket(int tasterId, int basketId) const;
    /// Сохраняет оценки (вставка или замена по паре «дегустатор, образец»).
    void save(const QVector<Evaluation>& evaluations) const;

    QVector<Evaluation> drafts(int tasterId, int basketId) const;
    void saveDrafts(const QVector<Evaluation>& drafts) const;
    void deleteDrafts(int tasterId, int basketId) const;

    QSet<int> finishedBaskets(int tasterId) const;
    void markFinished(int tasterId, int basketId) const;
    int count() const;   ///< всего сохранённых оценок

private:
    QVector<Evaluation> load(const QString& table, int tasterId, int basketId) const;
    void store(const QString& table, const QVector<Evaluation>& rows) const;

    Database& m_db;
};

}  // namespace sedm
