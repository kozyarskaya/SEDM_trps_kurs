/**
 * @file TastingService.cpp
 * @brief Реализация оценивания образцов дегустатором.
 */
#include "TastingService.h"
#include "AuditService.h"
#include "BasketProgress.h"
#include "BasketRepository.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"
#include "EvaluationRepository.h"
#include "SampleRepository.h"
#include "SessionRepository.h"
#include "TasterRepository.h"
#include "TeamRepository.h"

#include <QHash>
#include <algorithm>

namespace sedm {

namespace {
QString serverKey(const ConnectionSettings& s)
{
    return QStringLiteral("%1:%2/%3").arg(s.host).arg(s.port).arg(s.database);
}
}  // namespace

Evaluation BasketSheet::savedFor(int sampleId) const
{
    for (const Evaluation& e : saved)
        if (e.sampleId() == sampleId)
            return e;
    return Evaluation(0, sampleId);
}

TastingService::TastingService(Database& db, AuditService& audit, const Taster& taster,
                               const QString& sessionToken, const QString& draftDir)
    : m_db(db), m_audit(audit), m_taster(taster), m_token(sessionToken),
      m_drafts(draftDir, serverKey(db.settings()))
{
}

std::optional<Team> TastingService::team() const
{
    return TeamRepository(m_db).byId(m_taster.teamId());
}

TastingSession TastingService::session() const
{
    return SessionRepository(m_db).load();
}

QVector<TasterBasketRow> TastingService::baskets() const
{
    return ReportRepository(m_db).basketsForTaster(m_taster.id());
}

bool TastingService::allFinished() const
{
    const auto rows = baskets();
    if (rows.isEmpty())
        return false;
    return std::all_of(rows.begin(), rows.end(), [](const TasterBasketRow& r) { return r.finished; });
}

bool TastingService::canEnter() const
{
    return session().canTasterEnter(allFinished());
}

bool TastingService::isSessionCurrent() const
{
    return !m_token.isEmpty() && TasterRepository(m_db).sessionToken(m_taster.id()) == m_token;
}

Basket TastingService::teamBasket(int basketId) const
{
    const auto b = BasketRepository(m_db).byId(basketId);
    if (!b || b->teamId() != m_taster.teamId())
        throw DomainError(QStringLiteral("Эта корзина больше не назначена вашей команде. "
                                         "Вернитесь к списку корзин."));
    return *b;
}

void TastingService::ensureWritable(const Basket& basket) const
{
    if (!isSessionCurrent())
        throw DomainError(QStringLiteral("Под вашей учётной записью выполнен вход на другом компьютере. "
                                         "Здесь сохранить оценки нельзя — выйдите и войдите снова."));
    if (EvaluationRepository(m_db).finishedBaskets(m_taster.id()).contains(basket.id()))
        throw DomainError(QStringLiteral("Дегустация корзины № %1 уже завершена — оценки изменить нельзя.")
                              .arg(basket.number()));
}

QVector<Evaluation> TastingService::forBasket(const Basket& basket, const QVector<Evaluation>& evaluations) const
{
    // Только образцы этой корзины и только непустые оценки — от имени текущего дегустатора
    QVector<Evaluation> result;
    for (Evaluation e : evaluations) {
        if (!basket.contains(e.sampleId()) || e.isEmpty())
            continue;
        e.setTasterId(m_taster.id());
        result.append(e);
    }
    return result;
}

QString TastingService::ciphers(const QVector<Sample>& samples, const QVector<Evaluation>& evaluations)
{
    QHash<int, QString> byId;
    for (const Sample& s : samples)
        byId.insert(s.id(), s.cipher());
    QStringList list;
    for (const Evaluation& e : evaluations)
        list << byId.value(e.sampleId());
    list.sort();
    return list.join(QStringLiteral(", "));
}

BasketSheet TastingService::openBasket(int basketId) const
{
    const Basket basket = teamBasket(basketId);
    BasketSheet sheet;
    for (const TasterBasketRow& r : baskets())
        if (r.basket.id() == basketId)
            sheet.info = r;
    sheet.samples = SampleRepository(m_db).inBasket(basketId);
    std::sort(sheet.samples.begin(), sheet.samples.end(),
              [](const Sample& a, const Sample& b) { return a.cipher() < b.cipher(); });

    EvaluationRepository repo(m_db);
    sheet.saved = repo.forTasterBasket(m_taster.id(), basketId);
    if (sheet.readOnly())
        return sheet;

    // Черновик: из базы данных или локальный — какой новее
    QVector<Evaluation> draft = repo.drafts(m_taster.id(), basketId);
    QDateTime draftAt;
    for (const Evaluation& e : draft)
        if (!draftAt.isValid() || e.savedAt() > draftAt)
            draftAt = e.savedAt();
    QDateTime localAt;
    const QVector<Evaluation> local = m_drafts.load(m_taster.id(), basketId, &localAt);
    if (!local.isEmpty() && (!draftAt.isValid() || localAt > draftAt)) {
        draft = local;
        draftAt = localAt;
    }
    // Черновик нужен, только если он отличается от сохранённых оценок
    bool differs = false;
    for (const Evaluation& d : draft)
        if (basket.contains(d.sampleId()) && !d.sameContentAs(sheet.savedFor(d.sampleId()))) {
            differs = true;
            break;
        }
    if (differs) {
        sheet.draft = forBasket(basket, draft);
        sheet.draftSavedAt = draftAt;
    }
    return sheet;
}

void TastingService::save(int basketId, const QVector<Evaluation>& evaluations)
{
    const Basket basket = teamBasket(basketId);
    ensureWritable(basket);
    const QVector<Evaluation> rows = forBasket(basket, evaluations);
    EvaluationRepository repo(m_db);
    Transaction tx(m_db);
    repo.save(rows);
    repo.deleteDrafts(m_taster.id(), basketId);
    tx.commit();
    m_drafts.remove(m_taster.id(), basketId);
    m_audit.log(QStringLiteral("Сохранение оценок"),
                QStringLiteral("Корзина № %1; образцы: %2")
                    .arg(basket.number()).arg(ciphers(SampleRepository(m_db).inBasket(basketId), rows)));
}

void TastingService::finish(int basketId, const QVector<Evaluation>& evaluations)
{
    const Basket basket = teamBasket(basketId);
    ensureWritable(basket);
    EvaluationRepository repo(m_db);
    Transaction tx(m_db);
    repo.save(forBasket(basket, evaluations));
    // Проверка по сохранённым данным: по каждому образцу — все четыре оценки
    BasketProgress progress(m_taster.id(), basketId);
    progress.finish(basket, repo.forTasterBasket(m_taster.id(), basketId), QDateTime::currentDateTime());
    repo.markFinished(m_taster.id(), basketId);
    repo.deleteDrafts(m_taster.id(), basketId);
    tx.commit();
    m_drafts.remove(m_taster.id(), basketId);
    m_audit.log(QStringLiteral("Завершение дегустации корзины"),
                QStringLiteral("Корзина № %1, образцов %2").arg(basket.number()).arg(basket.sampleCount()));
}

bool TastingService::saveCheckpoint(int basketId, const QVector<Evaluation>& evaluations)
{
    const Basket basket = teamBasket(basketId);
    const QVector<Evaluation> rows = forBasket(basket, evaluations);
    m_drafts.save(m_taster.id(), basketId, rows);   // сначала — на этот компьютер
    try {
        if (!isSessionCurrent())
            return false;
        EvaluationRepository repo(m_db);
        Transaction tx(m_db);
        repo.deleteDrafts(m_taster.id(), basketId);
        repo.saveDrafts(rows);
        tx.commit();
        return true;
    } catch (const DbError&) {
        return false;   // сервер недоступен: черновик остался в локальном файле
    }
}

void TastingService::discardDraft(int basketId)
{
    m_drafts.remove(m_taster.id(), basketId);
    try {
        EvaluationRepository(m_db).deleteDrafts(m_taster.id(), basketId);
    } catch (const DbError&) {
        // черновик в базе будет удалён при следующем сохранении
    }
}

}  // namespace sedm
