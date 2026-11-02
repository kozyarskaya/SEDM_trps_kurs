/**
 * @file DraftStore.h
 * @brief Локальная контрольная точка оценок дегустатора (на случай сбоя сети или ПК).
 */
#pragma once

#include "Evaluation.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace sedm {

/// Хранит черновик оценок корзины в файле JSON на компьютере дегустатора.
/// Дублирует черновик в базе данных (таблица score_draft): если связь с
/// сервером пропала, несохранённые оценки не теряются (п. 4.4.4 ТЗ).
/// Запись атомарная (QSaveFile): при сбое питания остаётся прежняя версия файла.
class DraftStore {
public:
    /// dir — каталог черновиков; serverKey — «сервер:порт/база» (черновики разных баз не смешиваются).
    DraftStore(const QString& dir, const QString& serverKey);

    /// Записывает черновик; false — файл записать не удалось.
    bool save(int tasterId, int basketId, const QVector<Evaluation>& evaluations) const;
    /// Читает черновик (пусто — черновика нет); savedAt — время записи.
    QVector<Evaluation> load(int tasterId, int basketId, QDateTime* savedAt = nullptr) const;
    void remove(int tasterId, int basketId) const;

private:
    QString path(int tasterId, int basketId) const;

    QString m_dir;
    QString m_serverKey;
};

}  // namespace sedm
