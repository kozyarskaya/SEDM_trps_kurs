/**
 * @file TasterWindow.cpp
 * @brief Реализация главного окна программы дегустатора.
 */
#include "TasterWindow.h"
#include "AboutDialog.h"
#include "AppPaths.h"
#include "BasketListPage.h"
#include "Database.h"
#include "DbError.h"
#include "EvaluationPage.h"
#include "HelpWindow.h"
#include "LoginPage.h"
#include "Messages.h"
#include "ReviewPage.h"
#include "SessionService.h"
#include "Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVariant>

#include <algorithm>

namespace sedm::taster {

namespace {
const double kScales[] = {0.85, 1.0, 1.15, 1.3, 1.5};   ///< шаги размера шрифта
constexpr int kScaleCount = 5;
constexpr int kBaseFontPx = 18;                          ///< крупный шрифт дегустатора
}  // namespace

TasterWindow::TasterWindow(Database& db, QWidget* parent)
    : QMainWindow(parent),
      m_db(db),
      m_audit(db, AppPaths::pendingAuditFile()),
      m_auth(db, m_audit)
{
    setWindowTitle(QStringLiteral("СЭДМ — дегустатор"));
    setMinimumSize(1024, 700);
    try {
        m_audit.flushPending();   // записи журнала, отложенные при сбое связи
    } catch (const DbError&) {
    }

    m_stack = new QStackedWidget;
    m_loginPage = new LoginPage;
    m_basketPage = new BasketListPage;
    m_evalPage = new EvaluationPage;
    m_reviewPage = new ReviewPage;
    for (QWidget* page : {static_cast<QWidget*>(m_loginPage), static_cast<QWidget*>(m_basketPage),
                          static_cast<QWidget*>(m_evalPage), static_cast<QWidget*>(m_reviewPage)})
        m_stack->addWidget(page);
    setCentralWidget(m_stack);
    buildMenu();

    connect(m_loginPage, &LoginPage::loginRequested, this, &TasterWindow::login);
    connect(m_basketPage, &BasketListPage::basketChosen, this, &TasterWindow::openBasket);
    connect(m_basketPage, &BasketListPage::refreshRequested, this, &TasterWindow::showBaskets);
    connect(m_basketPage, &BasketListPage::reviewRequested, this, [this]() { showReview(0); });
    connect(m_basketPage, &BasketListPage::logoutRequested, this, &TasterWindow::logout);
    connect(m_evalPage, &EvaluationPage::leaveRequested, this, [this]() {
        m_evalPage->stop();
        showBaskets();
    });
    connect(m_evalPage, &EvaluationPage::basketFinished, this, [this](int number) {
        ui::Messages::info(this, QStringLiteral("Корзина завершена"),
                           QStringLiteral("Дегустация корзины № %1 завершена. Спасибо!").arg(number));
        showBaskets();
    });
    connect(m_reviewPage, &ReviewPage::backRequested, this, &TasterWindow::showBaskets);
    connect(m_reviewPage, &ReviewPage::logoutRequested, this, &TasterWindow::logout);

    const auto settings = AppPaths::settings();
    m_scaleIndex = qBound(0, settings->value(QStringLiteral("taster/scale"), 1).toInt(), kScaleCount - 1);
    changeScale(0);
    showLogin();
}

void TasterWindow::buildMenu()
{
    QMenu* file = menuBar()->addMenu(QStringLiteral("&Файл"));
    m_logoutAction = file->addAction(QStringLiteral("Выйти из учётной записи"), this, &TasterWindow::logout,
                                     QKeySequence(Qt::CTRL + Qt::Key_Q));
    m_logoutAction->setEnabled(false);
    file->addSeparator();
    file->addAction(QStringLiteral("Выход"), this, &QWidget::close, QKeySequence(Qt::ALT + Qt::Key_F4));

    QMenu* view = menuBar()->addMenu(QStringLiteral("&Вид"));
    view->addAction(QStringLiteral("Крупнее шрифт"), this, [this]() { changeScale(+1); }, QKeySequence::ZoomIn);
    view->addAction(QStringLiteral("Мельче шрифт"), this, [this]() { changeScale(-1); }, QKeySequence::ZoomOut);
    view->addAction(QStringLiteral("Обычный размер"), this, [this]() {
        m_scaleIndex = 1;
        changeScale(0);
    }, QKeySequence(Qt::CTRL + Qt::Key_0));

    QMenu* help = menuBar()->addMenu(QStringLiteral("&Справка"));
    help->addAction(QStringLiteral("Содержание справки"), this, &TasterWindow::showHelp, QKeySequence::HelpContents);
    help->addAction(QStringLiteral("О программе"), this, [this]() {
        ui::AboutDialog::show(this, QStringLiteral("Программа дегустатора"));
    });

    // Справа в строке меню: дегустатор и кнопки размера шрифта «A−»/«A+»
    auto* corner = new QWidget;
    auto* h = new QHBoxLayout(corner);
    h->setContentsMargins(0, 2, 6, 2);
    h->setSpacing(8);
    m_userLabel = new QLabel;
    m_userLabel->setProperty("role", "muted");
    auto* smaller = new QPushButton(QStringLiteral("A−"));
    smaller->setAccessibleName(QStringLiteral("Уменьшить шрифт"));
    smaller->setToolTip(QStringLiteral("Мельче шрифт (Ctrl+−)"));
    auto* bigger = new QPushButton(QStringLiteral("A+"));
    bigger->setAccessibleName(QStringLiteral("Увеличить шрифт"));
    bigger->setToolTip(QStringLiteral("Крупнее шрифт (Ctrl++)"));
    bigger->setProperty("kind", "strong");
    for (QPushButton* b : {smaller, bigger}) {
        b->setProperty("fontStep", true);
        b->setFocusPolicy(Qt::NoFocus);
    }
    h->addWidget(m_userLabel);
    h->addWidget(smaller);
    h->addWidget(bigger);
    menuBar()->setCornerWidget(corner, Qt::TopRightCorner);
    connect(smaller, &QPushButton::clicked, this, [this]() { changeScale(-1); });
    connect(bigger, &QPushButton::clicked, this, [this]() { changeScale(+1); });
}

void TasterWindow::setUserText(const QString& text)
{
    // угол строки меню не растягивается сам: ширина подписи задаётся по тексту
    m_userLabel->setText(text);
    m_userLabel->setMinimumWidth(m_userLabel->fontMetrics().horizontalAdvance(text) + 12);
    QWidget* corner = menuBar()->cornerWidget(Qt::TopRightCorner);
    corner->adjustSize();
    menuBar()->setCornerWidget(corner, Qt::TopRightCorner);
    menuBar()->updateGeometry();
}

void TasterWindow::changeScale(int step)
{
    m_scaleIndex = qBound(0, m_scaleIndex + step, kScaleCount - 1);
    ui::Theme::apply(*qApp, kBaseFontPx, kScales[m_scaleIndex],
                     ui::Theme::resourceText(QStringLiteral(":/taster/taster.qss")));
    AppPaths::settings()->setValue(QStringLiteral("taster/scale"), m_scaleIndex);
}

QString TasterWindow::sessionText()
{
    QString text;
    try {
        text = SessionService(m_db, m_audit).session().displayText();
    } catch (const DbError&) {
        text = QStringLiteral("Нет связи с сервером базы данных");
    }
    return text;
}

void TasterWindow::showLogin()
{
    m_loginPage->setSessionText(sessionText());
    m_loginPage->reset();
    m_stack->setCurrentWidget(m_loginPage);
}

void TasterWindow::login(const QString& login, const QString& pin)
{
    AuthService::TasterLogin result;
    if (!ui::Messages::guard(this, m_db, [&]() { result = m_auth.loginTaster(login, pin); },
                             QStringLiteral("Вход не выполнен")))
        return;
    switch (result.status) {
    case LoginStatus::Ok: {
        auto tasting = std::make_unique<TastingService>(m_db, m_audit, result.taster, result.sessionToken,
                                                        AppPaths::draftDir());
        bool canEnter = true;
        bool allFinished = false;
        if (!ui::Messages::guard(this, m_db, [&]() {
                canEnter = tasting->canEnter();
                allFinished = tasting->allFinished();
            }))
            return;
        if (!canEnter) {
            // все корзины завершены, повторный вход запрещён
            ui::Messages::guard(this, [&]() { m_auth.logoutTaster(result.taster.id(), result.sessionToken); });
            m_loginPage->showError(QStringLiteral("Работа завершена: дегустация всех ваших корзин окончена. "
                                                  "Спасибо!"));
            return;
        }
        m_tasting = std::move(tasting);
        m_logoutAction->setEnabled(true);
        setUserText(m_tasting->taster().fullName());
        if (allFinished)
            showReview(0);
        else
            showBaskets();
        break;
    }
    case LoginStatus::Locked:
        m_loginPage->lockFor(result.secondsLeft);
        break;
    default:
        m_loginPage->showError(QStringLiteral("Неверная фамилия или PIN-код. Проверьте и попробуйте ещё раз."));
        break;
    }
}

void TasterWindow::showBaskets()
{
    if (!m_tasting)
        return;
    QVector<TasterBasketRow> rows;
    std::optional<Team> team;
    if (!ui::Messages::guard(this, m_db, [&]() {
            rows = m_tasting->baskets();
            team = m_tasting->team();
        }))
        return;
    m_basketPage->setHeader(team ? team->displayName() : QString(), m_tasting->taster().fullName(), sessionText());
    m_basketPage->setBaskets(rows);
    m_logoutAction->setEnabled(true);
    m_stack->setCurrentWidget(m_basketPage);
}

void TasterWindow::openBasket(int basketId, bool finished)
{
    if (finished) {
        showReview(basketId);
        return;
    }
    BasketSheet sheet;
    if (!ui::Messages::guard(this, m_db, [&]() { sheet = m_tasting->openBasket(basketId); }))
        return;
    if (sheet.readOnly()) {   // корзину успели завершить на другом компьютере
        showReview(basketId);
        return;
    }
    m_stack->setCurrentWidget(m_evalPage);
    m_logoutAction->setEnabled(false);   // выход — только при закрытой корзине
    m_evalPage->open(m_tasting.get(), &m_db, sheet);
}

void TasterWindow::showReview(int basketId)
{
    QVector<TasterBasketRow> rows;
    if (!ui::Messages::guard(this, m_db, [&]() { rows = m_tasting->baskets(); }))
        return;
    const bool unfinished = std::any_of(rows.begin(), rows.end(), [](const TasterBasketRow& r) { return !r.finished; });
    m_reviewPage->open(m_tasting.get(), &m_db, rows, basketId, unfinished);
    m_logoutAction->setEnabled(true);
    m_stack->setCurrentWidget(m_reviewPage);
}

void TasterWindow::logout()
{
    if (m_stack->currentWidget() == m_evalPage) {
        if (!m_evalPage->confirmLeave())
            return;
        m_evalPage->stop();
    }
    if (m_tasting) {
        const int id = m_tasting->taster().id();
        const QString token = m_tasting->sessionToken();
        ui::Messages::guard(this, m_db, [&]() { m_auth.logoutTaster(id, token); });
        m_tasting.reset();
    }
    setUserText(QString());
    m_logoutAction->setEnabled(false);
    showLogin();
}

void TasterWindow::showHelp()
{
    QString topic = QStringLiteral("index.html");
    if (m_stack->currentWidget() == m_loginPage)
        topic = QStringLiteral("login.html");
    else if (m_stack->currentWidget() == m_basketPage)
        topic = QStringLiteral("baskets.html");
    else if (m_stack->currentWidget() == m_evalPage)
        topic = QStringLiteral("evaluation.html");
    else if (m_stack->currentWidget() == m_reviewPage)
        topic = QStringLiteral("review.html");
    ui::HelpWindow::showTopic(this, topic);
}

void TasterWindow::closeEvent(QCloseEvent* event)
{
    if (m_stack->currentWidget() == m_evalPage) {
        if (!m_evalPage->confirmLeave()) {
            event->ignore();
            return;
        }
        m_evalPage->stop();
    }
    if (m_tasting) {
        try {
            m_auth.logoutTaster(m_tasting->taster().id(), m_tasting->sessionToken());
        } catch (const std::exception&) {
            // сервер недоступен — сеанс закроется при следующем входе
        }
        m_tasting.reset();
    }
    event->accept();
}

}  // namespace sedm::taster
