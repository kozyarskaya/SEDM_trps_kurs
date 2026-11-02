/**
 * @file CsvReader.h
 * @brief Чтение файлов CSV (UTF-8, разделитель «;», кавычки по RFC 4180).
 */
#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sedm {

/// Разбор CSV: поля в кавычках могут содержать разделитель, кавычки ("" — кавычка)
/// и переводы строк. Пустые строки пропускаются; метка порядка байтов (BOM) удаляется.
class CsvReader {
public:
    /// Разбирает текст CSV с заданным разделителем.
    static QVector<QStringList> parse(const QString& text, QChar delimiter = QLatin1Char(';'));
    /// Читает файл в кодировке UTF-8; разделитель определяется по первой строке
    /// («;» по ТЗ, «,» — если точки с запятой нет). DomainError, если файл не открыть.
    static QVector<QStringList> readFile(const QString& path);
    /// Определение разделителя по первой строке.
    static QChar detectDelimiter(const QString& firstLine);
};

}  // namespace sedm
