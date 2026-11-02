/**
 * @file tst_performance.cpp
 * @brief Испытания производительности СЭДМ на объёме ТЗ (п. 4.1.5–4.1.7).
 *
 * Объём: 500 образцов, 50 команд по 3 дегустатора, 50 корзин по 10 образцов,
 * 1500 полных оценок. Требования ТЗ:
 *   — отклик на действие пользователя — не более 2 с (кроме экспорта XLSX и
 *     резервного копирования);
 *   — формирование итогового XLSX — не более 30 с;
 *   — оперативная память клиента — не более 512 МБ.
 * Результаты замеров выводятся в журнал теста и, если задана переменная
 * SEDM_PERF_REPORT, записываются в файл (строки «операция;время, с;предел, с»).
 * Параметры тестового сервера PostgreSQL — см. TestDatabase.h.
 */
#include "AuditService.h"
#include "BackupService.h"
#include "CoreResources.h"
#include "Database.h"
#include "DemoSeeder.h"
#include "EvaluationRepository.h"
#include "MonitoringService.h"
#include "ReportRepository.h"
#include "ResultService.h"
#include "TastingService.h"
#include "TasterRepository.h"
#include "TestDatabase.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

using namespace sedm;
using sedm::test::TestDatabase;

namespace {
/// Рабочий набор процесса в мегабайтах (память, занятая программой).
double workingSetMb()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
#endif
    return 0;
}
}  // namespace

class TestPerformance : public QObject {
    Q_OBJECT

private:
    /// Замер операции: время в секундах; проверка предела (0 — без предела).
    double measure(const QString& name, double limitSeconds, const std::function<void()>& op)
    {
        QElapsedTimer t;
        t.start();
        op();
        const double s = t.nsecsElapsed() / 1e9;
        qInfo().noquote() << QStringLiteral("%1: %2 с (предел %3)").arg(name).arg(s, 0, 'f', 3)
                                 .arg(limitSeconds > 0 ? QString::number(limitSeconds) + QStringLiteral(" с")
                                                       : QStringLiteral("не задан"));
        m_report << QStringLiteral("%1;%2;%3").arg(name).arg(s, 0, 'f', 3)
                        .arg(limitSeconds > 0 ? QString::number(limitSeconds) : QStringLiteral("—"));
        if (limitSeconds > 0)
            [&]() { QVERIFY2(s <= limitSeconds, qPrintable(name)); }();
        return s;
    }

    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<AuditService> m_audit;
    QTemporaryDir m_tmp;
    QStringList m_report;
    DemoResult m_demo;

private slots:
    void initTestCase()
    {
        initCoreResources();
        if (!TestDatabase::configured())
            QSKIP("Не задан SEDM_TEST_PGSUPERPASS — испытания производительности пропущены.");
        m_testDb = std::make_unique<TestDatabase>(QStringLiteral("sedm_perftest"));
        m_testDb->create();
        m_db = std::make_unique<Database>(QStringLiteral("sedm_perf_conn"));
        m_db->open(m_testDb->app());
        m_audit = std::make_unique<AuditService>(*m_db, m_tmp.filePath(QStringLiteral("pending.csv")));
        DemoOptions options;
        options.participants = 500;
        options.teams = 50;
        options.tastersPerTeam = 3;
        options.basketSize = 10;
        options.evaluate = true;
        measure(QStringLiteral("Заполнение базы: 500 образцов, 50 команд, 1500 оценок"), 0,
                [&]() { m_demo = DemoSeeder::seed(*m_db, *m_audit, options); });
        QCOMPARE(m_demo.samples, 500);
        QCOMPARE(m_demo.evaluations, 1500);
    }

