/**
 * @file TastingService.h
 * @brief Оценивание образцов дегустатором: корзины, сохранение, черновики, завершение.
 */
#pragma once

#include "DraftStore.h"
#include "Evaluation.h"
#include "ReportRepository.h"
#include "Sample.h"
#include "Taster.h"
#include "TastingSession.h"
#include "Team.h"

#include <QVector>
#include <optional>

namespace sedm {

class Database;
class AuditService;

/// Лист оценивания корзины для дегустатора. Дегустатор видит только шифры образцов.
struct BasketSheet {
    TasterBasketRow info;         ///< корзина, номинация, оценено, завершена ли
    QVector<Sample> samples;      ///< образцы корзины по возрастанию шифра
    QVector<Evaluation> saved;    ///< сохранённые оценки дегустатора
    QVector<Evaluation> draft;    ///< черновик, отличающийся от сохранённого (пусто — нет)
    QDateTime draftSavedAt;       ///< время контрольной точки черновика
    bool readOnly() const { return info.finished; }
    /// Сохранённая оценка образца (пустая, если её ещё нет).
    Evaluation savedFor(int sampleId) const;
};

/// Сервис оценивания (п. 4.3.1.2 ТЗ). Все изменения проверяются по правилам
/// дегустации (образец — из корзины команды, завершённую корзину не изменить),
/// а база данных повторяет эти проверки триггерами.
class TastingService {
public:
    /// draftDir — каталог локальных черновиков; sessionToken — токен входа.
    TastingService(Database& db, AuditService& audit, const Taster& taster,
                   const QString& sessionToken, const QString& draftDir);

    const Taster& taster() const { return m_taster; }
    std::optional<Team> team() const;
    TastingSession session() const;

    QVector<TasterBasketRow> baskets() const;
    /// Все назначенные корзины завершены (и есть хотя бы одна).
    bool allFinished() const;
    /// Можно ли работать (с учётом разрешения повторного входа после завершения).
    bool canEnter() const;

    BasketSheet openBasket(int basketId) const;
    /// Кнопка «Сохранить»: оценки — в базу данных, черновики корзины удаляются.
    void save(int basketId, const QVector<Evaluation>& evaluations);
    /// Сохраняет и завершает дегустацию корзины (все образцы оценены по четырём критериям).
    void finish(int basketId, const QVector<Evaluation>& evaluations);
    /// Контрольная точка несохранённых оценок: в локальный файл и в базу данных.
    /// false — сервер недоступен, черновик сохранён только на этом компьютере.
    bool saveCheckpoint(int basketId, const QVector<Evaluation>& evaluations);
    /// Отказ от восстановления черновика.
    void discardDraft(int basketId);

    /// Не выполнен ли вход под этой учётной записью на другом компьютере.
    bool isSessionCurrent() const;
    const QString& sessionToken() const { return m_token; }

private:
    Basket teamBasket(int basketId) const;
    void ensureWritable(const Basket& basket) const;
    QVector<Evaluation> forBasket(const Basket& basket, const QVector<Evaluation>& evaluations) const;
    static QString ciphers(const QVector<Sample>& samples, const QVector<Evaluation>& evaluations);

    Database& m_db;
    AuditService& m_audit;
    Taster m_taster;
    QString m_token;
    DraftStore m_drafts;
};

}  // namespace sedm
