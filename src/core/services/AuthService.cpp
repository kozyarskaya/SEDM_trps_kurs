/**
 * @file AuthService.cpp
 * @brief Реализация аутентификации.
 */
#include "AuthService.h"
#include "AuditService.h"
#include "Database.h"
#include "DomainError.h"
#include "PasswordHasher.h"
#include "PinGenerator.h"
#include "SessionRepository.h"
#include "TasterRepository.h"

#include <QUuid>

namespace sedm {

namespace {
// Хэш для «холостой» проверки неизвестного логина: время ответа не выдаёт,
// существует ли такой дегустатор (защита от перебора логинов).
const QString& dummyHash()
{
    static const QString hash = PasswordHasher::hash(QStringLiteral("sedm-dummy-pin"));
    return hash;
}
}  // namespace

AuthService::AuthService(Database& db, AuditService& audit)
    : m_db(db), m_audit(audit)
{
}

QString AuthService::passwordProblem(const QString& password)
{
    if (password.size() < kMinPasswordLength)
        return QStringLiteral("Пароль должен содержать не менее %1 символов.").arg(kMinPasswordLength);
    bool letter = false, digit = false;
    for (const QChar ch : password) {
        letter = letter || ch.isLetter();
        digit = digit || ch.isDigit();
    }
    if (!letter || !digit)
        return QStringLiteral("Пароль должен содержать хотя бы одну букву и одну цифру.");
    return QString();
}

bool AuthService::adminExists() const
{
    return SessionRepository(m_db).hasAdmin();
}

void AuthService::createAdmin(const QString& password)
{
    const QString problem = passwordProblem(password);
    if (!problem.isEmpty())
        throw DomainError(problem);
    if (adminExists())
        throw DomainError(QStringLiteral("Пароль администратора уже задан."));
    SessionRepository(m_db).setAdminPassword(PasswordHasher::hash(password));
    m_audit.setActor(QStringLiteral("администратор"), QStringLiteral("администратор"));
    m_audit.log(QStringLiteral("Задан пароль администратора"));
}

AuthService::AdminLogin AuthService::loginAdmin(const QString& password)
{
    SessionRepository repo(m_db);
    AdminLogin result;
    if (!repo.hasAdmin()) {
        result.status = LoginStatus::NoAdmin;
        return result;
    }
    AdminAccount admin = repo.admin();
    const QDateTime now = QDateTime::currentDateTime();
    if (admin.isLockedAt(now)) {
        result.status = LoginStatus::Locked;
        result.secondsLeft = admin.secondsUntilUnlock(now);
        return result;
    }
    m_audit.setActor(admin.login(), admin.roleName());
    if (PasswordHasher::verify(password, admin.secretHash())) {
        admin.registerSuccess();
        repo.saveAdminLockState(admin);
        m_audit.log(QStringLiteral("Вход администратора"));
        result.status = LoginStatus::Ok;
        return result;
    }
    admin.registerFailedAttempt(now);
    repo.saveAdminLockState(admin);
    if (admin.isLockedAt(now)) {
        m_audit.log(QStringLiteral("Блокировка входа администратора"),
                    QStringLiteral("5 неверных паролей подряд"));
        result.status = LoginStatus::Locked;
        result.secondsLeft = admin.secondsUntilUnlock(now);
    } else {
        m_audit.log(QStringLiteral("Неверный пароль администратора"));
    }
    return result;
}

void AuthService::changeAdminPassword(const QString& currentPassword, const QString& newPassword)
{
    SessionRepository repo(m_db);
    if (!PasswordHasher::verify(currentPassword, repo.admin().secretHash()))
        throw DomainError(QStringLiteral("Текущий пароль указан неверно."));
    const QString problem = passwordProblem(newPassword);
    if (!problem.isEmpty())
        throw DomainError(problem);
    repo.setAdminPassword(PasswordHasher::hash(newPassword));
    m_audit.log(QStringLiteral("Смена пароля администратора"));
}

AuthService::TasterLogin AuthService::loginTaster(const QString& login, const QString& pin)
{
    TasterRepository repo(m_db);
    TasterLogin result;
    const auto found = repo.byLogin(login);
    if (!found) {
        PasswordHasher::verify(pin, dummyHash());   // одинаковое время ответа
        m_audit.setActor(login.simplified(), QStringLiteral("дегустатор"));
        m_audit.log(QStringLiteral("Неверный логин дегустатора"));
        return result;
    }
    Taster taster = *found;
    m_audit.setActor(taster.login(), taster.roleName());
    const QDateTime now = QDateTime::currentDateTime();
    if (taster.isLockedAt(now)) {
        result.status = LoginStatus::Locked;
        result.secondsLeft = taster.secondsUntilUnlock(now);
        return result;
    }
    if (PinGenerator::isWellFormed(pin) && PasswordHasher::verify(pin, taster.secretHash())) {
        taster.registerSuccess();
        repo.saveLockState(taster);
        result.sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        repo.setSessionToken(taster.id(), result.sessionToken);
        m_audit.log(QStringLiteral("Вход дегустатора"), taster.fullName());
        result.status = LoginStatus::Ok;
        result.taster = taster;
        return result;
    }
    taster.registerFailedAttempt(now);
    repo.saveLockState(taster);
    if (taster.isLockedAt(now)) {
        m_audit.log(QStringLiteral("Блокировка входа дегустатора"), QStringLiteral("5 неверных PIN-кодов подряд"));
        result.status = LoginStatus::Locked;
        result.secondsLeft = taster.secondsUntilUnlock(now);
    } else {
        m_audit.log(QStringLiteral("Неверный PIN-код дегустатора"));
    }
    return result;
}

bool AuthService::isSessionCurrent(int tasterId, const QString& token) const
{
    return !token.isEmpty() && TasterRepository(m_db).sessionToken(tasterId) == token;
}

void AuthService::logoutTaster(int tasterId, const QString& token)
{
    if (isSessionCurrent(tasterId, token))
        TasterRepository(m_db).setSessionToken(tasterId, QString());
    m_audit.log(QStringLiteral("Выход дегустатора"));
}

}  // namespace sedm
