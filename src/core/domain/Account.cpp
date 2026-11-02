/**
 * @file Account.cpp
 * @brief Защита учётной записи от подбора пароля/PIN-кода.
 */
#include "Account.h"

namespace sedm {

void Account::setLockState(int failedAttempts, const QDateTime& lockedUntil)
{
    m_failedAttempts = failedAttempts;
    m_lockedUntil = lockedUntil;
}

bool Account::isLockedAt(const QDateTime& now) const
{
    return m_lockedUntil.isValid() && now < m_lockedUntil;
}

int Account::secondsUntilUnlock(const QDateTime& now) const
{
    if (!isLockedAt(now))
        return 0;
    // округление вверх: пока блокировка действует, на экране не появится «0 с»
    return static_cast<int>((now.msecsTo(m_lockedUntil) + 999) / 1000);
}

void Account::registerFailedAttempt(const QDateTime& now)
{
    ++m_failedAttempts;
    if (m_failedAttempts >= kMaxFailedAttempts) {
        m_lockedUntil = now.addSecs(kLockSeconds);
        m_failedAttempts = 0;  // после блокировки отсчёт начинается заново
    }
}

void Account::registerSuccess()
{
    m_failedAttempts = 0;
    m_lockedUntil = QDateTime();
}

}  // namespace sedm
