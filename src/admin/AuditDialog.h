/**
 * @file AuditDialog.h
 * @brief Окно «Журнал действий» с поиском и выгрузкой в CSV.
 */
#pragma once

#include <QDialog>

class QLineEdit;
class QStandardItemModel;
class QTableView;

namespace sedm {
class AuditService;
class AuthService;
}

namespace sedm::admin {

class RecordFilter;

/// Журнал действий пользователей (п. 4.3.4–4.3.5 ТЗ): время, пользователь,
/// роль, рабочее место, действие, подробности. Выгрузка — в файл CSV (UTF-8).
class AuditDialog : public QDialog {
    Q_OBJECT
public:
    static constexpr int kShownEntries = 5000;   ///< показывается последних записей

    explicit AuditDialog(AuditService& audit, QWidget* parent = nullptr);
    /// Выгрузка журнала в CSV с выбором файла (также из меню «Сервис»).
    static void exportCsv(AuditService& audit, QWidget* parent);

private:
    void load();

    AuditService& m_audit;
    QLineEdit* m_search;
    QStandardItemModel* m_model;
    RecordFilter* m_filter;
    QTableView* m_table;
};

/// Смена пароля администратора (меню «Файл»).
class ChangePasswordDialog : public QDialog {
    Q_OBJECT
public:
    ChangePasswordDialog(AuthService& auth, QWidget* parent = nullptr);

private:
    void accept() override;

    AuthService& m_auth;
    QLineEdit* m_current;
    QLineEdit* m_new;
    QLineEdit* m_repeat;
};

}  // namespace sedm::admin
