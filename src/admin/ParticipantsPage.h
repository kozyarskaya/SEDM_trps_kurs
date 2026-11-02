/**
 * @file ParticipantsPage.h
 * @brief Окно «Участники и номинации»: справочник номинаций и реестр участников.
 */
#pragma once

#include "AdminPage.h"

class QLineEdit;
class QListWidget;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;

namespace sedm::admin {

class RecordFilter;

/// Слева — справочник номинаций (выбор номинации отбирает участников), справа —
/// реестр участников с поиском; добавление, изменение, удаление и загрузка
/// реестра из файлов CSV и XLSX (п. 4.3.1.1 ТЗ).
class ParticipantsPage : public AdminPage {
    Q_OBJECT
public:
    explicit ParticipantsPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("participants.html"); }

    void focusNominations();   ///< меню «Данные → Номинации»
    void importRegistry();     ///< меню «Данные → Импорт реестра»

private:
    void addParticipant();
    void editParticipant();
    void removeParticipant();
    void addNomination();
    void renameNomination();
    void removeNomination();
    int selectedParticipantId() const;
    int selectedNominationId() const;

    QListWidget* m_nominations;
    QLineEdit* m_search;
    QStandardItemModel* m_model;
    RecordFilter* m_filter;
    QTableView* m_table;
};

}  // namespace sedm::admin
