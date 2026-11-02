/**
 * @file ConnectionSettings.cpp
 * @brief Хранение параметров подключения; защита пароля (DPAPI в Windows).
 */
#include "ConnectionSettings.h"

#include <QByteArray>
#include <QSettings>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dpapi.h>
#endif

namespace sedm {

namespace {
const char* kGroup = "connection";
const QString kProtectedPrefix = QStringLiteral("dpapi:");
const QString kPlainPrefix = QStringLiteral("b64:");
}  // namespace

bool ConnectionSettings::isComplete() const
{
    return !host.trimmed().isEmpty() && port > 0 && port <= 65535 && !database.trimmed().isEmpty()
           && !user.trimmed().isEmpty();
}

ConnectionSettings ConnectionSettings::load(QSettings& settings)
{
    ConnectionSettings s;
    settings.beginGroup(QLatin1String(kGroup));
    s.host = settings.value(QStringLiteral("host"), s.host).toString();
    s.port = settings.value(QStringLiteral("port"), s.port).toInt();
    s.database = settings.value(QStringLiteral("database"), s.database).toString();
    s.user = settings.value(QStringLiteral("user"), s.user).toString();
    s.password = unprotect(settings.value(QStringLiteral("password")).toString());
    settings.endGroup();
    return s;
}

void ConnectionSettings::save(QSettings& settings) const
{
    settings.beginGroup(QLatin1String(kGroup));
    settings.setValue(QStringLiteral("host"), host.trimmed());
    settings.setValue(QStringLiteral("port"), port);
    settings.setValue(QStringLiteral("database"), database.trimmed());
    settings.setValue(QStringLiteral("user"), user.trimmed());
    settings.setValue(QStringLiteral("password"), protect(password));
    settings.endGroup();
    settings.sync();
}

bool ConnectionSettings::exist(QSettings& settings)
{
    return settings.contains(QStringLiteral("connection/host"));
}

QString ConnectionSettings::protect(const QString& secret)
{
    const QByteArray plain = secret.toUtf8();
#ifdef Q_OS_WIN
    DATA_BLOB in{static_cast<DWORD>(plain.size()),
                 reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()))};
    DATA_BLOB out{0, nullptr};
    if (CryptProtectData(&in, L"SEDM", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        const QByteArray cipher(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        return kProtectedPrefix + QString::fromLatin1(cipher.toBase64());
    }
#endif
    // Резервный вариант (другие ОС): только скрытие от случайного просмотра
    return kPlainPrefix + QString::fromLatin1(plain.toBase64());
}

QString ConnectionSettings::unprotect(const QString& stored)
{
    if (stored.startsWith(kPlainPrefix))
        return QString::fromUtf8(QByteArray::fromBase64(stored.mid(kPlainPrefix.size()).toLatin1()));
#ifdef Q_OS_WIN
    if (stored.startsWith(kProtectedPrefix)) {
        QByteArray cipher = QByteArray::fromBase64(stored.mid(kProtectedPrefix.size()).toLatin1());
        DATA_BLOB in{static_cast<DWORD>(cipher.size()), reinterpret_cast<BYTE*>(cipher.data())};
        DATA_BLOB out{0, nullptr};
        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const QString plain = QString::fromUtf8(reinterpret_cast<const char*>(out.pbData),
                                                    static_cast<int>(out.cbData));
            LocalFree(out.pbData);
            return plain;
        }
    }
#endif
    return QString();
}

}  // namespace sedm
