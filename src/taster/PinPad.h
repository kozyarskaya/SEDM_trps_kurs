/**
 * @file PinPad.h
 * @brief Экранная цифровая клавиатура для ввода PIN-кода.
 */
#pragma once

#include <QWidget>

namespace sedm::taster {

/// Крупные кнопки 0–9, «⌫» (стереть цифру) и «Очистить» — удобно на сенсорном
/// экране и для возрастных пользователей (п. 4.3.2 ТЗ). Ввод с клавиатуры
/// компьютера обрабатывает окно входа.
class PinPad : public QWidget {
    Q_OBJECT
public:
    explicit PinPad(QWidget* parent = nullptr);

signals:
    void digitPressed(QChar digit);
    void backspacePressed();
    void clearPressed();
};

}  // namespace sedm::taster
