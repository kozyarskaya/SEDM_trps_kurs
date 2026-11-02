/**
 * @file CsvWriter.h
 * @brief Запись файлов CSV (UTF-8 с BOM для корректного открытия в Excel, разделитель «;»).
 */
#pragma once

#include <QChar>
#include <QStringList>

class QIODevice;

namespace sedm {

/// Запись строк CSV. Поля с разделителем, кавычками или переводами строк
/// заключаются в кавычки; кавычки внутри удваиваются (RFC 4180).
class CsvWriter {
public:
    /// writeBom — записать метку UTF-8 (BOM) в начало файла.
    explicit CsvWriter(QIODevice& device, bool writeBom = true, QChar delimiter = QLatin1Char(';'));
    void writeRow(const QStringList& fields);
    /// Экранирование одного поля.
    static QString escape(const QString& field, QChar delimiter = QLatin1Char(';'));

private:
    QIODevice& m_device;
    QChar m_delimiter;
};

}  // namespace sedm
