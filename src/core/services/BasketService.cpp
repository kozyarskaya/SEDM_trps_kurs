/**
 * @file BasketService.cpp
 * @brief Реализация формирования корзин.
 */
#include "BasketService.h"
#include "AuditService.h"
#include "BasketRepository.h"
#include "Database.h"
#include "DomainError.h"
#include "NominationRepository.h"
#include "SampleRepository.h"
#include "TeamRepository.h"

namespace sedm {

BasketService::BasketService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

QVector<Basket> BasketService::baskets() const
{
    return BasketRepository(m_db).list();
}

std::optional<Basket> BasketService::basket(int basketId) const
{
    return BasketRepository(m_db).byId(basketId);
}

Basket BasketService::load(int basketId) const
{
    const auto b = BasketRepository(m_db).byId(basketId);
    if (!b)
        throw DomainError(QStringLiteral("Корзина не найдена — возможно, её удалили."));
    return *b;
}

int BasketService::suggestedNumber() const
{
    return BasketRepository(m_db).nextNumber();
}

Basket BasketService::createBasket(int number, int nominationId, int desiredCount)
{
    if (!NominationRepository(m_db).byId(nominationId))
        throw DomainError(QStringLiteral("Выберите номинацию корзины."));
    Basket b;
    b.setNumber(number);
    b.setNominationId(nominationId);
    b.setDesiredCount(desiredCount);
    BasketRepository repo(m_db);
    for (const Basket& other : repo.list())
        if (other.number() == number)
            throw DomainError(QStringLiteral("Корзина № %1 уже есть.").arg(number));
    repo.insert(b);
    m_audit.log(QStringLiteral("Создание корзины"),
                QStringLiteral("№ %1, образцов %2").arg(b.number()).arg(b.desiredCount()));
    return b;
}

void BasketService::updateBasket(int basketId, int number, int nominationId, int desiredCount)
{
    Basket b = load(basketId);
    BasketRepository repo(m_db);
    if (number != b.number())
        for (const Basket& other : repo.list())
            if (other.number() == number)
                throw DomainError(QStringLiteral("Корзина № %1 уже есть.").arg(number));
    if (nominationId != b.nominationId() && !NominationRepository(m_db).byId(nominationId))
        throw DomainError(QStringLiteral("Выберите номинацию корзины."));
    b.setNumber(number);
    b.setNominationId(nominationId);     // DomainError, если корзина не пуста
    b.setDesiredCount(desiredCount);     // DomainError, если меньше числа образцов
    repo.update(b);
    m_audit.log(QStringLiteral("Изменение корзины"),
                QStringLiteral("№ %1, образцов %2").arg(b.number()).arg(b.desiredCount()));
}

void BasketService::removeBasket(int basketId)
{
    BasketRepository repo(m_db);
    const auto b = repo.byId(basketId);
    if (!b)
        return;
    if (repo.hasEvaluations(basketId))
        throw DomainError(QStringLiteral("Образцы корзины № %1 уже оцениваются — удалить корзину нельзя.")
                              .arg(b->number()));
    repo.remove(basketId);   // образцы возвращаются в пул (ON DELETE SET NULL)
    m_audit.log(QStringLiteral("Удаление корзины"), QStringLiteral("№ %1").arg(b->number()));
}

QVector<Sample> BasketService::availableSamples(int nominationId) const
{
    return SampleRepository(m_db).available(nominationId);
}

QVector<Sample> BasketService::basketSamples(int basketId) const
{
    return SampleRepository(m_db).inBasket(basketId);
}

void BasketService::addSample(int basketId, int sampleId)
{
    Basket b = load(basketId);
    SampleRepository samples(m_db);
    const auto s = samples.byId(sampleId);
    if (!s)
        throw DomainError(QStringLiteral("Образец не найден — возможно, его удалили."));
    b.addSample(*s);   // правила корзины (DomainError с объяснением)
    samples.setBasket(sampleId, basketId);
    m_audit.log(QStringLiteral("Добавление образца в корзину"),
                QStringLiteral("Корзина № %1, шифр %2").arg(b.number()).arg(s->cipher()));
}

void BasketService::removeSample(int basketId, int sampleId)
{
    const Basket b = load(basketId);
    SampleRepository samples(m_db);
    const auto s = samples.byId(sampleId);
    if (!s || s->basketId() != basketId)
        return;
    if (samples.hasEvaluations(sampleId))
        throw DomainError(QStringLiteral("Образец %1 уже оценивался — убрать его из корзины нельзя.")
                              .arg(s->cipher()));
    samples.setBasket(sampleId, std::nullopt);
    m_audit.log(QStringLiteral("Удаление образца из корзины"),
                QStringLiteral("Корзина № %1, шифр %2").arg(b.number()).arg(s->cipher()));
}

int BasketService::fillFromPool(int basketId)
{
    Basket b = load(basketId);
    SampleRepository samples(m_db);
    const QVector<Sample> pool = samples.available(b.nominationId());
    int added = 0;
    Transaction tx(m_db);
    for (const Sample& s : pool) {
        if (b.isFull())
            break;
        b.addSample(s);
        samples.setBasket(s.id(), basketId);
        ++added;
    }
    tx.commit();
    if (added > 0)
        m_audit.log(QStringLiteral("Заполнение корзины из пула"),
                    QStringLiteral("Корзина № %1, добавлено образцов %2").arg(b.number()).arg(added));
    return added;
}

void BasketService::assignTeam(int basketId, std::optional<int> teamId)
{
    const Basket b = load(basketId);
    if (b.teamId() == teamId)
        return;
    QString teamTitle = QStringLiteral("—");
    if (teamId) {
        const auto team = TeamRepository(m_db).byId(*teamId);
        if (!team)
            throw DomainError(QStringLiteral("Команда не найдена — возможно, её удалили."));
        teamTitle = team->displayName();
    }
    BasketRepository repo(m_db);
    if (b.teamId() && repo.hasEvaluations(basketId))
        throw DomainError(QStringLiteral("Корзина № %1 уже оценивается командой — передать её другой "
                                         "команде нельзя.").arg(b.number()));
    repo.assignTeam(basketId, teamId);
    m_audit.log(QStringLiteral("Назначение корзины команде"),
                QStringLiteral("Корзина № %1 → %2").arg(b.number()).arg(teamTitle));
}

}  // namespace sedm
