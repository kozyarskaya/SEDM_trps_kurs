/**
 * @file SchemaInstaller.h
 * @brief Первичная настройка сервера: учётная запись приложения, база и схема СЭДМ.
 */
#pragma once

#include "ConnectionSettings.h"

#include <QString>

namespace sedm {

class Database;

/// Первичная настройка сервера PostgreSQL для СЭДМ (выполняется администратором
/// один раз, п. 4.1.4 ТЗ). Под учётной записью суперпользователя создаются роль
/// приложения (SCRAM-SHA-256) и база данных; приложению открывается доступ
/// только к схеме sedm (п. 4.2.1 ТЗ). Затем от имени роли приложения создаются
/// таблицы, ограничения и триггеры из сценария :/sql/schema.sql.
class SchemaInstaller {
public:
    /// Итог установки — что было создано (повторный запуск безопасен).
    struct Result {
        bool roleCreated = false;      ///< создана роль приложения (иначе обновлён пароль)
        bool databaseCreated = false;  ///< создана база данных
        bool schemaCreated = false;    ///< созданы объекты схемы
    };

    /// superuser — подключение суперпользователя к тому же серверу;
    /// app — параметры роли приложения (имя, пароль, база). DbError при ошибке.
    static Result install(const ConnectionSettings& superuser, const ConnectionSettings& app);

    /// Установлена ли схема СЭДМ в базе, к которой подключён db.
    static bool isInstalled(Database& db);
    /// Текст сценария схемы из ресурсов программы.
    static QString schemaScript();
};

}  // namespace sedm
