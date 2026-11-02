/**
 * @file Localization.cpp
 * @brief Подключение русских переводов Qt.
 */
#include "Localization.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace sedm::ui {

void installRussian(QApplication& app)
{
    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));
    static QTranslator qtTranslator;   // живёт до конца работы программы
    // переводы Qt — рядом с программой (после установки) или в каталоге Qt (при разработке)
    const QStringList dirs = {QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations")),
                              QLibraryInfo::location(QLibraryInfo::TranslationsPath)};
    for (const QString& dir : dirs)
        if (qtTranslator.load(QLocale(QLocale::Russian), QStringLiteral("qtbase"), QStringLiteral("_"), dir)) {
            app.installTranslator(&qtTranslator);
            break;
        }
}

}  // namespace sedm::ui
