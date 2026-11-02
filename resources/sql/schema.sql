-- =============================================================================
-- СЭДМ — программная система экспертной дегустации мёда
-- Схема базы данных (PostgreSQL 14 и выше), версия 1.
--
-- Выполняется от имени учётной записи приложения (роль sedm_app), которой
-- принадлежит схема sedm (см. setup.sql). Ограничения целостности дублируют
-- правила программы (п. 4.4.3 ТЗ): даже при ошибке в клиенте или ручном
-- изменении данных база не допустит нарушения правил дегустации.
-- =============================================================================

SET search_path TO sedm;

-- Версия схемы БД (для будущих обновлений)
CREATE TABLE schema_version (
    version     integer PRIMARY KEY,
    applied_at  timestamptz NOT NULL DEFAULT now()
);

-- -----------------------------------------------------------------------------
-- Справочник номинаций
CREATE TABLE nomination (
    id    serial PRIMARY KEY,
    name  varchar(100) NOT NULL CHECK (length(btrim(name)) > 0)
);
CREATE UNIQUE INDEX nomination_name_uq ON nomination (lower(name));

-- -----------------------------------------------------------------------------
-- Реестр участников
CREATE TABLE participant (
    id             serial PRIMARY KEY,
    full_name      varchar(150) NOT NULL CHECK (length(btrim(full_name)) > 0),
    phone          varchar(30)  NOT NULL DEFAULT '',
    email          varchar(120) NOT NULL DEFAULT '',
    region         varchar(100) NOT NULL CHECK (length(btrim(region)) > 0),
    nomination_id  integer      NOT NULL REFERENCES nomination (id) ON DELETE RESTRICT
);
CREATE INDEX participant_nomination_idx ON participant (nomination_id);

-- -----------------------------------------------------------------------------
-- Дегустационные команды
CREATE TABLE team (
    id      serial PRIMARY KEY,
    number  integer      NOT NULL UNIQUE CHECK (number > 0),
    name    varchar(100) NOT NULL DEFAULT ''
);

-- -----------------------------------------------------------------------------
-- Корзины образцов (одна корзина — не более одной команды)
CREATE TABLE basket (
    id             serial PRIMARY KEY,
    number         integer NOT NULL UNIQUE CHECK (number > 0),
    nomination_id  integer NOT NULL REFERENCES nomination (id) ON DELETE RESTRICT,
    desired_count  integer NOT NULL CHECK (desired_count BETWEEN 1 AND 100),
    team_id        integer REFERENCES team (id) ON DELETE SET NULL
);
CREATE INDEX basket_team_idx ON basket (team_id);

-- -----------------------------------------------------------------------------
-- Образцы мёда: сквозной порядковый номер и уникальный шифр
CREATE SEQUENCE sample_seq_number START 1;
CREATE TABLE sample (
    id              serial PRIMARY KEY,
    seq_number      integer NOT NULL UNIQUE DEFAULT nextval('sample_seq_number'),
    cipher          char(4) NOT NULL UNIQUE CHECK (cipher ~ '^[1-9][0-9]{3}$'),
    participant_id  integer NOT NULL REFERENCES participant (id) ON DELETE RESTRICT,
    nomination_id   integer NOT NULL REFERENCES nomination (id) ON DELETE RESTRICT,
    basket_id       integer REFERENCES basket (id) ON DELETE SET NULL,
    created_at      timestamptz NOT NULL DEFAULT now()
);
ALTER SEQUENCE sample_seq_number OWNED BY sample.seq_number;
CREATE INDEX sample_basket_idx ON sample (basket_id);
CREATE INDEX sample_participant_idx ON sample (participant_id);

