/**
 * @file TasterWindow.h
 * @brief Главное окно программы дегустатора.
 */
#pragma once

#include "AuditService.h"
#include "AuthService.h"
#include "TastingService.h"

#include <QMainWindow>
#include <memory>

class QAction;
class QLabel;
class QStackedWidget;

namespace sedm {
class Database;
}

namespace sedm::taster {

class BasketListPage;
class EvaluationPage;
class LoginPage;
class ReviewPage;

/// Главное окно дегустатора: переключает окна по диаграмме состояний интерфейса
/// дегустатора (вход → выбор корзины → оценивание / просмотр оценок), содержит
/// меню «Файл», «Вид», «Справка» (структурная схема меню, рис. «Меню дегустатора»)
/// и кнопки размера шрифта «A−»/«A+» для возрастных пользователей (п. 4.3.2 ТЗ).
class TasterWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit TasterWindow(Database& db, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenu();
    void login(const QString& login, const QString& pin);
    void showLogin();
    void showBaskets();
    void openBasket(int basketId, bool finished);
    void showReview(int basketId);
    void logout();
    void changeScale(int step);
    void setUserText(const QString& text);
    void showHelp();
    QString sessionText();

    Database& m_db;
    AuditService m_audit;
    AuthService m_auth;
    std::unique_ptr<TastingService> m_tasting;   ///< после входа дегустатора

    QStackedWidget* m_stack;
    LoginPage* m_loginPage;
    BasketListPage* m_basketPage;
    EvaluationPage* m_evalPage;
    ReviewPage* m_reviewPage;
    QAction* m_logoutAction = nullptr;
    QLabel* m_userLabel = nullptr;
    int m_scaleIndex = 1;
};

}  // namespace sedm::taster
