/**
 * @file TeamService.h
 * @brief Дегустационные команды и учётные записи дегустаторов.
 */
#pragma once

#include "Taster.h"
#include "Team.h"

#include <QSet>
#include <QStringList>
#include <QVector>
#include <optional>

namespace sedm {

class Database;
class AuditService;

/// Данные для карточки дегустатора. PIN-код известен только в момент выдачи:
/// в базе данных хранится его bcrypt-хэш (п. 4.4.6 ТЗ).
struct TasterCredentials {
    int tasterId = 0;
    QString fullName;
    QString login;
    QString pin;          ///< открытый PIN-код — только для печати карточки
    int teamNumber = 0;
    QString teamTitle;    ///< «Команда 2 «Эксперты»»
};

/// Сервис команд (п. 4.3.1.1 ТЗ): номер, название, исходный перечень фамилий
/// (может быть пустым). Для каждого дегустатора формируются логин — фамилия
/// кириллицей (решение В9) — и случайный четырёхзначный PIN-код.
class TeamService {
public:
    TeamService(Database& db, AuditService& audit);

    QVector<Team> teams() const;
    std::optional<Team> team(int teamId) const;
    int suggestedNumber() const;

    /// Создаёт команду и её дегустаторов; возвращает данные для карточек.
    QVector<TasterCredentials> createTeam(int number, const QString& name, const QStringList& tasterNames,
                                          Team* created = nullptr);
    void updateTeam(int teamId, int number, const QString& name);
    /// Удаление: только если дегустаторы команды не выставляли оценок.
    void removeTeam(int teamId);

    QVector<Taster> tasters(int teamId) const;
    QVector<Taster> allTasters() const;
    TasterCredentials addTaster(int teamId, const QString& fullName);
    void renameTaster(int tasterId, const QString& fullName);
    void removeTaster(int tasterId);
    /// Новый PIN-код (прежний перестаёт действовать, блокировка снимается).
    QVector<TasterCredentials> resetPins(const QVector<int>& tasterIds);

    /// Разбор перечня ФИО: по одному в строке (или через «;»), пустые строки пропускаются.
    static QStringList parseNames(const QString& text);
    /// Проверка ФИО дегустатора: пустая строка — ФИО подходит.
    static QString nameProblem(const QString& fullName);

private:
    TasterCredentials createTaster(const Team& team, const QString& fullName, QSet<QString>& logins);

    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
