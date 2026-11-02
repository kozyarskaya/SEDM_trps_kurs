/**
 * @file SampleService.cpp
 * @brief Реализация регистрации образцов.
 */
#include "SampleService.h"
#include "AuditService.h"
#include "CipherGenerator.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"
#include "NominationRepository.h"
#include "ParticipantRepository.h"
#include "SampleRepository.h"
#include "BasketRepository.h"

namespace sedm {

namespace {
constexpr int kCipherAttempts = 5;   ///< попыток при совпадении шифра с чужой регистрацией
}

SampleService::SampleService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

QVector<Sample> SampleService::samples() const
{
    return SampleRepository(m_db).list();
}

Sample SampleService::insertNew(int participantId, int nominationId, CipherGenerator& generator)
{
    Sample s;
    s.setParticipantId(participantId);
    s.setNominationId(nominationId);
    s.setCipher(generator.next());
    SampleRepository(m_db).insert(s);
    return s;
}

Sample SampleService::registerSample(int participantId, int nominationId)
{
    const auto owner = ParticipantRepository(m_db).byId(participantId);
    if (!owner)
        throw DomainError(QStringLiteral("Выберите участника — владельца образца."));
    const auto nomination = NominationRepository(m_db).byId(nominationId);
    if (!nomination)
        throw DomainError(QStringLiteral("Выберите номинацию образца."));

    SampleRepository repo(m_db);
    CipherGenerator generator(repo.usedCiphers());
    for (int attempt = 1;; ++attempt) {
        try {
            const Sample s = insertNew(participantId, nominationId, generator);
            m_audit.log(QStringLiteral("Регистрация образца"),
                        QStringLiteral("№ %1, шифр %2, участник %3, номинация «%4»")
                            .arg(s.seqNumber()).arg(s.cipher(), owner->fullName(), nomination->name()));
            return s;
        } catch (const DbError& e) {
            // Шифр мог одновременно выдать другой экземпляр программы — берём следующий
            if (e.sqlState() != QLatin1String("23505") || attempt >= kCipherAttempts)
                throw;
            generator = CipherGenerator(repo.usedCiphers());
        }
    }
}

QVector<Sample> SampleService::registerForParticipants(const QVector<int>& participantIds)
{
    ParticipantRepository participants(m_db);
    SampleRepository repo(m_db);
    CipherGenerator generator(repo.usedCiphers());
    if (generator.remaining() < participantIds.size())
        throw DomainError(QStringLiteral("Свободных шифров осталось %1 — зарегистрировать %2 образцов нельзя.")
                              .arg(generator.remaining()).arg(participantIds.size()));
    QVector<Sample> created;
    Transaction tx(m_db);
    for (int id : participantIds) {
        const auto owner = participants.byId(id);
        if (!owner)
            continue;
        created.append(insertNew(owner->id(), owner->nominationId(), generator));
    }
    tx.commit();
    if (!created.isEmpty())
        m_audit.log(QStringLiteral("Регистрация образцов"),
                    QStringLiteral("Образцов: %1 (№ %2–%3)").arg(created.size())
                        .arg(created.first().seqNumber()).arg(created.last().seqNumber()));
    return created;
}

void SampleService::changeSample(int sampleId, int participantId, int nominationId)
{
    SampleRepository repo(m_db);
    const auto current = repo.byId(sampleId);
    if (!current)
        throw DomainError(QStringLiteral("Образец не найден — возможно, его удалили."));
    if (!ParticipantRepository(m_db).byId(participantId))
        throw DomainError(QStringLiteral("Выберите участника — владельца образца."));
    if (!NominationRepository(m_db).byId(nominationId))
        throw DomainError(QStringLiteral("Выберите номинацию образца."));
    if (current->isInBasket() && current->nominationId() != nominationId) {
        const auto basket = BasketRepository(m_db).byId(*current->basketId());
        throw DomainError(QStringLiteral("Образец %1 находится в корзине № %2. Чтобы сменить номинацию, "
                                         "сначала уберите его из корзины.")
                              .arg(current->cipher()).arg(basket ? basket->number() : 0));
    }
    Sample s = *current;
    s.setParticipantId(participantId);
    s.setNominationId(nominationId);
    repo.update(s);
    m_audit.log(QStringLiteral("Изменение образца"), QStringLiteral("Шифр %1").arg(s.cipher()));
}

void SampleService::removeSample(int sampleId)
{
    SampleRepository repo(m_db);
    const auto current = repo.byId(sampleId);
    if (!current)
        return;
    if (repo.hasEvaluations(sampleId))
        throw DomainError(QStringLiteral("Образец %1 уже оценивался дегустаторами — удалить его нельзя.")
                              .arg(current->cipher()));
    repo.remove(sampleId);
    m_audit.log(QStringLiteral("Удаление образца"),
                QStringLiteral("№ %1, шифр %2").arg(current->seqNumber()).arg(current->cipher()));
}

int SampleService::count() const
{
    return SampleRepository(m_db).count();
}

int SampleService::unassignedCount() const
{
    return SampleRepository(m_db).unassignedCount();
}

}  // namespace sedm
