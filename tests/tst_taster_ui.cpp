/**
 * @file tst_taster_ui.cpp
 * @brief Автоматические тесты интерфейса программы дегустатора (Qt Test).
 *
 * Окна работают на экранной платформе «offscreen» (ничего не появляется на
 * экране); действия пользователя имитируются нажатиями клавиш и мыши.
 * Проверяется сценарий диаграммы состояний: вход (ошибка, успех), выбор
 * корзины, ввод оценок ползунком, числом и кнопками, сохранение, завершение
 * корзины с подтверждением, просмотр оценок, выход, изменение размера шрифта.
 * Если задана переменная SEDM_SCREENSHOT_DIR, снимки окон сохраняются в PNG.
 * Параметры тестового сервера PostgreSQL — см. TestDatabase.h.
 */
#include "AuditService.h"
#include "BasketListPage.h"
#include "CoreResources.h"
#include "Database.h"
#include "DemoSeeder.h"
#include "EvaluationPage.h"
#include "EvaluationRepository.h"
#include "HoneySlider.h"
#include "LoginPage.h"
#include "ReviewPage.h"
#include "ScoreRow.h"
#include "TasterRepository.h"
#include "TasterWindow.h"
#include "TestDatabase.h"
#include "UiTestUtil.h"
#include "Theme.h"
#include "WidgetsResources.h"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <memory>

using namespace sedm;
using namespace sedm::taster;
using sedm::test::buttonWithText;
using sedm::test::DialogAnswerer;
using sedm::test::TestDatabase;

static void initTasterResources()
{
    Q_INIT_RESOURCE(taster);
}


