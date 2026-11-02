/**
 * @file ParticipantDialog.cpp
 * @brief Реализация окна записи участника.
 */
#include "ParticipantDialog.h"
#include "Messages.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

const QStringList& russianRegions()
{
    static const QStringList regions = {
        QStringLiteral("Республика Адыгея"), QStringLiteral("Республика Алтай"), QStringLiteral("Республика Башкортостан"),
        QStringLiteral("Республика Бурятия"), QStringLiteral("Республика Дагестан"), QStringLiteral("Донецкая Народная Республика"),
        QStringLiteral("Республика Ингушетия"), QStringLiteral("Кабардино-Балкарская Республика"), QStringLiteral("Республика Калмыкия"),
        QStringLiteral("Карачаево-Черкесская Республика"), QStringLiteral("Республика Карелия"), QStringLiteral("Республика Коми"),
        QStringLiteral("Республика Крым"), QStringLiteral("Луганская Народная Республика"), QStringLiteral("Республика Марий Эл"),
        QStringLiteral("Республика Мордовия"), QStringLiteral("Республика Саха (Якутия)"), QStringLiteral("Республика Северная Осетия — Алания"),
        QStringLiteral("Республика Татарстан"), QStringLiteral("Республика Тыва"), QStringLiteral("Удмуртская Республика"),
        QStringLiteral("Республика Хакасия"), QStringLiteral("Чеченская Республика"), QStringLiteral("Чувашская Республика"),
        QStringLiteral("Алтайский край"), QStringLiteral("Забайкальский край"), QStringLiteral("Камчатский край"),
        QStringLiteral("Краснодарский край"), QStringLiteral("Красноярский край"), QStringLiteral("Пермский край"),
        QStringLiteral("Приморский край"), QStringLiteral("Ставропольский край"), QStringLiteral("Хабаровский край"),
        QStringLiteral("Амурская область"), QStringLiteral("Архангельская область"), QStringLiteral("Астраханская область"),
        QStringLiteral("Белгородская область"), QStringLiteral("Брянская область"), QStringLiteral("Владимирская область"),
        QStringLiteral("Волгоградская область"), QStringLiteral("Вологодская область"), QStringLiteral("Воронежская область"),
        QStringLiteral("Запорожская область"), QStringLiteral("Ивановская область"), QStringLiteral("Иркутская область"),
        QStringLiteral("Калининградская область"), QStringLiteral("Калужская область"), QStringLiteral("Кемеровская область"),
        QStringLiteral("Кировская область"), QStringLiteral("Костромская область"), QStringLiteral("Курганская область"),
        QStringLiteral("Курская область"), QStringLiteral("Ленинградская область"), QStringLiteral("Липецкая область"),
        QStringLiteral("Магаданская область"), QStringLiteral("Московская область"), QStringLiteral("Мурманская область"),
        QStringLiteral("Нижегородская область"), QStringLiteral("Новгородская область"), QStringLiteral("Новосибирская область"),
        QStringLiteral("Омская область"), QStringLiteral("Оренбургская область"), QStringLiteral("Орловская область"),
        QStringLiteral("Пензенская область"), QStringLiteral("Псковская область"), QStringLiteral("Ростовская область"),
        QStringLiteral("Рязанская область"), QStringLiteral("Самарская область"), QStringLiteral("Саратовская область"),
        QStringLiteral("Сахалинская область"), QStringLiteral("Свердловская область"), QStringLiteral("Смоленская область"),
        QStringLiteral("Тамбовская область"), QStringLiteral("Тверская область"), QStringLiteral("Томская область"),
        QStringLiteral("Тульская область"), QStringLiteral("Тюменская область"), QStringLiteral("Ульяновская область"),
        QStringLiteral("Херсонская область"), QStringLiteral("Челябинская область"), QStringLiteral("Ярославская область"),
        QStringLiteral("Москва"), QStringLiteral("Санкт-Петербург"), QStringLiteral("Севастополь"),
        QStringLiteral("Еврейская автономная область"), QStringLiteral("Ненецкий автономный округ"),
        QStringLiteral("Ханты-Мансийский автономный округ — Югра"), QStringLiteral("Чукотский автономный округ"),
        QStringLiteral("Ямало-Ненецкий автономный округ")};
    return regions;
}

ParticipantDialog::ParticipantDialog(const Participant& participant, const QVector<Nomination>& nominations,
                                     std::function<void(const Participant&)> save, QWidget* parent)
    : QDialog(parent), m_participant(participant), m_save(std::move(save))
{
    setWindowTitle(participant.id() > 0 ? QStringLiteral("Изменение данных участника")
                                        : QStringLiteral("Новый участник"));
    setMinimumWidth(560);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    m_name = new QLineEdit(participant.fullName());
    m_name->setPlaceholderText(QStringLiteral("Фамилия Имя Отчество"));
    m_phone = new QLineEdit(participant.phone());
    m_phone->setPlaceholderText(QStringLiteral("+7 912 345-67-89"));
    m_email = new QLineEdit(participant.email());
    m_email->setPlaceholderText(QStringLiteral("имя@домен.ру (необязательно)"));
    m_region = new QComboBox;
    m_region->setEditable(true);
    m_region->addItems(russianRegions());
    m_region->setCurrentText(participant.region());
    m_region->completer()->setCompletionMode(QCompleter::PopupCompletion);
    m_region->completer()->setFilterMode(Qt::MatchContains);
    m_region->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    m_nomination = new QComboBox;
    for (const Nomination& n : nominations) {
        m_nomination->addItem(n.name(), n.id());
        if (n.id() == participant.nominationId())
            m_nomination->setCurrentIndex(m_nomination->count() - 1);
    }
    form->addRow(QStringLiteral("ФИО участника:"), m_name);
    form->addRow(QStringLiteral("Телефон:"), m_phone);
    form->addRow(QStringLiteral("E-mail:"), m_email);
    form->addRow(QStringLiteral("Регион:"), m_region);
    form->addRow(QStringLiteral("Номинация:"), m_nomination);
    layout->addLayout(form);
    if (nominations.isEmpty()) {
        auto* hint = new QLabel(QStringLiteral("Справочник номинаций пуст — сначала добавьте номинацию."));
        hint->setProperty("role", "warn");
        layout->addWidget(hint);
    }

    auto* buttons = new QDialogButtonBox;
    auto* ok = buttons->addButton(QStringLiteral("Сохранить"), QDialogButtonBox::AcceptRole);
    ok->setProperty("kind", "primary");
    ok->setDefault(true);
    buttons->addButton(QStringLiteral("Отмена"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ParticipantDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

Participant ParticipantDialog::collect() const
{
    Participant p = m_participant;
    p.setFullName(m_name->text());
    p.setPhone(m_phone->text());
    p.setEmail(m_email->text());
    p.setRegion(m_region->currentText());
    p.setNominationId(m_nomination->currentData().toInt());
    return p;
}

void ParticipantDialog::accept()
{
    const Participant p = collect();
    if (ui::Messages::guard(this, [&]() { m_save(p); }, QStringLiteral("Запись не сохранена")))
        QDialog::accept();
}

}  // namespace sedm::admin
