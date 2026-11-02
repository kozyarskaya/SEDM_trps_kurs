/**
 * @file RegistryService.h
 * @brief Справочник номинаций и реестр участников, загрузка реестра из файлов.
 */
#pragma once

#include "Nomination.h"
#include "Participant.h"
#include "ParticipantImporter.h"

#include <QStringList>
#include <QVector>

namespace sedm {

class Database;
class AuditService;

/// Итог загрузки реестра.
struct ImportResult {
    int added = 0;                  ///< добавлено участников
    int skippedDuplicates = 0;      ///< пропущено повторов (уже есть в реестре)
    int skippedErrors = 0;          ///< пропущено строк с ошибками
    QStringList createdNominations; ///< номинации, добавленные в справочник
};

/// Сервис реестра (п. 4.3.1.1 ТЗ): номинации, участники, импорт CSV и XLSX.
/// Все проверки выполняются до обращения к базе данных; сообщения — на русском.
class RegistryService {
public:
    RegistryService(Database& db, AuditService& audit);

    // --- номинации ---
    QVector<Nomination> nominations() const;
    Nomination addNomination(const QString& name);
    void renameNomination(int id, const QString& name);
    /// Удаление: только если номинация не используется участниками, образцами, корзинами.
    void removeNomination(int id);

    // --- участники ---
    QVector<Participant> participants() const;
    std::optional<Participant> participant(int id) const;
    Participant addParticipant(const Participant& participant);
    void updateParticipant(const Participant& participant);
    /// Удаление: только если у участника нет зарегистрированных образцов.
    void removeParticipant(int id);
    int participantCount() const;

    // --- загрузка реестра ---
    /// Читает файл и проверяет строки; отмечает участников, уже внесённых в реестр.
    ImportPreview previewImport(const QString& path) const;
    /// Загружает проверенные строки в одной транзакции (всё или ничего).
    /// skipDuplicates — не добавлять повторно участников, уже внесённых в реестр.
    ImportResult applyImport(const ImportPreview& preview, bool skipDuplicates = true);

private:
    static QString duplicateKey(const QString& fullName, const QString& phone);
    void checkParticipant(const Participant& participant) const;

    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
