-- =============================================================================
-- СЭДМ — проверка ограничений целостности и триггеров схемы БД.
-- Выполняется от имени sedm_app внутри транзакции с откатом (данные не остаются):
--   psql -h localhost -p 5434 -U sedm_app -d sedm -f test_constraints.sql
-- Каждая проверка печатает «ОК: …» или прерывает выполнение с «ОШИБКА ТЕСТА: …».
-- =============================================================================
\set ON_ERROR_STOP on
BEGIN;
SET search_path TO sedm;

CREATE TEMP TABLE t_result (n serial, line text);

-- тестовые данные: 2 номинации, 2 участника, команда с дегустатором, корзина на 2 образца
INSERT INTO nomination (id, name) VALUES (101, 'Липовый'), (102, 'Гречишный');
INSERT INTO participant (id, full_name, region, nomination_id)
VALUES (201, 'Абрамова Лидия Ивановна', 'Тульская обл.', 101),
       (202, 'Беляев Сергей Николаевич', 'Алтайский край', 102);
INSERT INTO team (id, number, name) VALUES (301, 1, 'Эксперты');
INSERT INTO taster (id, team_id, full_name, login, login_key, pin_hash)
VALUES (401, 301, 'Иванова Мария Петровна', 'Иванова', 'иванова', repeat('x', 60));
INSERT INTO basket (id, number, nomination_id, desired_count, team_id) VALUES (501, 3, 101, 2, 301);
INSERT INTO sample (id, cipher, participant_id, nomination_id, basket_id)
VALUES (601, '4827', 201, 101, 501), (602, '6802', 201, 101, 501);
INSERT INTO sample (id, cipher, participant_id, nomination_id) VALUES (603, '1593', 202, 102),
                                                                      (604, '3390', 201, 101);

-- вспомогательная процедура: выполнить оператор и убедиться, что он отклонён
CREATE FUNCTION pg_temp.expect_error(stmt text, title text) RETURNS void LANGUAGE plpgsql AS $$
BEGIN
    BEGIN
        EXECUTE stmt;
    EXCEPTION WHEN OTHERS THEN
        INSERT INTO t_result (line) VALUES ('ОК: ' || title || ' — отклонено: ' || SQLERRM);
        RETURN;
    END;
    RAISE EXCEPTION 'ОШИБКА ТЕСТА: % — оператор не был отклонён', title;
END $$;

SELECT pg_temp.expect_error($q$INSERT INTO sample (cipher, participant_id, nomination_id) VALUES ('4827', 201, 101)$q$,
                            'повтор шифра');
SELECT pg_temp.expect_error($q$INSERT INTO sample (cipher, participant_id, nomination_id) VALUES ('0123', 201, 101)$q$,
                            'шифр с ведущим нулём');
SELECT pg_temp.expect_error($q$UPDATE sample SET basket_id = 501 WHERE id = 603$q$,
                            'образец чужой номинации в корзину');
SELECT pg_temp.expect_error($q$UPDATE sample SET basket_id = 501 WHERE id = 604$q$,
                            'образец сверх желаемого количества');
SELECT pg_temp.expect_error($q$UPDATE basket SET desired_count = 1 WHERE id = 501$q$,
                            'желаемое количество меньше текущего');
SELECT pg_temp.expect_error($q$UPDATE basket SET nomination_id = 102 WHERE id = 501$q$,
                            'смена номинации непустой корзины');
SELECT pg_temp.expect_error($q$INSERT INTO evaluation (taster_id, sample_id, taste) VALUES (401, 601, 101)$q$,
                            'оценка 101 (вне шкалы 0–100)');
SELECT pg_temp.expect_error($q$INSERT INTO evaluation (taster_id, sample_id, taste) VALUES (401, 604, 50)$q$,
                            'оценка образца не из корзин команды');

-- полные оценки первого образца, неполная — второго
INSERT INTO evaluation (taster_id, sample_id, appearance, consistency, aroma, taste)
VALUES (401, 601, 85, 78, 84, 83);
INSERT INTO evaluation (taster_id, sample_id, appearance, consistency) VALUES (401, 602, 90, 86);
INSERT INTO t_result (line) VALUES ('ОК: допустимые оценки сохранены');

SELECT pg_temp.expect_error($q$INSERT INTO basket_progress (taster_id, basket_id) VALUES (401, 501)$q$,
                            'завершение корзины с неполными оценками');
SELECT pg_temp.expect_error($q$UPDATE sample SET basket_id = NULL WHERE id = 601$q$,
                            'удаление оценённого образца из корзины');
SELECT pg_temp.expect_error($q$DELETE FROM sample WHERE id = 601$q$,
                            'удаление оценённого образца');
SELECT pg_temp.expect_error($q$UPDATE basket SET team_id = NULL WHERE id = 501$q$,
                            'снятие оцениваемой корзины с команды');
SELECT pg_temp.expect_error($q$DELETE FROM participant WHERE id = 201$q$,
                            'удаление участника, у которого есть образцы');

UPDATE evaluation SET aroma = 70, taste = 75 WHERE taster_id = 401 AND sample_id = 602;
INSERT INTO basket_progress (taster_id, basket_id) VALUES (401, 501);
INSERT INTO t_result (line) VALUES ('ОК: корзина завершена при полных оценках');

SELECT pg_temp.expect_error($q$UPDATE evaluation SET taste = 10 WHERE taster_id = 401 AND sample_id = 601$q$,
                            'изменение оценки после завершения корзины');

SELECT line FROM t_result ORDER BY n;
ROLLBACK;