-- -----------------------------------------------------------------------------
-- Дегустаторы (учётные записи: логин — фамилия кириллицей, PIN — bcrypt-хэш)
CREATE TABLE taster (
    id               serial PRIMARY KEY,
    team_id          integer      NOT NULL REFERENCES team (id) ON DELETE CASCADE,
    full_name        varchar(150) NOT NULL CHECK (length(btrim(full_name)) > 0),
    login            varchar(60)  NOT NULL CHECK (length(btrim(login)) > 0),
    login_key        varchar(60)  NOT NULL UNIQUE,     -- логин для поиска: строчные, «ё» -> «е»
    pin_hash         char(60)     NOT NULL,
    failed_attempts  integer      NOT NULL DEFAULT 0,
    locked_until     timestamptz,
    session_token    uuid,                             -- текущий сеанс (вход с другого ПК)
    created_at       timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX taster_team_idx ON taster (team_id);

-- -----------------------------------------------------------------------------
-- Оценки: четыре критерия 0–100 и примечание
CREATE TABLE evaluation (
    taster_id    integer  NOT NULL REFERENCES taster (id) ON DELETE RESTRICT,
    sample_id    integer  NOT NULL REFERENCES sample (id) ON DELETE RESTRICT,
    appearance   smallint CHECK (appearance  BETWEEN 0 AND 100),
    consistency  smallint CHECK (consistency BETWEEN 0 AND 100),
    aroma        smallint CHECK (aroma       BETWEEN 0 AND 100),
    taste        smallint CHECK (taste       BETWEEN 0 AND 100),
    note         varchar(500) NOT NULL DEFAULT '',
    saved_at     timestamptz  NOT NULL DEFAULT now(),
    PRIMARY KEY (taster_id, sample_id)
);
CREATE INDEX evaluation_sample_idx ON evaluation (sample_id);

-- Черновики оценок — контрольные точки каждые 5 минут (п. 4.4.4 ТЗ)
CREATE TABLE score_draft (
    taster_id    integer  NOT NULL REFERENCES taster (id) ON DELETE CASCADE,
    sample_id    integer  NOT NULL REFERENCES sample (id) ON DELETE CASCADE,
    appearance   smallint CHECK (appearance  BETWEEN 0 AND 100),
    consistency  smallint CHECK (consistency BETWEEN 0 AND 100),
    aroma        smallint CHECK (aroma       BETWEEN 0 AND 100),
    taste        smallint CHECK (taste       BETWEEN 0 AND 100),
    note         varchar(500) NOT NULL DEFAULT '',
    saved_at     timestamptz  NOT NULL DEFAULT now(),
    PRIMARY KEY (taster_id, sample_id)
);

-- Завершение дегустации корзины дегустатором
CREATE TABLE basket_progress (
    taster_id    integer     NOT NULL REFERENCES taster (id) ON DELETE CASCADE,
    basket_id    integer     NOT NULL REFERENCES basket (id) ON DELETE CASCADE,
    finished_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (taster_id, basket_id)
);

-- -----------------------------------------------------------------------------
-- Конфигурация сеанса дегустации (единственная строка)
CREATE TABLE tasting_session (
    id             integer PRIMARY KEY DEFAULT 1 CHECK (id = 1),
    title          varchar(150) NOT NULL DEFAULT 'Дегустация мёда',
    starts_at      timestamptz,
    allow_reentry  boolean NOT NULL DEFAULT false
);
INSERT INTO tasting_session (id) VALUES (1);

-- Учётная запись администратора (единственная; пароль — bcrypt-хэш)
CREATE TABLE app_admin (
    id               integer  PRIMARY KEY DEFAULT 1 CHECK (id = 1),
    password_hash    char(60) NOT NULL,
    failed_attempts  integer  NOT NULL DEFAULT 0,
    locked_until     timestamptz
);

-- Журнал действий пользователей
CREATE TABLE audit_log (
    id           bigserial PRIMARY KEY,
    at           timestamptz  NOT NULL DEFAULT now(),
    actor        varchar(150) NOT NULL,
    role         varchar(30)  NOT NULL,
    workstation  varchar(100) NOT NULL DEFAULT '',
    action       varchar(100) NOT NULL,
    details      text         NOT NULL DEFAULT ''
);
CREATE INDEX audit_log_at_idx ON audit_log (at);

-- =============================================================================
-- Триггеры правил дегустации
-- Функции выполняются со своим путём поиска схем (search_path), поэтому
-- работают одинаково при любом подключении: из программы, psql или pg_restore.
-- =============================================================================

-- Образец — только в корзину своей номинации и не сверх желаемого количества;
-- оценённый образец нельзя убрать из корзины или переложить в другую.
CREATE FUNCTION check_sample_basket() RETURNS trigger LANGUAGE plpgsql
    SET search_path = sedm, pg_catalog AS $$
DECLARE
    b      basket%ROWTYPE;
    filled integer;
BEGIN
    IF TG_OP = 'UPDATE' AND OLD.basket_id IS NOT NULL
       AND NEW.basket_id IS DISTINCT FROM OLD.basket_id
       AND EXISTS (SELECT 1 FROM evaluation WHERE sample_id = OLD.id) THEN
        RAISE EXCEPTION 'Образец % уже оценивался — его нельзя убрать из корзины', OLD.cipher
            USING ERRCODE = 'check_violation';
    END IF;
    IF NEW.basket_id IS NULL THEN
        RETURN NEW;
    END IF;
    SELECT * INTO b FROM basket WHERE id = NEW.basket_id;
    IF b.nomination_id <> NEW.nomination_id THEN
        RAISE EXCEPTION 'Образец % относится к другой номинации, чем корзина № %', NEW.cipher, b.number
            USING ERRCODE = 'check_violation';
    END IF;
    IF TG_OP = 'INSERT' OR NEW.basket_id IS DISTINCT FROM OLD.basket_id THEN
        SELECT count(*) INTO filled FROM sample WHERE basket_id = NEW.basket_id AND id <> NEW.id;
        IF filled >= b.desired_count THEN
            RAISE EXCEPTION 'Корзина № % заполнена: % из % образцов', b.number, filled, b.desired_count
                USING ERRCODE = 'check_violation';
        END IF;
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER sample_basket_check BEFORE INSERT OR UPDATE OF basket_id, nomination_id ON sample
    FOR EACH ROW EXECUTE FUNCTION check_sample_basket();

-- Корзина: номинация меняется только у пустой; желаемое количество не меньше
-- текущего; корзину с оценками нельзя снять с команды.
CREATE FUNCTION check_basket_change() RETURNS trigger LANGUAGE plpgsql
    SET search_path = sedm, pg_catalog AS $$
DECLARE
    filled integer;
BEGIN
    SELECT count(*) INTO filled FROM sample WHERE basket_id = NEW.id;
    IF NEW.nomination_id <> OLD.nomination_id AND filled > 0 THEN
        RAISE EXCEPTION 'Номинацию корзины № % можно изменить только у пустой корзины', NEW.number
            USING ERRCODE = 'check_violation';
    END IF;
    IF NEW.desired_count < filled THEN
        RAISE EXCEPTION 'В корзине № % уже % образцов — желаемое количество не может быть меньше',
            NEW.number, filled USING ERRCODE = 'check_violation';
    END IF;
    IF NEW.team_id IS DISTINCT FROM OLD.team_id AND OLD.team_id IS NOT NULL
       AND EXISTS (SELECT 1 FROM evaluation e JOIN sample s ON s.id = e.sample_id
                   WHERE s.basket_id = NEW.id) THEN
        RAISE EXCEPTION 'Корзина № % уже оценивается — её нельзя передать другой команде', NEW.number
            USING ERRCODE = 'check_violation';
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER basket_change_check BEFORE UPDATE ON basket
    FOR EACH ROW EXECUTE FUNCTION check_basket_change();

-- Оценка: образец из корзины команды дегустатора; после завершения корзины —
-- без изменений (п. 4.3.1.2 ТЗ).
CREATE FUNCTION check_evaluation() RETURNS trigger LANGUAGE plpgsql
    SET search_path = sedm, pg_catalog AS $$
DECLARE
    sample_basket integer;
    basket_team   integer;
    taster_team   integer;
BEGIN
    SELECT s.basket_id, b.team_id INTO sample_basket, basket_team
      FROM sample s LEFT JOIN basket b ON b.id = s.basket_id
     WHERE s.id = NEW.sample_id;
    SELECT team_id INTO taster_team FROM taster WHERE id = NEW.taster_id;
    IF sample_basket IS NULL OR basket_team IS DISTINCT FROM taster_team THEN
        RAISE EXCEPTION 'Образец не входит в корзины, назначенные команде дегустатора'
            USING ERRCODE = 'check_violation';
    END IF;
    IF EXISTS (SELECT 1 FROM basket_progress
                WHERE taster_id = NEW.taster_id AND basket_id = sample_basket) THEN
        RAISE EXCEPTION 'Дегустация корзины завершена — оценки изменить нельзя'
            USING ERRCODE = 'check_violation';
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER evaluation_check BEFORE INSERT OR UPDATE ON evaluation
    FOR EACH ROW EXECUTE FUNCTION check_evaluation();

-- Завершение корзины — только при полных оценках всех её образцов.
CREATE FUNCTION check_basket_finish() RETURNS trigger LANGUAGE plpgsql
    SET search_path = sedm, pg_catalog AS $$
DECLARE
    total     integer;
    complete  integer;
BEGIN
    SELECT count(*) INTO total FROM sample WHERE basket_id = NEW.basket_id;
    SELECT count(*) INTO complete
      FROM evaluation e JOIN sample s ON s.id = e.sample_id
     WHERE e.taster_id = NEW.taster_id AND s.basket_id = NEW.basket_id
       AND e.appearance IS NOT NULL AND e.consistency IS NOT NULL
       AND e.aroma IS NOT NULL AND e.taste IS NOT NULL;
    IF total = 0 OR complete < total THEN
        RAISE EXCEPTION 'Нельзя завершить дегустацию: оценено % из % образцов', complete, total
            USING ERRCODE = 'check_violation';
    END IF;
    RETURN NEW;
END $$;
CREATE TRIGGER basket_finish_check BEFORE INSERT ON basket_progress
    FOR EACH ROW EXECUTE FUNCTION check_basket_finish();

INSERT INTO schema_version (version) VALUES (1);
