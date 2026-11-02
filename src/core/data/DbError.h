/**
 * @file DbError.h
 * @brief Исключение «ошибка работы с базой данных» с понятным пользователю текстом.
 */
#pragma once

#include <QSqlError>
#include <QString>
#include <stdexcept>

namespace sedm {

/// Ошибка СУБД. Текст переводится на язык пользователя: например, нарушение
/// уникальности шифра превращается в «Шифр образца уже используется».
class DbError : public std::runtime_error {
public:
    /// Категория ошибки — для выбора реакции интерфейса.
    enum class Kind {
        Connection,   ///< нет связи с сервером, неверный пароль, нет базы
        Constraint,   ///< нарушено правило целостности (уникальность, связь, проверка)
        Other         ///< прочие ошибки выполнения запроса
    };

    DbError(Kind kind, const QString& message, const QString& sqlState = QString(),
            const QString& details = QString());

    /// Преобразует ошибку Qt SQL (драйвер QPSQL) в DbError.
    static DbError fromSqlError(const QSqlError& error);

    Kind kind() const { return m_kind; }
    QString message() const { return m_message; }      ///< текст для пользователя
    QString sqlState() const { return m_sqlState; }    ///< код SQLSTATE PostgreSQL
    QString details() const { return m_details; }      ///< исходный текст сервера

private:
    Kind m_kind;
    QString m_message;
    QString m_sqlState;
    QString m_details;
};

}  // namespace sedm
