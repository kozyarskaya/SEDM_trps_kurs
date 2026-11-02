/**
 * @file AboutDialog.h
 * @brief Окно «О программе».
 */
#pragma once

#include <QString>

class QWidget;

namespace sedm::ui {

/// Сведения о программе: назначение, версия, разработчик, используемые библиотеки.
class AboutDialog {
public:
    /// programName — «Программа администратора» или «Программа дегустатора».
    static void show(QWidget* parent, const QString& programName);
};

}  // namespace sedm::ui
