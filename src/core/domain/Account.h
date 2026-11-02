/**
 * @file Account.h
 * @brief Учётная запись пользователя СЭДМ (абстрактный класс) и учётная запись администратора.
 */
#pragma once

#include <QDateTime>
#include <QString>

namespace sedm {

/// Учётная запись: логин, хэш пароля или PIN-кода (bcrypt) и защита от подбора —
/// после kMaxFailedAttempts неверных попыток вход блокируется на kLockSeconds секунд
/// (п. 4.4.5, 4.4.6 ТЗ). Конкретные роли — AdminAccount и Taster.
class Account {
public:
    static constexpr int kMaxFailedAttempts = 5;  ///< допустимое число ошибок подряд
    static constexpr int kLockSeconds = 60;       ///< длительность блокировки, с

    virtual ~Account() = default;

    /// Название роли для журнала действий («администратор», «дегустатор»).
    virtual QString roleName() const = 0;

    const QString& login() const { return m_login; }
    void setLogin(const QString& login) { m_login = login.trimmed(); }

    const QString& secretHash() const { return m_secretHash; }
    void setSecretHash(const QString& hash) { m_secretHash = hash; }

    int failedAttempts() const { return m_failedAttempts; }
    const QDateTime& lockedUntil() const { return m_lockedUntil; }
    /// Загрузка состояния блокировки из базы данных.
    void setLockState(int failedAttempts, const QDateTime& lockedUntil);

    /// Заблокирован ли вход в момент now.
    bool isLockedAt(const QDateTime& now) const;
    /// Сколько секунд осталось до снятия блокировки (0 — не заблокирован).
    int secondsUntilUnlock(const QDateTime& now) const;
    /// Учитывает неудачную попытку; на пятой подряд включает блокировку.
    void registerFailedAttempt(const QDateTime& now);
    /// Успешный вход: счётчик ошибок сбрасывается.
    void registerSuccess();

protected:
    Account() = default;

private:
    QString m_login;           ///< логин
    QString m_secretHash;      ///< bcrypt-хэш пароля или PIN-кода
    int m_failedAttempts = 0;  ///< неудачных попыток подряд
    QDateTime m_lockedUntil;   ///< время снятия блокировки
};

/// Учётная запись администратора (единственная, п. 4.5.3 ТЗ).
class AdminAccount : public Account {
public:
    AdminAccount() { setLogin(QStringLiteral("администратор")); }
    QString roleName() const override { return QStringLiteral("администратор"); }
};

}  // namespace sedm
