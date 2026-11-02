/**
 * @file DemoSeeder.cpp
 * @brief Реализация заполнения демонстрационными данными.
 */
#include "DemoSeeder.h"
#include "AuditService.h"
#include "AuthService.h"
#include "BasketService.h"
#include "Database.h"
#include "EvaluationRepository.h"
#include "RegistryService.h"
#include "SampleService.h"
#include "SessionService.h"
#include "TasterRepository.h"

#include <QHash>
#include <QRandomGenerator>
#include <QUuid>

namespace sedm {

namespace {
const QStringList kNominations = {
    QStringLiteral("Липовый"), QStringLiteral("Гречишный"), QStringLiteral("Цветочный"),
    QStringLiteral("Подсолнечниковый"), QStringLiteral("Акациевый"), QStringLiteral("Донниковый")};
const QStringList kRegions = {
    QStringLiteral("Тульская область"), QStringLiteral("Рязанская область"), QStringLiteral("Алтайский край"),
    QStringLiteral("Республика Башкортостан"), QStringLiteral("Краснодарский край"),
    QStringLiteral("Воронежская область"), QStringLiteral("Пензенская область"), QStringLiteral("Пермский край"),
    QStringLiteral("Московская область"), QStringLiteral("Калужская область")};
// Мужские фамилии; женские образуются окончанием «-а» (Иванов — Иванова)
const QStringList kSurnames = {
    QStringLiteral("Абрамов"), QStringLiteral("Беляев"), QStringLiteral("Власов"), QStringLiteral("Гордеев"),
    QStringLiteral("Дьяков"), QStringLiteral("Ермолов"), QStringLiteral("Жуков"), QStringLiteral("Зайцев"),
    QStringLiteral("Ильин"), QStringLiteral("Карпов"), QStringLiteral("Лаптев"), QStringLiteral("Медведев"),
    QStringLiteral("Никитин"), QStringLiteral("Орлов"), QStringLiteral("Пчелин"), QStringLiteral("Рябинин"),
    QStringLiteral("Соколов"), QStringLiteral("Тихонов"), QStringLiteral("Уваров"), QStringLiteral("Фролов"),
    QStringLiteral("Широков"), QStringLiteral("Щукин"), QStringLiteral("Яковлев"), QStringLiteral("Медовый")};
const QStringList kMaleNames = {QStringLiteral("Иван"), QStringLiteral("Пётр"), QStringLiteral("Сергей"),
                                QStringLiteral("Николай"), QStringLiteral("Алексей"), QStringLiteral("Михаил"),
                                QStringLiteral("Павел"), QStringLiteral("Владимир")};
const QStringList kFemaleNames = {QStringLiteral("Анна"), QStringLiteral("Мария"), QStringLiteral("Ольга"),
                                  QStringLiteral("Татьяна"), QStringLiteral("Елена"), QStringLiteral("Наталья"),
                                  QStringLiteral("Людмила"), QStringLiteral("Вера")};
const QStringList kPatronymicsM = {QStringLiteral("Иванович"), QStringLiteral("Петрович"), QStringLiteral("Сергеевич"),
                                   QStringLiteral("Николаевич"), QStringLiteral("Викторович")};
const QStringList kPatronymicsF = {QStringLiteral("Ивановна"), QStringLiteral("Петровна"), QStringLiteral("Сергеевна"),
                                   QStringLiteral("Николаевна"), QStringLiteral("Викторовна")};
const QStringList kTeamNames = {QStringLiteral("Эксперты"), QStringLiteral("Пасечники"), QStringLiteral("Медовары"),
                                QStringLiteral("Соты"), QString(), QStringLiteral("Нектар")};

QString pick(const QStringList& list, QRandomGenerator& rng)
{
    return list[rng.bounded(list.size())];
}

QString randomName(QRandomGenerator& rng)
{
    const bool female = rng.bounded(2) == 1;
    QString surname = pick(kSurnames, rng);
    if (female)
        surname += surname.endsWith(QStringLiteral("ый")) ? QString() : QStringLiteral("а");
    if (female && surname.endsWith(QStringLiteral("ый")))
        surname = surname.left(surname.size() - 2) + QStringLiteral("ая");
    return QStringLiteral("%1 %2 %3").arg(surname, pick(female ? kFemaleNames : kMaleNames, rng),
                                          pick(female ? kPatronymicsF : kPatronymicsM, rng));
}
}  // namespace

DemoResult DemoSeeder::seed(Database& db, AuditService& audit, const DemoOptions& options)
{
    QRandomGenerator rng(options.seed);
    DemoResult result;
    audit.setActor(QStringLiteral("администратор"), QStringLiteral("администратор"));

    AuthService auth(db, audit);
    result.adminPassword = QStringLiteral("Мёд2026");
    if (!auth.adminExists())
        auth.createAdmin(result.adminPassword);

    // Номинации и участники
    RegistryService registry(db, audit);
    QHash<QString, int> nominationIds;
    for (const Nomination& n : registry.nominations())
        nominationIds.insert(n.name(), n.id());
    for (const QString& name : kNominations)
        if (!nominationIds.contains(name))
            nominationIds.insert(name, registry.addNomination(name).id());
    QVector<int> participantIds;
    for (int i = 0; i < options.participants; ++i) {
        Participant p;
        p.setFullName(randomName(rng));
        p.setPhone(QStringLiteral("+7 9%1 %2-%3-%4").arg(rng.bounded(10, 99)).arg(rng.bounded(100, 999))
                       .arg(rng.bounded(10, 99)).arg(rng.bounded(10, 99)));
        if (rng.bounded(3) > 0)
            p.setEmail(QStringLiteral("uchastnik%1@pchela.ru").arg(i + 1));
        p.setRegion(pick(kRegions, rng));
        p.setNominationId(nominationIds.value(kNominations[i % kNominations.size()]));
        participantIds << registry.addParticipant(p).id();
    }

    // Образцы: по одному на участника, номер и шифр — автоматически
    SampleService samples(db, audit);
    const QVector<Sample> created = samples.registerForParticipants(participantIds);
    result.samples = created.size();

    // Команды и дегустаторы
    TeamService teams(db, audit);
    QVector<Team> teamList;
    for (int t = 0; t < options.teams; ++t) {
        QStringList names;
        for (int k = 0; k < options.tastersPerTeam; ++k)
            names << randomName(rng);
        Team team;
        result.credentials += teams.createTeam(teams.suggestedNumber(), kTeamNames[t % kTeamNames.size()], names, &team);
        teamList << team;
    }

    // Корзины по номинациям; назначение командам по кругу
    BasketService baskets(db, audit);
    int teamIndex = 0;
    for (const QString& name : kNominations) {
        const int nominationId = nominationIds.value(name);
        while (!baskets.availableSamples(nominationId).isEmpty()) {
            const int size = qMin(options.basketSize, baskets.availableSamples(nominationId).size());
            const Basket b = baskets.createBasket(baskets.suggestedNumber(), nominationId, size);
            baskets.fillFromPool(b.id());
            if (!teamList.isEmpty())
                baskets.assignTeam(b.id(), teamList[teamIndex++ % teamList.size()].id());
            ++result.baskets;
        }
    }

    // Сеанс дегустации: завтра в 10:00
    SessionService session(db, audit);
    TastingSession s = session.session();
    s.setTitle(QStringLiteral("Дегустация мёда — 2026"));
    s.setStartsAt(QDateTime(QDate::currentDate().addDays(1), QTime(10, 0)));
    session.saveSession(s);

    // Оценки всех образцов (испытание производительности): напрямую в хранилище
    if (options.evaluate) {
        EvaluationRepository evaluations(db);
        TasterRepository tasters(db);
        for (const Team& team : teamList)
            for (const Basket& b : baskets.baskets()) {
                if (b.teamId() != team.id())
                    continue;
                for (const Taster& t : tasters.forTeam(team.id())) {
                    QVector<Evaluation> rows;
                    for (int sampleId : b.sampleIds()) {
                        Evaluation e(t.id(), sampleId);
                        for (Criterion c : kAllCriteria)
                            e.setScore(c, 50 + static_cast<int>(rng.bounded(51)));
                        rows << e;
                    }
                    Transaction tx(db);
                    evaluations.save(rows);
                    tx.commit();
                    result.evaluations += rows.size();
                }
            }
    }
    return result;
}

}  // namespace sedm