class TestTasterUi : public QObject {
    Q_OBJECT

private:
    void screenshot(const QString& name)
    {
        const QString dir = qEnvironmentVariable("SEDM_SCREENSHOT_DIR");
        QTest::qWait(150);   // отложенные события компоновки окна
        if (!dir.isEmpty() && m_window)
            m_window->grab().save(QDir(dir).filePath(name + QStringLiteral(".png")));
    }
    QWidget* currentPage() const { return m_window->findChild<QStackedWidget*>()->currentWidget(); }
    void login(const QString& surname, const QString& pin)
    {
        auto* page = m_window->findChild<LoginPage*>();
        auto* loginField = page->findChild<QLineEdit*>(QStringLiteral("loginField"));
        auto* pinField = page->findChild<QLineEdit*>(QStringLiteral("pinField"));
        // QTest::keyClicks принимает только латиницу — фамилия подставляется в поле
        loginField->setFocus();
        loginField->setText(surname);
        pinField->clear();
        // PIN-код — на экранной цифровой клавиатуре
        for (QChar d : pin)
            QTest::mouseClick(buttonWithText(page, QString(d)), Qt::LeftButton);
        QTest::mouseClick(buttonWithText(page, QStringLiteral("Войти")), Qt::LeftButton);
    }

    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<AuditService> m_audit;
    std::unique_ptr<TasterWindow> m_window;
    DemoResult m_demo;
    QTemporaryDir m_tmp;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);   // настройки и черновики — в каталоге тестов
        if (!TestDatabase::configured())
            QSKIP("Не задан SEDM_TEST_PGSUPERPASS — тесты интерфейса пропущены.");
        m_testDb = std::make_unique<TestDatabase>(QStringLiteral("sedm_uitest"));
        m_testDb->create();
        m_db = std::make_unique<Database>(QStringLiteral("sedm_uitest_conn"));
        m_db->open(m_testDb->app());
        m_audit = std::make_unique<AuditService>(*m_db, m_tmp.filePath(QStringLiteral("pending.csv")));
        DemoOptions options;
        options.participants = 12;
        options.teams = 2;
        options.tastersPerTeam = 2;
        options.basketSize = 3;
        m_demo = DemoSeeder::seed(*m_db, *m_audit, options);
        QVERIFY(m_demo.credentials.size() == 4);

        m_window = std::make_unique<TasterWindow>(*m_db);
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

    void loginErrorsAndSuccess()
    {
        QVERIFY(qobject_cast<LoginPage*>(currentPage()));
        screenshot(QStringLiteral("taster_01_login"));
        const TasterCredentials& c = m_demo.credentials.first();
        QString wrong = c.pin;
        wrong[0] = QChar('0' + (wrong[0].digitValue() + 1) % 10);
        login(c.login, wrong);
        auto* page = m_window->findChild<LoginPage*>();
        QLabel* error = nullptr;
        for (QLabel* l : page->findChildren<QLabel*>())
            if (l->property("role").toString() == QLatin1String("error") && l->isVisible())
                error = l;
        QVERIFY(error);
        QVERIFY(error->text().contains(QStringLiteral("Неверная фамилия или PIN-код")));
        QVERIFY(qobject_cast<LoginPage*>(currentPage()));

        login(c.login.toLower(), c.pin);   // регистр букв не важен
        QVERIFY(qobject_cast<BasketListPage*>(currentPage()));
        screenshot(QStringLiteral("taster_02_baskets"));
    }

    void evaluateAndSave()
    {
        auto* list = m_window->findChild<BasketListPage*>();
        QPushButton* card = nullptr;
        for (QPushButton* b : list->findChildren<QPushButton*>())
            if (b->property("basket").toBool()) {
                card = b;
                break;
            }
        QVERIFY(card);
        QTest::mouseClick(card, Qt::LeftButton);
        auto* eval = qobject_cast<EvaluationPage*>(currentPage());
        QVERIFY(eval);
        QVERIFY(!eval->isDirty());
        screenshot(QStringLiteral("taster_03_evaluation_empty"));

        const auto rows = eval->findChildren<ScoreRow*>();
        QCOMPARE(rows.size(), 4);
        // 1) ползунок: щелчок на 3/4 шкалы
        auto* slider = rows[0]->findChild<ui::HoneySlider*>();
        QTest::mouseClick(slider, Qt::LeftButton, Qt::NoModifier, QPoint(slider->width() * 3 / 4, slider->height() / 2));
        QVERIFY(rows[0]->value().has_value());
        QVERIFY(qAbs(*rows[0]->value() - 75) <= 2);
        // 2) число с клавиатуры
        auto* edit = rows[1]->findChild<QLineEdit*>();
        edit->setFocus();
        QTest::keyClicks(edit, QStringLiteral("82"));
        QCOMPARE(*rows[1]->value(), 82);
        // 3) кнопка «+» и клавиши стрелок
        QTest::mouseClick(buttonWithText(rows[2], QStringLiteral("+")), Qt::LeftButton);
        QCOMPARE(*rows[2]->value(), 1);
        auto* tasteSlider = rows[3]->findChild<ui::HoneySlider*>();
        tasteSlider->setFocus();
        QTest::keyClick(tasteSlider, Qt::Key_End);
        QTest::keyClick(tasteSlider, Qt::Key_PageDown);
        QTest::keyClick(tasteSlider, Qt::Key_Left);
        QCOMPARE(*rows[3]->value(), 89);
        QTest::keyClick(rows[3]->findChild<ui::HoneySlider*>(), Qt::Key_Right);
        QCOMPARE(*rows[3]->value(), 90);
        QVERIFY(eval->isDirty());
        screenshot(QStringLiteral("taster_04_evaluation_scores"));

        QTest::mouseClick(buttonWithText(eval, QStringLiteral("Сохранить")), Qt::LeftButton);
        QVERIFY(!eval->isDirty());
        const auto taster = TasterRepository(*m_db).byLogin(m_demo.credentials.first().login);
        const auto saved = EvaluationRepository(*m_db).forTaster(taster->id());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(*saved.first().score(Criterion::Consistency), 82);
        QCOMPARE(*saved.first().score(Criterion::Taste), 90);
    }

    void finishRequiresAllScores()
    {
        auto* eval = qobject_cast<EvaluationPage*>(currentPage());
        QVERIFY(eval);
        // неполные оценки: программа называет неоценённые образцы, корзина не завершается
        DialogAnswerer warn({QStringLiteral("Понятно")});
        QTest::mouseClick(buttonWithText(eval, QStringLiteral("Завершить корзину")), Qt::LeftButton);
        QTRY_VERIFY(warn.done());
        QVERIFY(warn.seen().first().contains(QStringLiteral("Не оценены")));
        QVERIFY(qobject_cast<EvaluationPage*>(currentPage()));

        // оценить все образцы: на каждом ползунке End (100) и PageDown (−10) = 90
        for (int guard = 0; guard < 20; ++guard) {
            for (ScoreRow* row : eval->findChildren<ScoreRow*>()) {
                auto* s = row->findChild<ui::HoneySlider*>();
                s->setFocus();
                QTest::keyClick(s, Qt::Key_End);
                QTest::keyClick(s, Qt::Key_PageDown);
            }
            QPushButton* next = buttonWithText(eval, QStringLiteral("Следующий →"));
            if (!next || !next->isEnabled())
                break;
            QTest::mouseClick(next, Qt::LeftButton);
        }
        // подтверждение завершения и сообщение «Корзина завершена»
        DialogAnswerer confirm({QStringLiteral("Да, завершить"), QStringLiteral("Понятно")});
        QTest::mouseClick(buttonWithText(eval, QStringLiteral("Завершить корзину")), Qt::LeftButton);
        QTRY_VERIFY(confirm.done());
        QVERIFY(qobject_cast<BasketListPage*>(currentPage()));
        screenshot(QStringLiteral("taster_05_baskets_after_finish"));
    }

    void reviewAndLogout()
    {
        auto* list = m_window->findChild<BasketListPage*>();
        QPushButton* finished = nullptr;
        for (QPushButton* b : list->findChildren<QPushButton*>())
            if (b->property("basket").toBool() && b->property("finished").toBool())
                finished = b;
        QVERIFY(finished);
        QTest::mouseClick(finished, Qt::LeftButton);
        auto* review = qobject_cast<ReviewPage*>(currentPage());
        QVERIFY(review);
        auto* table = review->findChild<QTableWidget*>();
        QVERIFY(table->rowCount() >= 1);
        QCOMPARE(table->item(0, 2)->text(), QStringLiteral("90"));   // консистенция: 100 − 10
        screenshot(QStringLiteral("taster_06_review"));

        QTest::mouseClick(buttonWithText(review, QStringLiteral("Выйти")), Qt::LeftButton);
        QVERIFY(qobject_cast<LoginPage*>(currentPage()));
    }

    void fontScaling()
    {
        const int before = QApplication::font().pixelSize();
        QTest::mouseClick(buttonWithText(m_window.get(), QStringLiteral("A+")), Qt::LeftButton);
        QVERIFY(QApplication::font().pixelSize() > before);
        screenshot(QStringLiteral("taster_07_login_large"));
        QTest::mouseClick(buttonWithText(m_window.get(), QStringLiteral("A−")), Qt::LeftButton);
        QCOMPARE(QApplication::font().pixelSize(), before);
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");   // окна не показываются на экране
    QApplication app(argc, argv);
    initCoreResources();
    ui::initWidgetsResources();
    initTasterResources();
    ui::Theme::apply(app, 18, 1.0, ui::Theme::resourceText(QStringLiteral(":/taster/taster.qss")));
    TestTasterUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_taster_ui.moc"
