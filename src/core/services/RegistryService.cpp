/**
 * @file RegistryService.cpp
 * @brief Реализация сервиса реестра.
 */
#include "RegistryService.h"
#include "AuditService.h"
#include "Database.h"
#include "DomainError.h"
#include "NominationRepository.h"
#include "ParticipantRepository.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

namespace sedm {

RegistryService::RegistryService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

// ------------------------------------------------------------------ номинации
QVector<Nomination> RegistryService::nominations() const
{
    return NominationRepository(m_db).list();
}

Nomination RegistryService::addNomination(const QString& name)
{
    Nomination n;
    n.setName(name);   // проверка названия (DomainError)
    NominationRepository repo(m_db);
    if (repo.byName(n.name()))
        throw DomainError(QStringLiteral("Номинация «%1» уже есть в справочнике.").arg(n.name()));
    repo.insert(n);
    m_audit.log(QStringLiteral("Добавление номинации"), n.name());
    return n;
}

void RegistryService::renameNomination(int id, const QString& name)
{
    NominationRepository repo(m_db);
    const auto current = repo.byId(id);
    if (!current)
        throw DomainError(QStringLiteral("Номинация не найдена — возможно, её удалили."));
    Nomination n = *current;
    n.setName(name);
    const auto same = repo.byName(n.name());
    if (same && same->id() != id)
        throw DomainError(QStringLiteral("Номинация «%1» уже есть в справочнике.").arg(n.name()));
    repo.update(n);
    m_audit.log(QStringLiteral("Переименование номинации"),
                QStringLiteral("«%1» → «%2»").arg(current->name(), n.name()));
}

void RegistryService::removeNomination(int id)
{
    NominationRepository repo(m_db);
    const auto current = repo.byId(id);
    if (!current)
        return;
    const int used = repo.usageCount(id);
    if (used > 0)
        throw DomainError(QStringLiteral("Номинация «%1» используется (участники, образцы или корзины: %2) — "
                                         "удалить её нельзя.").arg(current->name()).arg(used));
    repo.remove(id);
    m_audit.log(QStringLiteral("Удаление номинации"), current->name());
}

// ------------------------------------------------------------------ участники
QVector<Participant> RegistryService::participants() const
{
    return ParticipantRepository(m_db).list();
}

std::optional<Participant> RegistryService::participant(int id) const
{
    return ParticipantRepository(m_db).byId(id);
}

void RegistryService::checkParticipant(const Participant& participant) const
{
    const QStringList problems = participant.validate();
    if (!problems.isEmpty())
        throw DomainError(problems.join(QLatin1Char('\n')));
    if (!NominationRepository(m_db).byId(participant.nominationId()))
        throw DomainError(QStringLiteral("Выбранная номинация не найдена в справочнике."));
}

Participant RegistryService::addParticipant(const Participant& participant)
{
    checkParticipant(participant);
    Participant p = participant;
    ParticipantRepository(m_db).insert(p);
    m_audit.log(QStringLiteral("Добавление участника"), p.fullName());
    return p;
}

void RegistryService::updateParticipant(const Participant& participant)
{
    checkParticipant(participant);
    ParticipantRepository repo(m_db);
    if (!repo.byId(participant.id()))
        throw DomainError(QStringLiteral("Участник не найден — возможно, его удалили."));
    repo.update(participant);
    m_audit.log(QStringLiteral("Изменение данных участника"), participant.fullName());
}

void RegistryService::removeParticipant(int id)
{
    ParticipantRepository repo(m_db);
    const auto p = repo.byId(id);
    if (!p)
        return;
    const int samples = repo.sampleCount(id);
    if (samples > 0)
        throw DomainError(QStringLiteral("У участника «%1» зарегистрировано образцов: %2. "
                                         "Сначала удалите его образцы.").arg(p->fullName()).arg(samples));
    repo.remove(id);
    m_audit.log(QStringLiteral("Удаление участника"), p->fullName());
}

int RegistryService::participantCount() const
{
    return ParticipantRepository(m_db).count();
}

// ------------------------------------------------------------ загрузка реестра
QString RegistryService::duplicateKey(const QString& fullName, const QString& phone)
{
    // Телефон — только цифры в едином виде: «8 912 …», «+7 (912) …» и «912 …» совпадают
    static const QRegularExpression nonDigits(QStringLiteral("[^0-9]"));
    QString digits = QString(phone).remove(nonDigits);
    if (digits.size() == 11 && digits.startsWith(QLatin1Char('8')))
        digits[0] = QLatin1Char('7');
    else if (digits.size() == 10)
        digits.prepend(QLatin1Char('7'));
    return fullName.simplified().toLower() + QLatin1Char('|') + digits;
}

ImportPreview RegistryService::previewImport(const QString& path) const
{
    const auto source = ParticipantSource::forFile(path);
    ImportPreview preview = ParticipantImporter::buildPreview(source->readTable(path), QFileInfo(path).fileName());

    // Повторы с реестром ищутся в памяти — один запрос вместо запроса на строку
    QHash<QString, int> existing;
    for (const Participant& p : ParticipantRepository(m_db).list())
        existing.insert(duplicateKey(p.fullName(), p.phone()), p.id());
    QHash<QString, bool> known;
    for (const Nomination& n : NominationRepository(m_db).list())
        known.insert(n.name().toLower(), true);

    for (ImportRow& row : preview.rows) {
        const auto it = existing.constFind(duplicateKey(row.participant.fullName(), row.participant.phone()));
        if (it != existing.constEnd())
            row.duplicateOf = it.value();
        row.newNomination = !row.nominationName.isEmpty()
                            && !known.contains(Nomination::normalizedName(row.nominationName).toLower());
    }
    return preview;
}

ImportResult RegistryService::applyImport(const ImportPreview& preview, bool skipDuplicates)
{
    ImportResult result;
    NominationRepository nominations(m_db);
    ParticipantRepository participants(m_db);

    QHash<QString, int> nominationIds;   // название в нижнем регистре -> id
    for (const Nomination& n : nominations.list())
        nominationIds.insert(n.name().toLower(), n.id());

    Transaction tx(m_db);
    for (const ImportRow& row : preview.rows) {
        if (!row.isValid()) {
            ++result.skippedErrors;
            continue;
        }
        if (row.duplicateOf && skipDuplicates) {
            ++result.skippedDuplicates;
            continue;
        }
        const QString name = Nomination::normalizedName(row.nominationName);
        int nominationId = nominationIds.value(name.toLower(), 0);
        if (nominationId == 0) {
            Nomination n;
            n.setName(name);
            nominations.insert(n);
            nominationId = n.id();
            nominationIds.insert(name.toLower(), nominationId);
            result.createdNominations << n.name();
        }
        Participant p = row.participant;
        p.setNominationId(nominationId);
        participants.insert(p);
        ++result.added;
    }
    tx.commit();

    QString details = QStringLiteral("Файл «%1»: добавлено %2, пропущено повторов %3, строк с ошибками %4")
                          .arg(preview.fileName).arg(result.added).arg(result.skippedDuplicates)
                          .arg(result.skippedErrors);
    if (!result.createdNominations.isEmpty())
        details += QStringLiteral("; новые номинации: ") + result.createdNominations.join(QStringLiteral(", "));
    m_audit.log(QStringLiteral("Загрузка реестра участников"), details);
    return result;
}

}  // namespace sedm
