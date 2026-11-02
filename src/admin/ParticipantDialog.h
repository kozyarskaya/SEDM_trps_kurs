/**
 * @file ParticipantDialog.h
 * @brief Окно добавления и изменения записи участника.
 */
#pragma once

#include "Nomination.h"
#include "Participant.h"

#include <QDialog>
#include <QVector>
#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;

namespace sedm::admin {

/// Карточка участника: ФИО, телефон, e-mail, регион (с подсказкой списком
/// субъектов РФ), номинация. Запись сохраняется функцией save; если она
/// сообщает об ошибке (DomainError), окно остаётся открытым.
class ParticipantDialog : public QDialog {
    Q_OBJECT
public:
    ParticipantDialog(const Participant& participant, const QVector<Nomination>& nominations,
                      std::function<void(const Participant&)> save, QWidget* parent = nullptr);

private:
    void accept() override;
    Participant collect() const;

    Participant m_participant;
    std::function<void(const Participant&)> m_save;
    QLineEdit* m_name;
    QLineEdit* m_phone;
    QLineEdit* m_email;
    QComboBox* m_region;
    QComboBox* m_nomination;
};

/// Субъекты Российской Федерации — подсказка при вводе региона.
const QStringList& russianRegions();

}  // namespace sedm::admin
