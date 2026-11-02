/**
 * @file TeamsPage.h
 * @brief Окно «Команды и дегустаторы»: команды, учётные записи, назначение корзин.
 */
#pragma once

#include "AdminPage.h"
#include "Basket.h"
#include "Taster.h"
#include "Team.h"

#include <QHash>

class QGroupBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace sedm::admin {

/// Команды (п. 4.3.1.1 ТЗ): номер, название, исходный перечень фамилий (может
/// быть пустым); дегустаторам выдаются логины (фамилия) и PIN-коды; каждой
/// команде назначается одна или несколько корзин.
class TeamsPage : public AdminPage {
    Q_OBJECT
public:
    explicit TeamsPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("teams.html"); }

    /// Меню «Дегустация → Карточки дегустаторов»: новые PIN-коды и печать карточек.
    void issueCards();

private:
    void createTeam();
    void showTeam();
    void saveTeam();
    void removeTeam();
    void addTaster();
    void renameTaster();
    void removeTaster();
    void resetPin();
    void assignBaskets();
    void unassignBasket();
    int selectedTeamId() const;
    int selectedTasterId() const;
    QString sessionText() const;

    QVector<Team> m_teams;
    QVector<Taster> m_tasters;
    QVector<Basket> m_baskets;
    QHash<int, QString> m_nominations;

    QTableWidget* m_list;
    QGroupBox* m_editor;
    QSpinBox* m_number;
    QLineEdit* m_name;
    QTableWidget* m_tasterTable;
    QTableWidget* m_basketTable;
};

}  // namespace sedm::admin
