/**
 * @file ConnectionSettings.h
 * @brief Параметры подключения к серверу PostgreSQL и их хранение на рабочем месте.
 */
#pragma once

#include <QString>

class QSettings;

namespace sedm {

/// Параметры подключения клиента СЭДМ к серверу БД (п. 4.1.2–4.1.3 ТЗ).
/// Хранятся в файле настроек пользователя; пароль учётной записи приложения
/// записывается в защищённом виде (Windows DPAPI), а не открытым текстом.
class ConnectionSettings {
public:
    static constexpr int kDefaultPort = 5432;  ///< стандартный порт PostgreSQL

    QString host = QStringLiteral("localhost");   ///< адрес сервера
    int port = kDefaultPort;                      ///< порт сервера
    QString database = QStringLiteral("sedm");    ///< имя базы данных
    QString user = QStringLiteral("sedm_app");    ///< учётная запись приложения
    QString password;                             ///< пароль учётной записи

    /// Заполнены ли обязательные поля.
    bool isComplete() const;

    /// Загрузка из настроек (раздел «connection»); отсутствующие поля — по умолчанию.
    static ConnectionSettings load(QSettings& settings);
    /// Сохранение в настройки; пароль — в защищённом виде.
    void save(QSettings& settings) const;
    /// Есть ли сохранённые параметры подключения.
    static bool exist(QSettings& settings);

    /// Защита пароля средствами ОС (DPAPI) и обратное преобразование.
    static QString protect(const QString& secret);
    static QString unprotect(const QString& stored);
};

}  // namespace sedm
