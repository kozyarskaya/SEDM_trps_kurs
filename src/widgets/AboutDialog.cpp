/**
 * @file AboutDialog.cpp
 * @brief Реализация окна «О программе».
 */
#include "AboutDialog.h"
#include "LogoWidget.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace sedm::ui {

void AboutDialog::show(QWidget* parent, const QString& programName)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("О программе"));
    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(28, 24, 28, 20);
    layout->setSpacing(12);

    auto* head = new QHBoxLayout;
    head->addWidget(new LogoWidget(false, 64));
    auto* name = new QLabel(QStringLiteral("<b>СЭДМ</b><br>Программная система экспертной дегустации мёда<br>%1, версия %2")
                                .arg(programName, QApplication::applicationVersion()));
    head->addWidget(name, 1);
    layout->addLayout(head);

    auto* text = new QLabel(QStringLiteral(
        "Закрытая (по шифрам) органолептическая оценка образцов мёда командами дегустаторов: "
        "реестр участников, шифрование образцов, корзины и команды, оценивание по четырём "
        "критериям, итоговая ведомость с местами в номинациях.<br><br>"
        "Курсовая работа по дисциплине «Технология разработки программных систем».<br>"
        "МГТУ им. Н.Э. Баумана, кафедра ИУ6, группа ИУ6-73Б, 2026 г.<br><br>"
        "Используются: Qt 5.15 (LGPL v3), QXlsx (MIT), crypt_blowfish (общественное достояние), "
        "библиотека libpq PostgreSQL (лицензия PostgreSQL)."));
    text->setWordWrap(true);
    text->setMinimumWidth(460);
    layout->addWidget(text);

    auto* buttons = new QDialogButtonBox;
    auto* ok = buttons->addButton(QStringLiteral("Закрыть"), QDialogButtonBox::AcceptRole);
    ok->setProperty("kind", "primary");
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(buttons);
    dlg.exec();
}

}  // namespace sedm::ui
