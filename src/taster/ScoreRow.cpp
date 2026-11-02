/**
 * @file ScoreRow.cpp
 * @brief Реализация строки ввода оценки.
 */
#include "ScoreRow.h"
#include "HoneySlider.h"

#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVariant>

namespace sedm::taster {

ScoreRow::ScoreRow(Criterion criterion, QWidget* parent)
    : QFrame(parent), m_criterion(criterion)
{
    setProperty("card", "row");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(14);

    const QString title = criterionTitle(criterion);
    m_title = new QLabel(title);
    m_title->setProperty("role", "criterion");
    m_title->setMinimumWidth(190);
    m_slider = new ui::HoneySlider;
    m_slider->setAccessibleName(QStringLiteral("Оценка «%1»").arg(title));
    m_minus = new QPushButton(QStringLiteral("−"));
    m_minus->setProperty("step", true);
    m_minus->setAutoRepeat(true);
    m_minus->setAccessibleName(QStringLiteral("Уменьшить оценку «%1»").arg(title));
    m_edit = new QLineEdit;
    m_edit->setProperty("score", true);
    m_edit->setValidator(new QIntValidator(kMinScore, kMaxScore, m_edit));
    m_edit->setMaxLength(3);
    m_edit->setPlaceholderText(QStringLiteral("—"));
    m_edit->setAccessibleName(QStringLiteral("Оценка «%1» числом").arg(title));
    m_plus = new QPushButton(QStringLiteral("+"));
    m_plus->setProperty("step", true);
    m_plus->setAutoRepeat(true);
    m_plus->setAccessibleName(QStringLiteral("Увеличить оценку «%1»").arg(title));
    m_title->setBuddy(m_slider);

    layout->addWidget(m_title);
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_minus);
    layout->addWidget(m_edit);
    layout->addWidget(m_plus);

    connect(m_slider, &ui::HoneySlider::valueChanged, this, &ScoreRow::userSet);
    connect(m_minus, &QPushButton::clicked, this, [this]() { userSet(m_value.value_or(kMinScore + 1) - 1); });
    connect(m_plus, &QPushButton::clicked, this, [this]() { userSet(m_value ? *m_value + 1 : kMinScore + 1); });
    connect(m_edit, &QLineEdit::textEdited, this, [this](const QString& text) {
        bool ok = false;
        const int v = text.toInt(&ok);
        if (ok && isValidScore(v))
            userSet(v);
    });
    connect(m_edit, &QLineEdit::editingFinished, this, &ScoreRow::showValue);   // вернуть верное число
}

void ScoreRow::showValue()
{
    const QString text = m_value ? QString::number(*m_value) : QString();
    if (m_edit->text() != text)
        m_edit->setText(text);
    m_slider->setValue(m_value);
    m_minus->setEnabled(!m_slider->isReadOnly() && m_value.value_or(kMinScore) > kMinScore);
    m_plus->setEnabled(!m_slider->isReadOnly() && m_value.value_or(kMinScore) < kMaxScore);
}

void ScoreRow::setValue(std::optional<int> value)
{
    m_value = value;
    showValue();
}

void ScoreRow::userSet(int value)
{
    value = qBound(kMinScore, value, kMaxScore);
    const bool changed = m_value != value;
    m_value = value;
    // поле ввода не перезаписывается, пока пользователь набирает число
    if (!m_edit->hasFocus())
        m_edit->setText(QString::number(value));
    m_slider->setValue(m_value);
    m_minus->setEnabled(value > kMinScore);
    m_plus->setEnabled(value < kMaxScore);
    if (changed)
        emit valueChanged(m_criterion, value);
}

void ScoreRow::setReadOnly(bool readOnly)
{
    m_slider->setReadOnly(readOnly);
    m_edit->setReadOnly(readOnly);
    m_minus->setVisible(!readOnly);
    m_plus->setVisible(!readOnly);
    showValue();
}

}  // namespace sedm::taster
