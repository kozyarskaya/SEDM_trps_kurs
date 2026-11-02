/**
 * @file LoginGenerator.h
 * @brief Формирование логинов дегустаторов.
 */
#pragma once

#include <QSet>
#include <QString>

namespace sedm {

/// Логин дегустатора — его фамилия кириллицей (решение заказчика В9): «Иванова».
/// При совпадении фамилий добавляется номер: «Иванова2», «Иванова3».
/// Сравнение логинов при входе не зависит от регистра букв.
class LoginGenerator {
public:
    /// Логин для дегустатора с ФИО fullName; existing — уже занятые логины.
    static QString makeLogin(const QString& fullName, const QSet<QString>& existing);
    /// Приводит введённый логин к виду для поиска (без пробелов, строчные буквы).
    static QString normalizedForLookup(const QString& login);
};

}  // namespace sedm
