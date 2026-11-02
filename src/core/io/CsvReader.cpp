/**
 * @file CsvReader.cpp
 * @brief Конечный автомат разбора CSV.
 *
 * Состояния: «вне кавычек» и «внутри кавычек». Внутри кавычек разделитель и
 * перевод строки — обычные символы, пара кавычек "" — одна кавычка.
 */
#include "CsvReader.h"
#include "DomainError.h"

#include <QFile>
#include <QTextCodec>

namespace sedm {

QVector<QStringList> CsvReader::parse(const QString& source, QChar delimiter)
{
    QString text = source;
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);

    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool inQuotes = false;
    bool fieldStarted = false;

    auto endField = [&]() {
        row.append(field);
        field.clear();
        fieldStarted = false;
    };
    auto endRow = [&]() {
        endField();
        bool empty = true;
        for (const QString& f : row)
            if (!f.trimmed().isEmpty()) {
                empty = false;
                break;
            }
        if (!empty)
            rows.append(row);
        row.clear();
    };

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text[i + 1] == QLatin1Char('"')) {
                    field += QLatin1Char('"');   // удвоенная кавычка внутри поля
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += ch;
            }
            continue;
        }
        if (ch == QLatin1Char('"') && !fieldStarted) {
            inQuotes = true;
            fieldStarted = true;
        } else if (ch == delimiter) {
            endField();
        } else if (ch == QLatin1Char('\r')) {
            if (i + 1 < text.size() && text[i + 1] == QLatin1Char('\n'))
                ++i;
            endRow();
        } else if (ch == QLatin1Char('\n')) {
            endRow();
        } else {
            field += ch;
            fieldStarted = true;
        }
    }
    if (fieldStarted || !field.isEmpty() || !row.isEmpty())
        endRow();
    return rows;
}

QChar CsvReader::detectDelimiter(const QString& firstLine)
{
    if (firstLine.contains(QLatin1Char(';')))
        return QLatin1Char(';');
    if (firstLine.contains(QLatin1Char('\t')))
        return QLatin1Char('\t');
    if (firstLine.contains(QLatin1Char(',')))
        return QLatin1Char(',');
    return QLatin1Char(';');
}

QVector<QStringList> CsvReader::readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw DomainError(QStringLiteral("Не удалось открыть файл «%1»: %2").arg(path, file.errorString()));
    const QByteArray bytes = file.readAll();
    // Требуется UTF-8; если файл в Windows-1251 (частая ошибка), перекодируем
    QTextCodec::ConverterState state;
    QString text = QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
    if (state.invalidChars > 0)
        text = QTextCodec::codecForName("Windows-1251")->toUnicode(bytes);
    const QString first = text.section(QLatin1Char('\n'), 0, 0);
    return parse(text, detectDelimiter(first));
}

}  // namespace sedm
