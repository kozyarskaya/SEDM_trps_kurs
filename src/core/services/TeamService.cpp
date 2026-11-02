/**
 * @file TeamService.cpp
 * @brief Реализация сервиса команд и дегустаторов.
 */
#include "TeamService.h"
#include "AuditService.h"
#include "Database.h"
#include "DomainError.h"
#include "LoginGenerator.h"
#include "PasswordHasher.h"
#include "PinGenerator.h"
#include "TasterRepository.h"
#include "TeamRepository.h"

#include <QRegularExpression>
#include <QSet>

namespace sedm {

namespace {
constexpr int kMaxFullNameLength = 150;
}

TeamService::TeamService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

QStringList TeamService::parseNames(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[\\r\\n;]+"));
    QStringList names;
    for (const QString& part : text.split(separators, Qt::SkipEmptyParts)) {
        const QString name = part.simplified();
        if (!name.isEmpty())
            names << name;
    }
    return names;
}

QString TeamService::nameProblem(const QString& fullName)
{
    const QString name = fullName.simplified();
    if (name.isEmpty())
        return QStringLiteral("Укажите фамилию дегустатора.");
    if (name.size() > kMaxFullNameLength)
        return QStringLiteral("ФИО слишком длинное (более %1 символов).").arg(kMaxFullNameLength);
    // Фамилия — русскими буквами (из неё формируется логин); далее — имя, отчество или инициалы
    static const QRegularExpression re(QStringLiteral(
        "^[А-ЯЁа-яё]{2,}(?:-[А-ЯЁа-яё]+)*(?: [А-ЯЁа-яё.\\-]+)*$"));
    if (!re.match(name).hasMatch())
        return QStringLiteral("«%1»: ФИО дегустатора пишется русскими буквами — фамилия, затем "
                              "(необязательно) имя и отчество.").arg(name);
    return QString();
}

QVector<Team> TeamService::teams() const
{
    return TeamRepository(m_db).list();
}

std::optional<Team> TeamService::team(int teamId) const
{
    return TeamRepository(m_db).byId(teamId);
}

int TeamService::suggestedNumber() const
{
    return TeamRepository(m_db).nextNumber();
}

TasterCredentials TeamService::createTaster(const Team& team, const QString& fullName, QSet<QString>& logins)
{
    Taster t;
    t.setTeamId(team.id());
    t.setFullName(fullName);
    t.setLogin(LoginGenerator::makeLogin(t.fullName(), logins));
    const QString pin = PinGenerator::generate();
    t.setSecretHash(PasswordHasher::hash(pin));
    TasterRepository(m_db).insert(t);
    logins.insert(t.login());

    TasterCredentials c;
    c.tasterId = t.id();
    c.fullName = t.fullName();
    c.login = t.login();
    c.pin = pin;
    c.teamNumber = team.number();
    c.teamTitle = team.displayName();
    return c;
}

QVector<TasterCredentials> TeamService::createTeam(int number, const QString& name,
                                                   const QStringList& tasterNames, Team* created)
{
    Team team;
    team.setNumber(number);   // DomainError при неверном номере
    team.setName(name);
    for (const QString& n : tasterNames) {
        const QString problem = nameProblem(n);
        if (!problem.isEmpty())
            throw DomainError(problem);
    }
    TeamRepository teams(m_db);
    for (const Team& other : teams.list())
        if (other.number() == number)
            throw DomainError(QStringLiteral("Команда № %1 уже есть.").arg(number));

    QVector<TasterCredentials> result;
    QSet<QString> logins = TasterRepository(m_db).existingLogins();
    Transaction tx(m_db);
    teams.insert(team);
    for (const QString& n : tasterNames)
        result.append(createTaster(team, n, logins));
    tx.commit();
    if (created)
        *created = team;
    m_audit.log(QStringLiteral("Создание команды"),
                QStringLiteral("%1, дегустаторов %2").arg(team.displayName()).arg(result.size()));
    return result;
}

void TeamService::updateTeam(int teamId, int number, const QString& name)
{
    TeamRepository repo(m_db);
    const auto current = repo.byId(teamId);
    if (!current)
        throw DomainError(QStringLiteral("Команда не найдена — возможно, её удалили."));
    if (number != current->number())
        for (const Team& other : repo.list())
            if (other.number() == number)
                throw DomainError(QStringLiteral("Команда № %1 уже есть.").arg(number));
    Team t = *current;
    t.setNumber(number);
    t.setName(name);
    repo.update(t);
    m_audit.log(QStringLiteral("Изменение команды"), t.displayName());
}

void TeamService::removeTeam(int teamId)
{
    TeamRepository repo(m_db);
    const auto current = repo.byId(teamId);
    if (!current)
        return;
    if (TasterRepository(m_db).countWithEvaluations(teamId) > 0)
        throw DomainError(QStringLiteral("Дегустаторы команды «%1» уже выставляли оценки — удалить команду "
                                         "нельзя.").arg(current->displayName()));
    repo.remove(teamId);   // дегустаторы удаляются, корзины остаются без команды
    m_audit.log(QStringLiteral("Удаление команды"), current->displayName());
}

QVector<Taster> TeamService::tasters(int teamId) const
{
    return TasterRepository(m_db).forTeam(teamId);
}

QVector<Taster> TeamService::allTasters() const
{
    return TasterRepository(m_db).list();
}

TasterCredentials TeamService::addTaster(int teamId, const QString& fullName)
{
    const QString problem = nameProblem(fullName);
    if (!problem.isEmpty())
        throw DomainError(problem);
    const auto t = TeamRepository(m_db).byId(teamId);
    if (!t)
        throw DomainError(QStringLiteral("Команда не найдена — возможно, её удалили."));
    QSet<QString> logins = TasterRepository(m_db).existingLogins();
    const TasterCredentials c = createTaster(*t, fullName, logins);
    m_audit.log(QStringLiteral("Добавление дегустатора"),
                QStringLiteral("%1 (логин %2), %3").arg(c.fullName, c.login, c.teamTitle));
    return c;
}

void TeamService::renameTaster(int tasterId, const QString& fullName)
{
    const QString problem = nameProblem(fullName);
    if (!problem.isEmpty())
        throw DomainError(problem);
    TasterRepository repo(m_db);
    const auto t = repo.byId(tasterId);
    if (!t)
        throw DomainError(QStringLiteral("Дегустатор не найден — возможно, его удалили."));
    repo.updateName(tasterId, fullName);   // логин не меняется: он уже выдан на карточке
    m_audit.log(QStringLiteral("Изменение ФИО дегустатора"),
                QStringLiteral("%1 → %2").arg(t->fullName(), fullName.simplified()));
}

void TeamService::removeTaster(int tasterId)
{
    TasterRepository repo(m_db);
    const auto t = repo.byId(tasterId);
    if (!t)
        return;
    if (repo.hasEvaluations(tasterId))
        throw DomainError(QStringLiteral("Дегустатор %1 уже выставлял оценки — удалить его нельзя.")
                              .arg(t->fullName()));
    repo.remove(tasterId);
    m_audit.log(QStringLiteral("Удаление дегустатора"), QStringLiteral("%1 (логин %2)").arg(t->fullName(), t->login()));
}

QVector<TasterCredentials> TeamService::resetPins(const QVector<int>& tasterIds)
{
    TasterRepository repo(m_db);
    TeamRepository teams(m_db);
    QVector<TasterCredentials> result;
    QStringList logins;
    Transaction tx(m_db);
    for (int id : tasterIds) {
        const auto t = repo.byId(id);
        if (!t)
            continue;
        const auto team = teams.byId(t->teamId());
        TasterCredentials c;
        c.tasterId = t->id();
        c.fullName = t->fullName();
        c.login = t->login();
        c.pin = PinGenerator::generate();
        c.teamNumber = team ? team->number() : 0;
        c.teamTitle = team ? team->displayName() : QString();
        repo.updatePin(id, PasswordHasher::hash(c.pin));
        result.append(c);
        logins << c.login;
    }
    tx.commit();
    if (!result.isEmpty())
        m_audit.log(QStringLiteral("Выдача новых PIN-кодов"), logins.join(QStringLiteral(", ")));
    return result;
}

}  // namespace sedm
