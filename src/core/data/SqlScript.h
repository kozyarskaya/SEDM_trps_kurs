/**
 * @file SqlScript.h
 * @brief Разбор SQL-сценариев (схема БД, резервные копии) на отдельные команды.
 */
#pragma once

#include <QString>
#include <QStringList>

namespace sedm {

/// Лексический разбор SQL-сценария с учётом синтаксиса PostgreSQL: строковых
/// литералов ('…', E'…'), идентификаторов в кавычках ("…"), строк в долларовых
/// кавычках ($$…$$, $тег$…$тег$), комментариев (-- …, /* … */) и метакоманд
/// psql (строки, начинающиеся с «\»).
///
/// Сценарий выполняется по одной команде: драйвер Qt для PostgreSQL при
/// отправке нескольких команд одной строкой сообщает только о результате
/// первой из них, и ошибка в последующих осталась бы незамеченной.
class SqlScript {
public:
    /// Команды сценария по порядку (без завершающей «;»). Комментарии и
    /// метакоманды psql (\restrict, \unrestrict, \connect …), которые pg_dump
    /// добавляет в дамп, пропускаются: сервер их не понимает.
    static QStringList splitStatements(const QString& script);
    /// Текст сценария без строк метакоманд psql.
    static QString stripPsqlMetaCommands(const QString& script);
    /// Содержит ли дамп данные в формате COPY … FROM stdin (нужна утилита psql).
    static bool containsCopyFromStdin(const QString& script);
};

}  // namespace sedm
