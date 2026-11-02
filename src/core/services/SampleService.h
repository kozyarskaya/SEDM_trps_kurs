/**
 * @file SampleService.h
 * @brief Регистрация образцов мёда: сквозной порядковый номер и уникальный шифр.
 */
#pragma once

#include "Sample.h"

#include <QVector>

namespace sedm {

class Database;
class AuditService;
class CipherGenerator;

/// Сервис образцов (п. 4.3.1.1 ТЗ). Порядковый номер выдаёт последовательность
/// базы данных, шифр — CipherGenerator: случайное четырёхзначное число, не
/// повторяющееся ни у одного образца (решение заказчика В11).
class SampleService {
public:
    SampleService(Database& db, AuditService& audit);

    QVector<Sample> samples() const;
    /// Регистрирует образец участника в номинации; возвращает его с номером и шифром.
    Sample registerSample(int participantId, int nominationId);
    /// Регистрирует по одному образцу для каждого участника в его заявленной
    /// номинации — в одной транзакции (удобно после загрузки реестра).
    QVector<Sample> registerForParticipants(const QVector<int>& participantIds);
    /// Смена владельца или номинации; номинация меняется только вне корзины.
    void changeSample(int sampleId, int participantId, int nominationId);
    /// Удаление: только для образца, который ещё не оценивался.
    void removeSample(int sampleId);
    int count() const;
    int unassignedCount() const;   ///< образцов вне корзин

private:
    Sample insertNew(int participantId, int nominationId, CipherGenerator& generator);

    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