    void cleanupTestCase()
    {
        const QString path = qEnvironmentVariable("SEDM_PERF_REPORT");
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text))
                f.write((QStringLiteral("Операция;Время, с;Предел, с\n") + m_report.join(QLatin1Char('\n'))
                         + QLatin1Char('\n')).toUtf8());
        }
        m_audit.reset();
        m_db.reset();
        if (m_testDb)
            m_testDb->drop();
    }

    void responseTimes()
    {
        MonitoringSnapshot snap;
        measure(QStringLiteral("Мониторинг хода дегустации (50 команд)"), 2, [&]() {
            snap = MonitoringService(*m_db).snapshot();
        });
        QCOMPARE(snap.teams.size(), 50);
        measure(QStringLiteral("Сводка проекта (боковая панель)"), 2, [&]() { MonitoringService(*m_db).summary(); });

        const auto taster = TasterRepository(*m_db).byLogin(m_demo.credentials.first().login);
        QVERIFY(taster);
        TastingService ts(*m_db, *m_audit, *taster, QString(), m_tmp.filePath(QStringLiteral("drafts")));
        QVector<TasterBasketRow> baskets;
        measure(QStringLiteral("Список корзин дегустатора"), 2, [&]() { baskets = ts.baskets(); });
        QVERIFY(!baskets.isEmpty());
        BasketSheet sheet;
        measure(QStringLiteral("Открытие корзины (10 образцов)"), 2, [&]() { sheet = ts.openBasket(baskets.first().basket.id()); });
        QCOMPARE(sheet.samples.size(), 10);
        QVector<NominationResult> results;
        measure(QStringLiteral("Расчёт итогов и мест (500 образцов)"), 2, [&]() {
            results = ResultService(*m_db, *m_audit).compute();
        });
        QCOMPARE(results.size(), 6);
    }

    void saveEvaluations()
    {
        // сохранение оценок корзины — от имени дегустатора с действующим сеансом
        const auto taster = TasterRepository(*m_db).byLogin(m_demo.credentials.last().login);
        TasterRepository(*m_db).setSessionToken(taster->id(), QStringLiteral("00000000-0000-0000-0000-000000000001"));
        TastingService ts(*m_db, *m_audit, *taster, QStringLiteral("00000000-0000-0000-0000-000000000001"),
                          m_tmp.filePath(QStringLiteral("drafts")));
        const auto rows = ts.baskets();
        BasketSheet sheet = ts.openBasket(rows.first().basket.id());
        QVector<Evaluation> evals = sheet.saved;
        for (Evaluation& e : evals)
            e.setScore(Criterion::Taste, 77);
        measure(QStringLiteral("Сохранение оценок корзины (10 образцов)"), 2, [&]() { ts.save(rows.first().basket.id(), evals); });
        measure(QStringLiteral("Контрольная точка (черновик)"), 2, [&]() { ts.saveCheckpoint(rows.first().basket.id(), evals); });
    }

    void exportAndBackup()
    {
        const QString xlsx = m_tmp.filePath(QStringLiteral("perf.xlsx"));
        measure(QStringLiteral("Формирование итоговой ведомости XLSX"), 30, [&]() {
            ResultService(*m_db, *m_audit).exportXlsx(xlsx);
        });
        QVERIFY(QFileInfo(xlsx).size() > 10000);
        BackupService backup(*m_db, m_audit.get());
        const QString copy = m_tmp.filePath(QStringLiteral("perf.sql"));
        measure(QStringLiteral("Резервная копия базы данных"), 0, [&]() { backup.createBackup(copy); });
        measure(QStringLiteral("Восстановление из резервной копии"), 0, [&]() {
            backup.restoreBackup(copy, m_tmp.filePath(QStringLiteral("safety")));
        });
        QCOMPARE(EvaluationRepository(*m_db).count(), 1500);
    }

    void memory()
    {
        const double mb = workingSetMb();
        qInfo().noquote() << QStringLiteral("Пиковый объём памяти процесса: %1 МБ (предел 512 МБ)").arg(mb, 0, 'f', 1);
        m_report << QStringLiteral("Пиковый объём памяти, МБ;%1;512").arg(mb, 0, 'f', 1);
        QVERIFY(mb < 512);
    }
};

QTEST_GUILESS_MAIN(TestPerformance)
#include "tst_performance.moc"
