/**
 * @file TestDatabase.h
 * @brief Тестовая база данных PostgreSQL для интеграционных тестов СЭДМ.
 *
 * Параметры тестового сервера — переменные окружения:
 *   SEDM_TEST_PGHOST       — адрес сервера (по умолчанию localhost);
 *   SEDM_TEST_PGPORT       — порт (по умолчанию 5432);
 *   SEDM_TEST_PGSUPERUSER  — суперпользователь (по умолчанию postgres);
 *   SEDM_TEST_PGSUPERPASS  — его пароль (без него интеграционные тесты пропускаются).
 * Перед тестом база и роль приложения создаются заново (SchemaInstaller),
 * после теста — удаляются. Рабочие базы пользователя не затрагиваются.
 */
#pragma once

#include "ConnectionSettings.h"
#include "Database.h"
#include "SchemaInstaller.h"
#include "SqlUtil.h"

#include <QRandomGenerator>
#include <QString>

namespace sedm::test {

class TestDatabase {
public:
    /// Задан ли тестовый сервер.
    static bool configured() { return !qEnvironmentVariable("SEDM_TEST_PGSUPERPASS").isEmpty(); }

    /// name — имя тестовой базы; роль приложения — name + "_app".
    explicit TestDatabase(const QString& name)
    {
        m_super.host = qEnvironmentVariable("SEDM_TEST_PGHOST", QStringLiteral("localhost"));
        const int port = qEnvironmentVariableIntValue("SEDM_TEST_PGPORT");
        m_super.port = port > 0 ? port : ConnectionSettings::kDefaultPort;
        m_super.user = qEnvironmentVariable("SEDM_TEST_PGSUPERUSER", QStringLiteral("postgres"));
        m_super.password = qEnvironmentVariable("SEDM_TEST_PGSUPERPASS");
        m_super.database = QStringLiteral("postgres");
        m_app = m_super;
        m_app.database = name;
        m_app.user = name + QStringLiteral("_app");
        m_app.password = QStringLiteral("T%1x").arg(QRandomGenerator::system()->generate64(), 0, 36);
    }

    const ConnectionSettings& superuser() const { return m_super; }
    const ConnectionSettings& app() const { return m_app; }

    /// Удаляет тестовые базу и роль, если они остались от прошлого запуска.
    void drop() const
    {
        Database admin(QStringLiteral("sedm_test_drop"));
        admin.open(m_super);
        admin.execScript(QStringLiteral("DROP DATABASE IF EXISTS %1 WITH (FORCE)").arg(sql::quoteIdent(m_app.database)));
        admin.execScript(QStringLiteral("DROP ROLE IF EXISTS %1").arg(sql::quoteIdent(m_app.user)));
    }

    /// Создаёт базу и роль заново тем же способом, что и мастер первичной настройки.
    SchemaInstaller::Result create() const
    {
        drop();
        return SchemaInstaller::install(m_super, m_app);
    }

private:
    ConnectionSettings m_super;
    ConnectionSettings m_app;
};

}  // namespace sedm::test
