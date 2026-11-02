/**
 * @file Basket.cpp
 * @brief Реализация правил формирования корзины.
 */
#include "Basket.h"
#include "DomainError.h"
#include "Sample.h"

namespace sedm {

void Basket::setNumber(int number)
{
    if (number <= 0)
        throw DomainError(QStringLiteral("Номер корзины должен быть положительным числом."));
    m_number = number;
}

void Basket::setNominationId(int nominationId)
{
    if (nominationId != m_nominationId && !m_sampleIds.isEmpty())
        throw DomainError(QStringLiteral("Номинацию можно изменить только у пустой корзины."));
    m_nominationId = nominationId;
}

void Basket::setDesiredCount(int count)
{
    if (count < 1 || count > kMaxDesiredCount)
        throw DomainError(QStringLiteral("Желаемое количество образцов — от 1 до %1.").arg(kMaxDesiredCount));
    if (count < m_sampleIds.size())
        throw DomainError(QStringLiteral("В корзине уже %1 образцов — нельзя задать меньшее количество.")
                              .arg(m_sampleIds.size()));
    m_desiredCount = count;
}

bool Basket::canAccept(const Sample& sample, QString* reason) const
{
    auto refuse = [reason](const QString& text) {
        if (reason)
            *reason = text;
        return false;
    };
    if (contains(sample.id()))
        return refuse(QStringLiteral("Образец %1 уже находится в этой корзине.").arg(sample.cipher()));
    if (sample.isInBasket())
        return refuse(QStringLiteral("Образец %1 уже распределён в другую корзину.").arg(sample.cipher()));
    if (sample.nominationId() != m_nominationId)
        return refuse(QStringLiteral("Образец %1 относится к другой номинации.").arg(sample.cipher()));
    if (isFull())
        return refuse(QStringLiteral("Корзина заполнена: %1 из %2 образцов.")
                          .arg(m_sampleIds.size()).arg(m_desiredCount));
    return true;
}

void Basket::addSample(const Sample& sample)
{
    QString reason;
    if (!canAccept(sample, &reason))
        throw DomainError(reason);
    m_sampleIds.append(sample.id());
}

void Basket::removeSample(int sampleId)
{
    if (!m_sampleIds.removeOne(sampleId))
        throw DomainError(QStringLiteral("Образца нет в корзине № %1.").arg(m_number));
}

}  // namespace sedm
