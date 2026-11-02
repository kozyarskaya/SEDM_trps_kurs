/**
 * @file PinPad.cpp
 * @brief Реализация цифровой клавиатуры.
 */
#include "PinPad.h"

#include <QGridLayout>
#include <QPushButton>
#include <QVariant>

namespace sedm::taster {

PinPad::PinPad(QWidget* parent)
    : QWidget(parent)
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    auto makeButton = [this](const QString& text, const QString& accessible) {
        auto* b = new QPushButton(text, this);
        b->setProperty("pin", true);
        b->setFocusPolicy(Qt::NoFocus);   // фокус остаётся в поле ввода
        b->setAccessibleName(accessible);
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        return b;
    };
    // 1 2 3 / 4 5 6 / 7 8 9 / Очистить 0 ⌫
    for (int d = 1; d <= 9; ++d) {
        auto* b = makeButton(QString::number(d), QStringLiteral("Цифра %1").arg(d));
        connect(b, &QPushButton::clicked, this, [this, d]() { emit digitPressed(QChar('0' + d)); });
        grid->addWidget(b, (d - 1) / 3, (d - 1) % 3);
    }
    auto* clear = makeButton(QStringLiteral("Очистить"), QStringLiteral("Очистить PIN-код"));
    clear->setProperty("pinAux", true);
    connect(clear, &QPushButton::clicked, this, &PinPad::clearPressed);
    grid->addWidget(clear, 3, 0);
    auto* zero = makeButton(QStringLiteral("0"), QStringLiteral("Цифра 0"));
    connect(zero, &QPushButton::clicked, this, [this]() { emit digitPressed(QLatin1Char('0')); });
    grid->addWidget(zero, 3, 1);
    auto* back = makeButton(QStringLiteral("⌫"), QStringLiteral("Стереть последнюю цифру"));
    back->setProperty("pinBack", true);
    connect(back, &QPushButton::clicked, this, &PinPad::backspacePressed);
    grid->addWidget(back, 3, 2);
}

}  // namespace sedm::taster
