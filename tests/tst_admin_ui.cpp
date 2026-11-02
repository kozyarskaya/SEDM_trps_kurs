/**
 * @file tst_admin_ui.cpp
 * @brief Автоматические тесты интерфейса программы администратора (Qt Test, offscreen).
 *
 * Проверяются: переход по всем разделам боковой панели и сочетаниям клавиш меню,
 * счётчики и ход подготовки, регистрация образца с выдачей шифра, создание
 * корзины, создание команды с выдачей PIN-кодов, мониторинг, итоги, журнал.
 * Если задана переменная SEDM_SCREENSHOT_DIR, снимки окон сохраняются в PNG.
 * Параметры тестового сервера PostgreSQL — см. TestDatabase.h.
 */
#include "AdminWindow.h"
#include "AuditService.h"
#include "BasketsPage.h"
#include "CoreResources.h"
#include "Database.h"
#include "DemoSeeder.h"
#include "MonitoringPage.h"
#include "MonitoringService.h"
#include "ParticipantsPage.h"
#include "ResultsPage.h"
#include "SamplesPage.h"
#include "SessionPage.h"
#include "TeamsPage.h"
#include "TestDatabase.h"
#include "Theme.h"
#include "UiTestUtil.h"
#include "WidgetsResources.h"
#include "BackupPage.h"

#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <memory>

using namespace sedm;
using namespace sedm::admin;
using sedm::test::buttonWithText;
using sedm::test::DialogAnswerer;
using sedm::test::DialogStep;
using sedm::test::TestDatabase;

static void initAdminResources()
{
    Q_INIT_RESOURCE(admin);
}

class TestAdminUi : public QObject {
    Q_OBJECT

private:
    QWidget* currentPage() const { return m_window->findChild<QStackedWidget*>()->currentWidget(); }
    QPushButton* nav(const QString& text) const
    {
        for (QPushButton* b : m_window->findChildren<QPushButton*>())
            if (b->property("nav").toBool() && b->accessibleName() == text)
                return b;
        return nullptr;
    }
    ProjectSummary summary() const { return MonitoringService(*m_db).summary(); }

    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<AuditService> m_audit;
    std::unique_ptr<AdminWindow> m_window;
    QTemporaryDir m_tmp;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        if (!TestDatabase::configured())
            QSKIP("Не задан SEDM_TEST_PGSUPERPASS — тесты интерфейса пропущены.");
        m_testDb = std::make_unique<TestDatabase>(QStringLiteral("sedm_admintest"));
        m_testDb->create();
        m_db = std::make_unique<Database>(QStringLiteral("sedm_admintest_conn"));
        m_db->open(m_testDb->app());
        m_audit = std::make_unique<AuditService>(*m_db, m_tmp.filePath(QStringLiteral("pending.csv")));
        DemoOptions options;
        options.participants = 24;
        options.teams = 3;
        options.tastersPerTeam = 3;
        options.basketSize = 4;
        options.evaluate = true;
        DemoSeeder::seed(*m_db, *m_audit, options);
        m_window = std::make_unique<AdminWindow>(*m_db, *m_audit);
        m_window->resize(1366, 768);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanupTestCase()
    {
        m_window.reset();
        m_audit.reset();
        m_db.reset();
        if (m_testDb)
            m_testDb->drop();
    }

    void sidebarNavigation()
    {
        const struct {
            const char* nav;
            const char* shot;
        } pages[] = {{"Участники", "admin_01_participants"}, {"Образцы", "admin_02_samples"},
                     {"Корзины", "admin_03_baskets"},        {"Команды", "admin_04_teams"},
                     {"Сеанс дегустации", "admin_05_session"}, {"Мониторинг", "admin_06_monitoring"},
                     {"Итоги и ведомость", "admin_07_results"}, {"Резервные копии", "admin_08_backup"}};
        for (const auto& p : pages) {
            QPushButton* b = nav(QString::fromUtf8(p.nav));
            QVERIFY2(b, p.nav);
            QTest::mouseClick(b, Qt::LeftButton);
            QVERIFY(b->isChecked());
            sedm::test::screenshot(m_window.get(), QString::fromLatin1(p.shot));
        }
        // сочетания клавиш меню «Данные»
        QTest::keyClick(m_window.get(), Qt::Key_3, Qt::ControlModifier);
        QVERIFY(qobject_cast<SamplesPage*>(currentPage()));
        QTest::keyClick(m_window.get(), Qt::Key_1, Qt::ControlModifier);
        QVERIFY(qobject_cast<ParticipantsPage*>(currentPage()));
        QTest::keyClick(m_window.get(), Qt::Key_7, Qt::ControlModifier);
        QVERIFY(qobject_cast<MonitoringPage*>(currentPage()));
    }

    void participantsTable()
    {
        QTest::mouseClick(nav(QStringLiteral("Участники")), Qt::LeftButton);
        auto* page = qobject_cast<ParticipantsPage*>(currentPage());
        auto* table = page->findChild<QTableView*>();
        QCOMPARE(table->model()->rowCount(), 24);
        // поиск по фрагменту региона
        auto* search = page->findChildren<QLineEdit*>().first();
        search->setText(QStringLiteral("Тульская"));
        QVERIFY(table->model()->rowCount() < 24);
        search->clear();
        QCOMPARE(table->model()->rowCount(), 24);
    }

