/**
 * @file AppPaths.h
 * @brief Расположение файлов настроек и рабочих данных клиента СЭДМ.
 */
#pragma once

#include <QString>
#include <memory>

class QSettings;

namespace sedm {

/// Пути к файлам клиента. Настройки (параметры подключения) хранятся в
/// %LOCALAPPDATA%\SEDM\sedm.ini (GenericConfigLocation Qt 5), рабочие данные — там же:
/// автоматические резервные копии, черновики оценок, отложенные записи журнала.
class AppPaths {
public:
    static QString settingsFile();          ///< файл настроек (INI)
    static std::unique_ptr<QSettings> settings();
    static QString dataDir();               ///< каталог рабочих данных
    static QString autoBackupDir();         ///< автоматические копии БД (каждые 5 минут)
    static QString draftDir();              ///< локальные черновики оценок дегустатора
    static QString pendingAuditFile();      ///< записи журнала, ожидающие связи с сервером
};

}  // namespace sedm
