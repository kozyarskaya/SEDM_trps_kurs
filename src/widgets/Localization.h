/**
 * @file Localization.h
 * @brief Русский язык стандартных окон Qt и русские форматы дат и чисел.
 */
#pragma once

class QApplication;

namespace sedm::ui {

/// Интерфейс на русском языке (п. 4.3.2, 4.6.2 ТЗ) независимо от языка Windows:
/// русские форматы дат и чисел (QLocale) и перевод стандартных окон Qt —
/// кнопки «ОК»/«Отмена», окно предварительного просмотра печати (qtbase_ru.qm).
void installRussian(QApplication& app);

}  // namespace sedm::ui
