/**
 * @file ParticipantImporter.h
 * @brief Загрузка реестра участников из файлов CSV и XLSX (п. 4.3.1.1, 4.3.4 ТЗ).
 */
#pragma once

#include "Participant.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <optional>

namespace sedm {

/// Строка реестра после разбора файла и проверки.
struct ImportRow {
    int line = 0;                     ///< номер строки в файле
    Participant participant;          ///< ФИО, телефон, e-mail, регион
    QString nominationName;           ///< номинация из файла
    QStringList problems;             ///< ошибки — строка не загружается
    std::optional<int> duplicateOf;   ///< участник с теми же ФИО и телефоном уже в реестре
    bool newNomination = false;       ///< номинации нет в справочнике — будет добавлена
    bool isValid() const { return problems.isEmpty(); }
};

/// Результат разбора файла для предварительного просмотра перед загрузкой.
struct ImportPreview {
    QString fileName;
    QStringList headers;              ///< распознанные столбцы
    QVector<ImportRow> rows;
    int validCount() const;
    int errorCount() const;
    int duplicateCount() const;
};

/// Источник табличных данных реестра (абстракция над форматом файла).
class ParticipantSource {
public:
    virtual ~ParticipantSource() = default;
    /// Таблица строк файла (первая строка — обычно заголовки). DomainError при ошибке чтения.
    virtual QVector<QStringList> readTable(const QString& path) const = 0;
    /// Источник по расширению файла: .csv/.txt — CSV, .xlsx — Excel.
    static std::unique_ptr<ParticipantSource> forFile(const QString& path);
};

/// Чтение реестра из CSV (UTF-8, разделитель «;»).
class CsvParticipantSource : public ParticipantSource {
public:
    QVector<QStringList> readTable(const QString& path) const override;
};

/// Чтение реестра из первого листа книги Excel (XLSX) средствами QXlsx.
class XlsxParticipantSource : public ParticipantSource {
public:
    QVector<QStringList> readTable(const QString& path) const override;
};

/// Разбор таблицы реестра: распознавание столбцов и проверка строк.
class ParticipantImporter {
public:
    /// Номера столбцов таблицы (−1 — столбца нет).
    struct ColumnMap {
        int fullName = -1, surname = -1, firstName = -1, patronymic = -1;
        int phone = -1, email = -1, region = -1, nomination = -1;
        bool hasName() const { return fullName >= 0 || (surname >= 0 && firstName >= 0); }
        bool isUsable() const { return hasName() && region >= 0 && nomination >= 0; }
    };

    /// Распознаёт столбцы по заголовкам; если заголовка нет — порядок по ТЗ:
    /// ФИО; телефон; e-mail; регион; номинация.
    static ColumnMap mapHeaders(const QStringList& headers, bool* hasHeader = nullptr);
    /// Строит предварительный просмотр (без обращения к базе данных).
    static ImportPreview buildPreview(const QVector<QStringList>& table, const QString& fileName);
};

}  // namespace sedm
