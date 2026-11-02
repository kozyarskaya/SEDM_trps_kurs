-- =============================================================================
-- СЭДМ — ручная первичная настройка сервера PostgreSQL (альтернатива мастеру
-- первичной настройки в программе администратора).
--
-- Запуск (от имени суперпользователя postgres), из каталога с этим файлом:
--   psql -h localhost -p 5432 -U postgres -d postgres ^
--        -v app_password=ПАРОЛЬ_ПРИЛОЖЕНИЯ -f setup_manual.sql
--
-- Создаётся учётная запись приложения sedm_app (аутентификация SCRAM-SHA-256)
-- и база sedm; приложению доступна только схема данных sedm (п. 4.2.1 ТЗ).
-- =============================================================================
\set ON_ERROR_STOP on
SET password_encryption = 'scram-sha-256';
CREATE ROLE sedm_app LOGIN PASSWORD :'app_password';
CREATE DATABASE sedm OWNER postgres ENCODING 'UTF8' TEMPLATE template0;

\connect sedm
REVOKE ALL ON DATABASE sedm FROM PUBLIC;
GRANT CONNECT, TEMPORARY, CREATE ON DATABASE sedm TO sedm_app;
REVOKE ALL ON SCHEMA public FROM PUBLIC;
CREATE SCHEMA sedm AUTHORIZATION sedm_app;
ALTER ROLE sedm_app IN DATABASE sedm SET search_path = sedm;

-- Объекты схемы создаются от имени sedm_app, чтобы приложение было их владельцем
SET ROLE sedm_app;
\ir schema.sql
RESET ROLE;
