/**
 * @file AuthService.h
 * @brief Аутентификация администратора (пароль) и дегустаторов (логин + PIN-код).
 */
#pragma once

#include "Account.h"
#include "Taster.h"

#include <QString>

namespace sedm {

class Database;
class AuditService;

/// Результат попытки входа.
enum class LoginStatus {
    Ok,           ///< вход выполнен
    WrongSecret,  ///< неверный логин, пароль или PIN-код
    Locked,       ///< вход временно заблокирован после пяти ошибок
    NoAdmin       ///< пароль администратора ещё не задан (первый запуск)
};

/// Сервис аутентификации (п. 4.2.1, 4.4.5–4.4.6 ТЗ): проверка bcrypt-хэшей,
/// блокировка на 60 с после пяти неверных попыток подряд, единственный
/// действующий сеанс дегустатора (вход с другого ПК завершает прежний сеанс).
class AuthService {
public:
    static constexpr int kMinPasswordLength = 6;  ///< минимальная длина пароля администратора

    struct AdminLogin {
        LoginStatus status = LoginStatus::WrongSecret;
        int secondsLeft = 0;   ///< до снятия блокировки
    };
    struct TasterLogin {
        LoginStatus status = LoginStatus::WrongSecret;
        int secondsLeft = 0;
        Taster taster;
        QString sessionToken;  ///< токен действующего сеанса
    };

    AuthService(Database& db, AuditService& audit);

    bool adminExists() const;
    /// Задаёт пароль администратора при первом запуске (DomainError, если пароль слабый).
    void createAdmin(const QString& password);
    AdminLogin loginAdmin(const QString& password);
    /// Смена пароля: требуется текущий пароль (DomainError при ошибке).
    void changeAdminPassword(const QString& currentPassword, const QString& newPassword);

    TasterLogin loginTaster(const QString& login, const QString& pin);
    /// Действителен ли сеанс дегустатора (не открыт ли вход на другом ПК).
    bool isSessionCurrent(int tasterId, const QString& token) const;
    void logoutTaster(int tasterId, const QString& token);

    /// Проверка пароля администратора: пустая строка — пароль подходит.
    static QString passwordProblem(const QString& password);

private:
    Database& m_db;
    AuditService& m_audit;
};

}  // namespace sedm
