/**
 * @file CsvWriter.cpp
 * @brief Реализация записи CSV.
 */
#include "CsvWriter.h"

#include <QIODevice>

namespace sedm {

CsvWriter::CsvWriter(QIODevice& device, bool writeBom, QChar delimiter)
    : m_device(device), m_delimiter(delimiter)
{
    if (writeBom)
        m_device.write("\xEF\xBB\xBF");
}

QString CsvWriter::escape(const QString& field, QChar delimiter)
{
    if (field.contains(delimiter) || field.contains(QLatin1Char('"')) || field.contains(QLatin1Char('\n'))
        || field.contains(QLatin1Char('\r'))) {
        QString s = field;
        s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + s + QLatin1Char('"');
    }
    return field;
}

void CsvWriter::writeRow(const QStringList& fields)
{
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString& f : fields)
        escaped.append(escape(f, m_delimiter));
    m_device.write((escaped.join(m_delimiter) + QStringLiteral("\r\n")).toUtf8());
}

}  // namespace sedm
