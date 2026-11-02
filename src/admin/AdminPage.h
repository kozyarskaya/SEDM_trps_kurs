/**
 * @file AdminPage.h
 * @brief Общая основа страниц программы администратора (заголовок, действия, данные).
 */
#pragma once

#include <QWidget>
#include <functional>

class QAbstractItemView;
class QStandardItem;
class QVariant;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QTableView;
class QVBoxLayout;

namespace sedm {
class AuditService;
class Database;
}

namespace sedm::admin {

/// Подключение к базе данных и журнал — общие для всех окон администратора.
struct AdminContext {
    Database& db;
    AuditService& audit;
};

/// Страница главного окна (макет Б1): заголовок, подзаголовок со сводкой,
/// кнопки действий справа и область содержимого.
class AdminPage : public QWidget {
    Q_OBJECT
public:
    AdminPage(AdminContext& ctx, const QString& title, QWidget* parent = nullptr);

    /// Перечитать данные из базы данных (при открытии страницы и после изменений).
    virtual void refresh() = 0;
    /// Раздел справки об этой странице.
    virtual QString helpTopic() const = 0;
    const QString& title() const { return m_title; }

signals:
    /// Данные изменились — главное окно обновляет счётчики боковой панели.
    void dataChanged();

protected:
    QVBoxLayout* body() const { return m_body; }
    void setSubtitle(const QString& text);
    /// Кнопка действия в заголовке страницы; primary — основная (медовая).
    QPushButton* addAction(const QString& text, bool primary = false, const QString& icon = QString());
    /// Выполнить действие с понятным сообщением об ошибке; после успеха — dataChanged.
    bool run(const std::function<void()>& action, bool changesData = true);

    AdminContext& m_ctx;

private:
    QString m_title;
    QLabel* m_subtitle;
    QHBoxLayout* m_actions;
    QVBoxLayout* m_body;
};

/// Единый вид таблиц: выделение строк, без редактирования, чередование фона.
void setupTable(QAbstractItemView* view);
/// Ячейка таблицы; sortValue — значение для сортировки (по умолчанию — текст).
QStandardItem* makeItem(const QString& text, const QVariant& sortValue);
QStandardItem* makeItem(const QString& text);

}  // namespace sedm::admin