    void registerSample()
    {
        QTest::mouseClick(nav(QStringLiteral("Образцы")), Qt::LeftButton);
        auto* page = qobject_cast<SamplesPage*>(currentPage());
        QVERIFY(page);
        const int before = summary().samples;
        DialogAnswerer answers({QStringLiteral("Сохранить"), QStringLiteral("Понятно")});
        QTest::mouseClick(buttonWithText(page, QStringLiteral("+ Зарегистрировать образец")), Qt::LeftButton);
        QTRY_VERIFY(answers.done());
        QVERIFY(answers.seen().last().contains(QStringLiteral("шифр")));
        QCOMPARE(summary().samples, before + 1);
        QCOMPARE(summary().unassignedSamples, 1);
        sedm::test::screenshot(m_window.get(), QStringLiteral("admin_09_samples_registered"));
    }

    void createBasketWithNewSample()
    {
        QTest::mouseClick(nav(QStringLiteral("Корзины")), Qt::LeftButton);
        auto* page = qobject_cast<BasketsPage*>(currentPage());
        const int before = summary().baskets;
        DialogAnswerer answers({QStringLiteral("Создать")});
        QTest::mouseClick(buttonWithText(page, QStringLiteral("+ Создать корзину")), Qt::LeftButton);
        QTRY_VERIFY(answers.done());
        QCOMPARE(summary().baskets, before + 1);
        // новая корзина выбрана; если в пуле её номинации есть образец — добавить
        auto* add = buttonWithText(page, QStringLiteral("Добавить"));
        QVERIFY(add);
        if (add->isEnabled()) {
            QTest::mouseClick(add, Qt::LeftButton);
            QCOMPARE(summary().unassignedSamples, 0);
        }
        sedm::test::screenshot(m_window.get(), QStringLiteral("admin_10_baskets_new"));
    }

    void createTeamAndIssuePins()
    {
        QTest::mouseClick(nav(QStringLiteral("Команды")), Qt::LeftButton);
        auto* page = qobject_cast<TeamsPage*>(currentPage());
        const int teams = summary().teams;
        const int tasters = summary().tasters;
        auto fillNames = [](QWidget* dialog) {
            dialog->findChild<QPlainTextEdit*>()->setPlainText(QStringLiteral("Медведева Анна\nУваров Илья"));
        };
        DialogAnswerer answers(QVector<DialogStep>{{QStringLiteral("Создать команду"), fillNames},
                                                   {QStringLiteral("Готово"), nullptr},
                                                   {QStringLiteral("Закрыть"), nullptr}});
        QTest::mouseClick(buttonWithText(page, QStringLiteral("+ Создать команду")), Qt::LeftButton);
        QTRY_VERIFY(answers.done());
        QCOMPARE(summary().teams, teams + 1);
        QCOMPARE(summary().tasters, tasters + 2);
        sedm::test::screenshot(m_window.get(), QStringLiteral("admin_11_teams_new"));
    }

    void monitoringAndResults()
    {
        QTest::mouseClick(nav(QStringLiteral("Мониторинг")), Qt::LeftButton);
        auto* monitoring = qobject_cast<MonitoringPage*>(currentPage());
        auto* tree = monitoring->findChild<QTreeWidget*>();
        QCOMPARE(tree->topLevelItemCount(), summary().teams);
        QVERIFY(tree->topLevelItem(0)->childCount() > 0);

        QTest::mouseClick(nav(QStringLiteral("Итоги и ведомость")), Qt::LeftButton);
        auto* results = qobject_cast<ResultsPage*>(currentPage());
        auto* table = results->findChild<QTableWidget*>();
        QVERIFY(table->rowCount() > 0);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("1"));   // первое место — первая строка
        auto* nominations = results->findChild<QComboBox*>();
        QVERIFY(nominations->count() >= 6);
        nominations->setCurrentIndex(1);
        sedm::test::screenshot(m_window.get(), QStringLiteral("admin_12_results_other"));
    }

    void prepProgressAndJournal()
    {
        QTest::mouseClick(nav(QStringLiteral("Сеанс дегустации")), Qt::LeftButton);
        // карточка «Подготовка дегустации» на боковой панели совпадает со сводкой
        const ProjectSummary s = summary();
        const int expected = 1 + 1 + (s.unassignedSamples == 0 ? 1 : 0) + 1 + (s.unassignedBaskets == 0 ? 1 : 0) + 1;
        QCOMPARE(s.stagesDone(), expected);
        bool shown = false;
        for (QLabel* l : m_window->findChildren<QLabel*>())
            if (l->text() == QStringLiteral("%1 из 6 этапов выполнено").arg(expected))
                shown = true;
        QVERIFY(shown);
        sedm::test::screenshot(m_window.get(), QStringLiteral("admin_13_session_steps"));
        // журнал действий: меню «Сервис», окно закрывается кнопкой «Закрыть»
        DialogAnswerer answers({QStringLiteral("Закрыть")});
        QTimer::singleShot(0, [this]() {
            for (QAction* a : m_window->findChildren<QAction*>())
                if (a->text() == QStringLiteral("Журнал действий…"))
                    a->trigger();
        });
        QTRY_VERIFY(answers.done());
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    initCoreResources();
    ui::initWidgetsResources();
    initAdminResources();
    ui::Theme::apply(app, 14);
    TestAdminUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_admin_ui.moc"
