/**
 * @file AdminWindow.cpp
 * @brief Реализация главного окна администратора.
 */
#include "AdminWindow.h"
#include "AboutDialog.h"
#include "AppPaths.h"
#include "AuditDialog.h"
#include "AutoBackupScheduler.h"
#include "BackupPage.h"
#include "BackupService.h"
#include "BasketsPage.h"
#include "ConnectionDialog.h"
#include "Database.h"
#include "DbError.h"
#include "HelpWindow.h"
#include "Icons.h"
#include "LogoWidget.h"
#include "Messages.h"
#include "MonitoringPage.h"
#include "MonitoringService.h"
#include "ParticipantsPage.h"
#include "ResultsPage.h"
#include "SamplesPage.h"
#include "SessionPage.h"
#include "SessionService.h"
#include "TeamsPage.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

namespace {
enum CountKey { kParticipants, kNominations, kSamples, kBaskets, kTeams };
constexpr int kPingMs = 30000;   ///< проверка связи с сервером
}  // namespace

AdminWindow::AdminWindow(Database& db, AuditService& audit, QWidget* parent)
    : QMainWindow(parent), m_db(db), m_audit(audit), m_auth(db, audit), m_ctx{db, audit}
{
    setWindowTitle(QStringLiteral("СЭДМ — администратор"));
    setMinimumSize(1180, 720);

    const auto settings = AppPaths::settings();
    m_scheduler = new AutoBackupScheduler(db.settings(), AppPaths::autoBackupDir(), this);
    m_scheduler->setPgBinDir(settings->value(QStringLiteral("tools/pgBinDir")).toString());

    m_participants = new ParticipantsPage(m_ctx);
    m_samples = new SamplesPage(m_ctx);
    m_baskets = new BasketsPage(m_ctx);
    m_teams = new TeamsPage(m_ctx);
    m_session = new SessionPage(m_ctx);
    m_monitoring = new MonitoringPage(m_ctx);
    m_results = new ResultsPage(m_ctx);
    m_backup = new BackupPage(m_ctx, *m_scheduler);

    auto* central = new QWidget;
    auto* h = new QHBoxLayout(central);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    m_stack = new QStackedWidget;
    for (AdminPage* page : std::initializer_list<AdminPage*>{m_participants, m_samples, m_baskets, m_teams, m_session,
                                                            m_monitoring, m_results, m_backup}) {
        m_stack->addWidget(page);
        connect(page, &AdminPage::dataChanged, this, &AdminWindow::updateSummary);
    }
    h->addWidget(buildSidebar());
    h->addWidget(m_stack, 1);
    setCentralWidget(central);
    buildMenu();

    // Строка состояния: подключение, автокопия, журнал
    m_connection = new QLabel;
    m_backupStatus = new QLabel(QStringLiteral("Автокопия БД: ещё не создавалась"));
    m_auditStatus = new QLabel;
    statusBar()->addWidget(m_connection);
    statusBar()->addWidget(m_backupStatus);
    statusBar()->addWidget(m_auditStatus);
    connect(m_scheduler, &AutoBackupScheduler::finished, this, [this](bool created, const QString& message) {
        if (created)
            m_backupStatus->setText(QStringLiteral("Автокопия БД: %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))));
        else if (message != QLatin1String("данные не менялись"))
            m_backupStatus->setText(QStringLiteral("Автокопия не создана: %1").arg(message));
    });
    connect(m_backup, &BackupPage::databaseRestored, this, &AdminWindow::refreshAll);
    m_pingTimer.setInterval(kPingMs);
    connect(&m_pingTimer, &QTimer::timeout, this, [this]() {
        updateConnection();
        updateSummary();
    });
    m_pingTimer.start();
    m_scheduler->start();

    updateConnection();
    updateSummary();
    m_nav.first().button->setChecked(true);
    showPage(m_participants);
}

AdminWindow::~AdminWindow() = default;

QWidget* AdminWindow::buildSidebar()
{
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(250);
    auto* v = new QVBoxLayout(sidebar);
    v->setContentsMargins(12, 16, 12, 16);
    v->setSpacing(4);

    auto* brand = new QHBoxLayout;
    brand->setContentsMargins(8, 4, 8, 16);
    brand->setSpacing(10);
    brand->addWidget(new ui::LogoWidget(true, 43));
    auto* names = new QVBoxLayout;
    names->setSpacing(0);
    auto* title = new QLabel(QStringLiteral("СЭДМ"));
    QFont f = title->font();
    f.setBold(true);
    f.setPixelSize(16);
    title->setFont(f);
    m_sessionLabel = new QLabel;
    m_sessionLabel->setProperty("role", "muted");
    m_sessionLabel->setWordWrap(true);
    names->addWidget(title);
    names->addWidget(m_sessionLabel);
    brand->addLayout(names, 1);
    v->addLayout(brand);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    auto addTo = [this, v](const QString& text, const QString& icon, AdminPage* page, int countKey) {
        addNav(text, icon, page, countKey);
        v->addWidget(m_nav.last().button);
    };
    addTo(QStringLiteral("Участники"), QStringLiteral("participants"), m_participants, kParticipants);
    addTo(QStringLiteral("Номинации"), QStringLiteral("nominations"), m_participants, kNominations);
    addTo(QStringLiteral("Образцы"), QStringLiteral("samples"), m_samples, kSamples);
    addTo(QStringLiteral("Корзины"), QStringLiteral("baskets"), m_baskets, kBaskets);
    addTo(QStringLiteral("Команды"), QStringLiteral("teams"), m_teams, kTeams);
    addTo(QStringLiteral("Сеанс дегустации"), QStringLiteral("session"), m_session, -1);
    addTo(QStringLiteral("Мониторинг"), QStringLiteral("monitoring"), m_monitoring, -1);
    addTo(QStringLiteral("Итоги и ведомость"), QStringLiteral("results"), m_results, -1);
    addTo(QStringLiteral("Резервные копии"), QStringLiteral("backup"), m_backup, -1);
    v->addStretch(1);

    auto* prep = new QFrame;
    prep->setObjectName(QStringLiteral("prepCard"));
    auto* p = new QVBoxLayout(prep);
    p->setContentsMargins(12, 12, 12, 12);
    p->setSpacing(8);
    auto* prepTitle = new QLabel(QStringLiteral("Подготовка дегустации"));
    prepTitle->setProperty("role", "muted");
    m_prepBar = new QProgressBar;
    m_prepBar->setObjectName(QStringLiteral("prepBar"));
    m_prepBar->setRange(0, ProjectSummary::kStages);
    m_prepText = new QLabel;
    p->addWidget(prepTitle);
    p->addWidget(m_prepBar);
    p->addWidget(m_prepText);
    prep->setCursor(Qt::PointingHandCursor);
    prep->setToolTip(QStringLiteral("Этапы подготовки — в окне «Сеанс дегустации»"));
    v->addWidget(prep);
    return sidebar;
}

void AdminWindow::addNav(const QString& text, const QString& icon, AdminPage* page, int countKey)
{
    auto* button = new QPushButton;
    button->setProperty("nav", true);
    button->setCheckable(true);
    button->setAccessibleName(text);
    auto* h = new QHBoxLayout(button);
    h->setContentsMargins(10, 0, 10, 0);
    h->setSpacing(10);
    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(ui::Icons::pixmap(icon, ui::palette::kOnDark, 18, devicePixelRatioF()));
    auto* textLabel = new QLabel(text);
    auto* count = new QLabel;
    count->setProperty("role", "muted");
    for (QLabel* l : {iconLabel, textLabel, count})
        l->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(iconLabel);
    h->addWidget(textLabel, 1);
    h->addWidget(count);
    button->setMinimumHeight(44);
    m_navGroup->addButton(button);
    m_nav.append({button, page, count, countKey});
    // активный пункт: тёмный текст и значок на медовом фоне
    connect(button, &QPushButton::toggled, this, [this, icon, iconLabel, textLabel, count](bool on) {
        const QColor color = on ? QColor(0x24, 0x1A, 0x0E) : ui::palette::kOnDark;
        iconLabel->setPixmap(ui::Icons::pixmap(icon, color, 18, devicePixelRatioF()));
        const QString style = on ? QStringLiteral("color: #241A0E; font-weight: bold;") : QString();
        textLabel->setStyleSheet(style);
        count->setStyleSheet(on ? QStringLiteral("color: #241A0E;") : QString());
    });
    connect(button, &QPushButton::clicked, this, [this, page, countKey]() {
        showPage(page);
        if (countKey == kNominations)
            m_participants->focusNominations();
    });
}

void AdminWindow::showPage(AdminPage* page)
{
    m_stack->setCurrentWidget(page);
    page->refresh();
}

void AdminWindow::buildMenu()
{
    auto go = [this](int navIndex) {
        return [this, navIndex]() {
            m_nav[navIndex].button->setChecked(true);
            m_nav[navIndex].button->click();
        };
    };
    QMenu* file = menuBar()->addMenu(QStringLiteral("&Файл"));
    file->addAction(QStringLiteral("Новый проект…"), this, &AdminWindow::newProject, QKeySequence(Qt::CTRL + Qt::Key_N));
    file->addSeparator();
    file->addAction(QStringLiteral("Подключение к БД…"), this, &AdminWindow::connectionSettings);
    file->addAction(QStringLiteral("Сменить пароль администратора…"), this, &AdminWindow::changePassword);
    file->addSeparator();
    file->addAction(QStringLiteral("Выход"), this, &QWidget::close, QKeySequence(Qt::ALT + Qt::Key_F4));

    QMenu* data = menuBar()->addMenu(QStringLiteral("&Данные"));
    data->addAction(QStringLiteral("Участники"), this, go(0), QKeySequence(Qt::CTRL + Qt::Key_1));
    data->addAction(QStringLiteral("Импорт реестра (CSV, XLSX)…"), this, [this, go]() {
        go(0)();
        m_participants->importRegistry();
    }, QKeySequence(Qt::CTRL + Qt::Key_I));
    data->addAction(QStringLiteral("Номинации"), this, go(1), QKeySequence(Qt::CTRL + Qt::Key_2));
    data->addAction(QStringLiteral("Образцы"), this, go(2), QKeySequence(Qt::CTRL + Qt::Key_3));
    data->addAction(QStringLiteral("Корзины"), this, go(3), QKeySequence(Qt::CTRL + Qt::Key_4));
    data->addAction(QStringLiteral("Команды и дегустаторы"), this, go(4), QKeySequence(Qt::CTRL + Qt::Key_5));

    QMenu* tasting = menuBar()->addMenu(QStringLiteral("Д&егустация"));
    tasting->addAction(QStringLiteral("Сеанс дегустации"), this, go(5), QKeySequence(Qt::CTRL + Qt::Key_6));
    tasting->addAction(QStringLiteral("Мониторинг"), this, go(6), QKeySequence(Qt::CTRL + Qt::Key_7));
    tasting->addAction(QStringLiteral("Итоги и ведомость"), this, go(7), QKeySequence(Qt::CTRL + Qt::Key_8));
    tasting->addSeparator();
    tasting->addAction(QStringLiteral("Экспорт ведомости XLSX…"), this, [this]() { m_results->exportXlsx(); },
                       QKeySequence(Qt::CTRL + Qt::Key_E));
    tasting->addAction(QStringLiteral("Карточки дегустаторов…"), this, [this, go]() {
        go(4)();
        m_teams->issueCards();
    }, QKeySequence(Qt::CTRL + Qt::Key_P));

    QMenu* service = menuBar()->addMenu(QStringLiteral("&Сервис"));
    service->addAction(QStringLiteral("Резервные копии"), this, go(8), QKeySequence(Qt::CTRL + Qt::Key_9));
    service->addAction(QStringLiteral("Создать резервную копию…"), this, [this]() { m_backup->createBackup(); },
                       QKeySequence(Qt::CTRL + Qt::Key_B));
    service->addAction(QStringLiteral("Восстановить из копии…"), this, [this]() { m_backup->restoreBackup(); });
    service->addSeparator();
    service->addAction(QStringLiteral("Журнал действий…"), this, [this]() {
        AuditDialog dlg(m_audit, this);
        dlg.exec();
    });
    service->addAction(QStringLiteral("Выгрузить журнал в CSV…"), this, [this]() { AuditDialog::exportCsv(m_audit, this); });

    QMenu* help = menuBar()->addMenu(QStringLiteral("С&правка"));
    help->addAction(QStringLiteral("Содержание справки"), this, &AdminWindow::showHelp, QKeySequence::HelpContents);
    help->addAction(QStringLiteral("О программе"), this, [this]() {
        ui::AboutDialog::show(this, QStringLiteral("Программа администратора"));
    });
}

void AdminWindow::updateSummary()
{
    ProjectSummary s;
    try {
        s = MonitoringService(m_db).summary();
    } catch (const DbError&) {
        return;   // связь пропала — индикатор покажет updateConnection()
    }
    const int counts[] = {s.participants, s.nominations, s.samples, s.baskets, s.teams};
    for (const NavItem& item : m_nav)
        item.count->setText(item.countKey >= 0 ? QString::number(counts[item.countKey]) : QString());
    m_sessionLabel->setText(s.sessionTitle);
    m_prepBar->setValue(s.stagesDone());
    m_prepText->setText(QStringLiteral("%1 из %2 этапов выполнено").arg(s.stagesDone()).arg(ProjectSummary::kStages));
    m_auditStatus->setText(QStringLiteral("Журнал: %1 записей").arg(s.auditEntries));
}

void AdminWindow::updateConnection()
{
    const ConnectionSettings& c = m_db.settings();
    const bool ok = m_db.ping();
    m_connection->setText(QStringLiteral("<span style='color:%1'>●</span> %2: %3:%4 / %5")
                              .arg(ok ? ui::palette::kGreen.name() : ui::palette::kRed.name(),
                                   ok ? QStringLiteral("Подключено") : QStringLiteral("Нет связи с сервером"),
                                   c.host).arg(c.port).arg(c.database));
}

void AdminWindow::refreshAll()
{
    updateSummary();
    if (auto* page = qobject_cast<AdminPage*>(m_stack->currentWidget()))
        page->refresh();
}

void AdminWindow::newProject()
{
    if (!ui::Messages::confirm(
            this, QStringLiteral("Новый проект"),
            QStringLiteral("Будут удалены все данные дегустации: участники, номинации, образцы, корзины, команды, "
                           "дегустаторы, оценки и журнал действий. Пароль администратора и параметры подключения "
                           "сохранятся.\n\nПеред очисткой программа создаст резервную копию текущих данных. "
                           "Создать новый проект?"),
            QStringLiteral("Удалить данные и начать"), QStringLiteral("Отмена"), true))
        return;
    QString backupFile;
    const bool ok = ui::Messages::guard(this, [&]() {
        BackupService backup(m_db, &m_audit);
        backup.setPgBinDir(AppPaths::settings()->value(QStringLiteral("tools/pgBinDir")).toString());
        backupFile = backup.createAutoBackup(AppPaths::autoBackupDir(), QStringLiteral("sedm_before_new_project")).path;
        SessionService(m_db, m_audit).newProject();
    }, QStringLiteral("Новый проект не создан"));
    if (!ok)
        return;
    refreshAll();
    ui::Messages::info(this, QStringLiteral("Новый проект создан"),
                       QStringLiteral("Данные очищены. Копия прежних данных: %1").arg(QDir::toNativeSeparators(backupFile)));
}

void AdminWindow::connectionSettings()
{
    ui::ConnectionDialog dlg(m_db.settings(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    dlg.settings().save(*AppPaths::settings());
    ui::Messages::info(this, QStringLiteral("Параметры подключения сохранены"),
                       QStringLiteral("Новые параметры вступят в силу после перезапуска программы."));
}

void AdminWindow::changePassword()
{
    ChangePasswordDialog dlg(m_auth, this);
    dlg.exec();
}

void AdminWindow::showHelp()
{
    const auto* page = qobject_cast<AdminPage*>(m_stack->currentWidget());
    ui::HelpWindow::showTopic(this, page ? page->helpTopic() : QStringLiteral("index.html"));
}

}  // namespace sedm::admin
