/**
 * @file tst_services.cpp
 * @brief Интеграционные тесты слоя данных и сервисов СЭДМ на сервере PostgreSQL.
 *
 * Тесты выполняются по порядку и проходят полный сценарий конкурса: первичная
 * настройка сервера, вход администратора, реестр (вручную и из CSV/XLSX),
 * образцы и шифры, корзины, команды и дегустаторы, вход и блокировка,
 * оценивание с черновиками и завершением, вход с другого компьютера, итоги и
 * ведомость XLSX, мониторинг, журнал CSV, резервное копирование и
 * восстановление, новый проект. Параметры сервера — см. TestDatabase.h.
 */
#include "AuditService.h"
#include "AuthService.h"
#include "BackupService.h"
#include "BasketService.h"
#include "CoreResources.h"
#include "CsvReader.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"
#include "MonitoringService.h"
#include "PinGenerator.h"
#include "RegistryService.h"
#include "ResultService.h"
#include "SampleService.h"
#include "SchemaInstaller.h"
#include "SessionService.h"
#include "SqlScript.h"
#include "TastingService.h"
#include "TeamService.h"
#include "TestDatabase.h"

#include "xlsxdocument.h"

#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <memory>

using namespace sedm;
using sedm::test::TestDatabase;

namespace {

Evaluation full(int sampleId, int a, int c, int r, int t, const QString& note = QString())
{
    Evaluation e(0, sampleId);
    e.setScore(Criterion::Appearance, a);
    e.setScore(Criterion::Consistency, c);
    e.setScore(Criterion::Aroma, r);
    e.setScore(Criterion::Taste, t);
    e.setNote(note);
    return e;
}

/// PIN-код, заведомо отличный от верного.
QString wrongPin(const QString& pin)
{
    QString w = pin;
    w[0] = QChar(QLatin1Char('0' + ((pin[0].digitValue() + 1) % 10)));
    return w;
}

}  // namespace

class TestServices : public QObject {
    Q_OBJECT

private:
    int count(const QString& table) const
    {
        return m_db->scalar(QStringLiteral("SELECT count(*) FROM %1").arg(table)).toInt();
    }
    QString writeFile(const QString& name, const QByteArray& content) const
    {
        const QString path = m_tmp.filePath(name);
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content);
        return path;
    }
    int nominationId(const QString& name) const
    {
        for (const Nomination& n : RegistryService(*m_db, *m_audit).nominations())
            if (n.name() == name)
                return n.id();
        return 0;
    }
    int participantId(const QString& fullName) const
    {
        for (const Participant& p : RegistryService(*m_db, *m_audit).participants())
            if (p.fullName() == fullName)
                return p.id();
        return 0;
    }

    std::unique_ptr<TestDatabase> m_testDb;
    std::unique_ptr<Database> m_db;
    std::unique_ptr<AuditService> m_audit;
    QTemporaryDir m_tmp;

    // Состояние сценария, общее для последовательных тестов
    int m_basket1 = 0, m_basket2 = 0, m_basket3 = 0;
    int m_team1 = 0, m_team2 = 0;
    QVector<TasterCredentials> m_creds;   // дегустаторы команды 1
    TasterCredentials m_elkina;           // дегустатор команды 2
    QVector<int> m_b1Samples, m_b2Samples;

private slots:
    void initTestCase()
    {
        initCoreResources();
        if (!TestDatabase::configured())
            QSKIP("Не задан SEDM_TEST_PGSUPERPASS — интеграционные тесты пропущены.");
        QVERIFY(m_tmp.isValid());
        m_testDb = std::make_unique<TestDatabase>(QStringLiteral("sedm_test"));
    }

    void cleanupTestCase()
    {
        m_audit.reset();
        m_db.reset();
        if (m_testDb)
            m_testDb->drop();
    }

    // ---------------------------------------------------------------- сервер
    void schemaInstall()
    {
        const auto r = m_testDb->create();
        QVERIFY(r.roleCreated);
        QVERIFY(r.databaseCreated);
        QVERIFY(r.schemaCreated);
        const auto again = SchemaInstaller::install(m_testDb->superuser(), m_testDb->app());
        QVERIFY(!again.roleCreated && !again.databaseCreated && !again.schemaCreated);

        m_db = std::make_unique<Database>(QStringLiteral("sedm_test_conn"));
        m_db->open(m_testDb->app());
        QVERIFY(SchemaInstaller::isInstalled(*m_db));
        m_audit = std::make_unique<AuditService>(*m_db, m_tmp.filePath(QStringLiteral("audit_pending.csv")));

        // Разграничение доступа: учётной записи приложения доступна только схема sedm
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("CREATE TABLE public.intruder (id int)")), DbError);
        // Неверный пароль приложения — понятная ошибка подключения
        ConnectionSettings wrong = m_testDb->app();
        wrong.password = QStringLiteral("not-the-password");
        Database other(QStringLiteral("sedm_test_wrong"));
        try {
            other.open(wrong);
            QFAIL("подключение с неверным паролем должно быть отклонено");
        } catch (const DbError& e) {
            QCOMPARE(e.kind(), DbError::Kind::Connection);
        }
    }

    void adminAuthentication()
    {
        AuthService auth(*m_db, *m_audit);
        QVERIFY(!auth.adminExists());
        QCOMPARE(auth.loginAdmin(QStringLiteral("x")).status, LoginStatus::NoAdmin);
        QVERIFY_EXCEPTION_THROWN(auth.createAdmin(QStringLiteral("ab1")), DomainError);
        QVERIFY_EXCEPTION_THROWN(auth.createAdmin(QStringLiteral("толькобуквы")), DomainError);
        auth.createAdmin(QStringLiteral("Мёд2026"));
        QVERIFY(auth.adminExists());
        QVERIFY_EXCEPTION_THROWN(auth.createAdmin(QStringLiteral("Другой1")), DomainError);
        QCOMPARE(auth.loginAdmin(QStringLiteral("Мёд2026")).status, LoginStatus::Ok);

        // пять неверных паролей подряд — блокировка на 60 с, верный пароль не принимается
        for (int i = 0; i < 4; ++i)
            QCOMPARE(auth.loginAdmin(QStringLiteral("неверный1")).status, LoginStatus::WrongSecret);
        const auto locked = auth.loginAdmin(QStringLiteral("неверный1"));
        QCOMPARE(locked.status, LoginStatus::Locked);
        QVERIFY(locked.secondsLeft > 0 && locked.secondsLeft <= Account::kLockSeconds);
        QCOMPARE(auth.loginAdmin(QStringLiteral("Мёд2026")).status, LoginStatus::Locked);
        m_db->execScript(QStringLiteral("UPDATE app_admin SET locked_until = now() - interval '1 second'"));
        QCOMPARE(auth.loginAdmin(QStringLiteral("Мёд2026")).status, LoginStatus::Ok);

        QVERIFY_EXCEPTION_THROWN(auth.changeAdminPassword(QStringLiteral("неверный1"), QStringLiteral("Новый2026")),
                                 DomainError);
        auth.changeAdminPassword(QStringLiteral("Мёд2026"), QStringLiteral("Новый2026"));
        QCOMPARE(auth.loginAdmin(QStringLiteral("Мёд2026")).status, LoginStatus::WrongSecret);
        QCOMPARE(auth.loginAdmin(QStringLiteral("Новый2026")).status, LoginStatus::Ok);

        // в базе — только bcrypt-хэш
        const QString hash = m_db->scalar(QStringLiteral("SELECT password_hash FROM app_admin")).toString();
        QVERIFY(hash.startsWith(QLatin1String("$2b$10$")));
        QCOMPARE(hash.size(), 60);
    }

    // ---------------------------------------------------------------- реестр
    void nominationsAndParticipants()
    {
        RegistryService reg(*m_db, *m_audit);
        const Nomination lime = reg.addNomination(QStringLiteral("  липовый "));
        QCOMPARE(lime.name(), QStringLiteral("Липовый"));
        QVERIFY_EXCEPTION_THROWN(reg.addNomination(QStringLiteral("ЛИПОВЫЙ")), DomainError);
        QVERIFY_EXCEPTION_THROWN(reg.addNomination(QStringLiteral("   ")), DomainError);
        const Nomination buck = reg.addNomination(QStringLiteral("Гречишный мёд"));
        reg.renameNomination(buck.id(), QStringLiteral("Гречишный"));
        QCOMPARE(nominationId(QStringLiteral("Гречишный")), buck.id());

        Participant p;
        p.setFullName(QStringLiteral("Сидоров  Иван Петрович"));
        p.setPhone(QStringLiteral("+7 912 345-67-89"));
        p.setEmail(QStringLiteral("sidorov@mail.ru"));
        p.setRegion(QStringLiteral("Тульская область"));
        p.setNominationId(lime.id());
        const Participant saved = reg.addParticipant(p);
        QVERIFY(saved.id() > 0);
        QCOMPARE(saved.fullName(), QStringLiteral("Сидоров Иван Петрович"));

        Participant bad = p;
        bad.setFullName(QStringLiteral("Сидоров"));
        bad.setEmail(QStringLiteral("не-почта"));
        try {
            reg.addParticipant(bad);
            QFAIL("запись с ошибками не должна сохраняться");
        } catch (const DomainError& e) {
            QVERIFY(e.message().contains(QStringLiteral("ФИО")));
            QVERIFY(e.message().contains(QStringLiteral("E-mail")));
        }

        Participant changed = saved;
        changed.setRegion(QStringLiteral("Московская область"));
        reg.updateParticipant(changed);
        QCOMPARE(reg.participant(saved.id())->region(), QStringLiteral("Московская область"));
        QVERIFY_EXCEPTION_THROWN(reg.removeNomination(lime.id()), DomainError);   // есть участник

        Participant temp = p;
        temp.setFullName(QStringLiteral("Временный Участник"));
        const Participant t = reg.addParticipant(temp);
        reg.removeParticipant(t.id());
        QVERIFY(!reg.participant(t.id()));
        QCOMPARE(reg.participantCount(), 1);
    }

    void importCsvAndXlsx()
    {
        RegistryService reg(*m_db, *m_audit);
        const QByteArray csv =
            "\xEF\xBB\xBF"
            "ФИО;Телефон;E-mail;Регион;Номинация\r\n"
            "Иванов Пётр Сергеевич;+7 (903) 111-22-33;ivanov@yandex.ru;Тульская область;Липовый\r\n"
            "Кузнецова Ольга Ивановна;89031112234;;\"Рязанская область\";цветочный\r\n"
            "\"Морозов; Алексей\";;;Тверская область;Липовый\r\n"
            "Сидоров Иван Петрович;8 912 345 67 89;sidorov@mail.ru;Тульская область;Липовый\r\n"
            "Петрова Анна;;;;Липовый\r\n";
        const QString csvPath = writeFile(QStringLiteral("registry.csv"), csv);
        ImportPreview preview = reg.previewImport(csvPath);
        QCOMPARE(preview.rows.size(), 5);
        QCOMPARE(preview.validCount(), 3);
        QCOMPARE(preview.errorCount(), 2);
        QCOMPARE(preview.duplicateCount(), 1);   // Сидоров уже в реестре (телефон в другом виде)
        QVERIFY(preview.rows[1].newNomination);
        QVERIFY(!preview.rows[0].newNomination);
        QCOMPARE(preview.rows[2].line, 4);

        const ImportResult r = reg.applyImport(preview);
        QCOMPARE(r.added, 2);
        QCOMPARE(r.skippedDuplicates, 1);
        QCOMPARE(r.skippedErrors, 2);
        QCOMPARE(r.createdNominations, QStringList{QStringLiteral("Цветочный")});
        // повторная загрузка того же файла — все строки уже в реестре
        QCOMPARE(reg.previewImport(csvPath).duplicateCount(), 3);

        // XLSX: столбцы «Фамилия», «Имя», «Отчество»; телефон хранится числом
        const QString xlsxPath = m_tmp.filePath(QStringLiteral("registry.xlsx"));
        {
            QXlsx::Document x;
            const QStringList headers = {QStringLiteral("Фамилия"), QStringLiteral("Имя"), QStringLiteral("Отчество"),
                                         QStringLiteral("Телефон"), QStringLiteral("Регион"), QStringLiteral("Номинация")};
            for (int c = 0; c < headers.size(); ++c)
                x.write(1, c + 1, headers[c]);
            x.write(2, 1, QStringLiteral("Смирнова"));
            x.write(2, 2, QStringLiteral("Елена"));
            x.write(2, 3, QStringLiteral("Викторовна"));
            x.write(2, 4, 79161234567.0);
            x.write(2, 5, QStringLiteral("Калужская область"));
            x.write(2, 6, QStringLiteral("Гречишный"));
            QVERIFY(x.saveAs(xlsxPath));
        }
        preview = reg.previewImport(xlsxPath);
        QCOMPARE(preview.validCount(), 1);
        QCOMPARE(preview.rows[0].participant.fullName(), QStringLiteral("Смирнова Елена Викторовна"));
        QCOMPARE(preview.rows[0].participant.phone(), QStringLiteral("79161234567"));
        QCOMPARE(reg.applyImport(preview).added, 1);
        QCOMPARE(reg.participantCount(), 4);

        QVERIFY_EXCEPTION_THROWN(reg.previewImport(writeFile(QStringLiteral("registry.doc"), "x")), DomainError);
    }

    // --------------------------------------------------------------- образцы
    void sampleRegistration()
    {
        SampleService samples(*m_db, *m_audit);
        QVector<int> ids;
        for (const Participant& p : RegistryService(*m_db, *m_audit).participants())
            ids << p.id();
        const QVector<Sample> created = samples.registerForParticipants(ids);
        QCOMPARE(created.size(), 4);
        QSet<QString> ciphers;
        int previous = 0;
        for (const Sample& s : created) {
            QVERIFY(Sample::isValidCipher(s.cipher()));
            QVERIFY(s.seqNumber() > previous);   // сквозная нумерация
            previous = s.seqNumber();
            ciphers.insert(s.cipher());
        }
        QCOMPARE(ciphers.size(), created.size());

        const int sidorov = participantId(QStringLiteral("Сидоров Иван Петрович"));
        const int lime = nominationId(QStringLiteral("Липовый"));
        for (int i = 0; i < 3; ++i)
            samples.registerSample(sidorov, lime);
        QVERIFY_EXCEPTION_THROWN(samples.registerSample(0, lime), DomainError);

        // уникальность шифра обеспечивает и база данных
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE sample SET cipher = '%1' WHERE id = %2")
                                                      .arg(created[0].cipher()).arg(created[1].id())),
                                 DbError);
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE sample SET cipher = '0123' WHERE id = %1")
                                                      .arg(created[1].id())),
                                 DbError);

        const Sample extra = samples.registerSample(sidorov, lime);
        samples.removeSample(extra.id());
        QCOMPARE(samples.count(), 7);
        QCOMPARE(samples.unassignedCount(), 7);
        QVERIFY_EXCEPTION_THROWN(RegistryService(*m_db, *m_audit).removeParticipant(sidorov), DomainError);
    }

    // ---------------------------------------------------------------- корзины
    void basketRules()
    {
        BasketService baskets(*m_db, *m_audit);
        const int lime = nominationId(QStringLiteral("Липовый"));
        const int buck = nominationId(QStringLiteral("Гречишный"));
        const int flower = nominationId(QStringLiteral("Цветочный"));

        const Basket b1 = baskets.createBasket(baskets.suggestedNumber(), lime, 3);
        QCOMPARE(b1.number(), 1);
        QVERIFY_EXCEPTION_THROWN(baskets.createBasket(1, lime, 3), DomainError);
        QVERIFY_EXCEPTION_THROWN(baskets.createBasket(2, lime, 0), DomainError);
        QVERIFY_EXCEPTION_THROWN(baskets.createBasket(2, lime, 101), DomainError);

        const QVector<Sample> flowerPool = baskets.availableSamples(flower);
        QCOMPARE(flowerPool.size(), 1);
        QVERIFY_EXCEPTION_THROWN(baskets.addSample(b1.id(), flowerPool.first().id()), DomainError);

        QCOMPARE(baskets.fillFromPool(b1.id()), 3);
        const QVector<Sample> limePool = baskets.availableSamples(lime);
        QCOMPARE(limePool.size(), 2);
        QVERIFY_EXCEPTION_THROWN(baskets.addSample(b1.id(), limePool.first().id()), DomainError);   // заполнена
        // переполнение в обход программы запрещает триггер базы данных
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE sample SET basket_id = %1 WHERE id = %2")
                                                      .arg(b1.id()).arg(limePool.first().id())),
                                 DbError);

        const int first = baskets.basketSamples(b1.id()).first().id();
        baskets.removeSample(b1.id(), first);
        QCOMPARE(baskets.availableSamples(lime).size(), 3);   // образец вернулся в пул
        baskets.addSample(b1.id(), first);
        QVERIFY_EXCEPTION_THROWN(baskets.updateBasket(b1.id(), 1, lime, 2), DomainError);
        QVERIFY_EXCEPTION_THROWN(baskets.updateBasket(b1.id(), 1, buck, 3), DomainError);
        baskets.updateBasket(b1.id(), 1, lime, 4);
        baskets.updateBasket(b1.id(), 1, lime, 3);

        const Basket b2 = baskets.createBasket(2, lime, 2);
        QCOMPARE(baskets.fillFromPool(b2.id()), 2);
        const Basket b3 = baskets.createBasket(3, buck, 1);
        QCOMPARE(baskets.fillFromPool(b3.id()), 1);
        const Basket b4 = baskets.createBasket(4, buck, 5);
        baskets.removeBasket(b4.id());
        QCOMPARE(baskets.baskets().size(), 3);

        m_basket1 = b1.id();
        m_basket2 = b2.id();
        m_basket3 = b3.id();
    }

    // ----------------------------------------------------- команды, дегустаторы
    void teamsAndTasters()
    {
        TeamService teams(*m_db, *m_audit);
        Team t1;
        m_creds = teams.createTeam(1, QStringLiteral("Эксперты"),
                                   TeamService::parseNames(QStringLiteral("Иванова Анна Петровна\n\n  иванова мария\r\nПетров")),
                                   &t1);
        QCOMPARE(m_creds.size(), 3);
        QCOMPARE(m_creds[0].login, QStringLiteral("Иванова"));
        QCOMPARE(m_creds[1].login, QStringLiteral("Иванова2"));
        QCOMPARE(m_creds[2].login, QStringLiteral("Петров"));
        for (const TasterCredentials& c : m_creds) {
            QVERIFY(PinGenerator::isWellFormed(c.pin));
            QVERIFY(!PinGenerator::isTrivial(c.pin));
            QCOMPARE(c.teamTitle, QStringLiteral("Команда 1 «Эксперты»"));
        }
        const QString pinHash = m_db->scalar(QStringLiteral("SELECT pin_hash FROM taster WHERE login = 'Петров'")).toString();
        QVERIFY(pinHash.startsWith(QLatin1String("$2b$")));

        QVERIFY_EXCEPTION_THROWN(teams.createTeam(1, QString(), {}), DomainError);                       // номер занят
        QVERIFY_EXCEPTION_THROWN(teams.createTeam(2, QString(), {QStringLiteral("Smith John")}), DomainError);
        QVERIFY_EXCEPTION_THROWN(teams.createTeam(0, QString(), {}), DomainError);
        Team t2;
        QVERIFY(teams.createTeam(2, QString(), {}, &t2).isEmpty());   // перечень фамилий может быть пустым
        m_elkina = teams.addTaster(t2.id(), QStringLiteral("Ёлкина Вера"));
        QCOMPARE(m_elkina.login, QStringLiteral("Ёлкина"));
        QCOMPARE(teams.tasters(t2.id()).size(), 1);
        teams.renameTaster(m_elkina.tasterId, QStringLiteral("Ёлкина Вера Ивановна"));
        QCOMPARE(teams.tasters(t2.id()).first().login(), QStringLiteral("Ёлкина"));   // логин не меняется

        BasketService baskets(*m_db, *m_audit);
        baskets.assignTeam(m_basket1, t1.id());
        baskets.assignTeam(m_basket2, t1.id());
        QCOMPARE(*baskets.basket(m_basket1)->teamId(), t1.id());
        baskets.assignTeam(m_basket3, t2.id());
        baskets.assignTeam(m_basket3, std::nullopt);   // снять назначение до начала оценивания можно
        m_team1 = t1.id();
        m_team2 = t2.id();
    }

    void tasterLoginAndLock()
    {
        AuthService auth(*m_db, *m_audit);
        const auto ok = auth.loginTaster(QStringLiteral("  ИВАНОВА "), m_creds[0].pin);
        QCOMPARE(ok.status, LoginStatus::Ok);
        QCOMPARE(ok.taster.teamId(), m_team1);
        QVERIFY(!ok.sessionToken.isEmpty());
        QCOMPARE(auth.loginTaster(QStringLiteral("елкина"), m_elkina.pin).status, LoginStatus::Ok);   // «ё» = «е»
        QCOMPARE(auth.loginTaster(QStringLiteral("Неизвестная"), QStringLiteral("2580")).status,
                 LoginStatus::WrongSecret);
        QCOMPARE(auth.loginTaster(QStringLiteral("Иванова"), QStringLiteral("12a4")).status, LoginStatus::WrongSecret);

        const QString wrong = wrongPin(m_creds[2].pin);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(auth.loginTaster(QStringLiteral("Петров"), wrong).status, LoginStatus::WrongSecret);
        QCOMPARE(auth.loginTaster(QStringLiteral("Петров"), wrong).status, LoginStatus::Locked);
        QCOMPARE(auth.loginTaster(QStringLiteral("Петров"), m_creds[2].pin).status, LoginStatus::Locked);

        // новый PIN-код снимает блокировку, прежний перестаёт действовать
        const auto fresh = TeamService(*m_db, *m_audit).resetPins({m_creds[2].tasterId});
        QCOMPARE(fresh.size(), 1);
        if (fresh[0].pin != m_creds[2].pin)
            QCOMPARE(auth.loginTaster(QStringLiteral("Петров"), m_creds[2].pin).status, LoginStatus::WrongSecret);
        QCOMPARE(auth.loginTaster(QStringLiteral("Петров"), fresh[0].pin).status, LoginStatus::Ok);
        m_creds[2].pin = fresh[0].pin;
    }

    // ------------------------------------------------------------ оценивание
    void tastingWorkflow()
    {
        AuthService auth(*m_db, *m_audit);
        const auto login = auth.loginTaster(QStringLiteral("Иванова"), m_creds[0].pin);
        TastingService ts(*m_db, *m_audit, login.taster, login.sessionToken, m_tmp.filePath(QStringLiteral("drafts")));
        QCOMPARE(ts.team()->number(), 1);
        const auto rows = ts.baskets();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0].nominationName, QStringLiteral("Липовый"));
        QVERIFY(!ts.allFinished());
        QVERIFY(ts.canEnter());
        QVERIFY_EXCEPTION_THROWN(ts.openBasket(m_basket3), DomainError);   // чужая корзина

        BasketSheet sheet = ts.openBasket(m_basket1);
        QCOMPARE(sheet.samples.size(), 3);
        QVERIFY(std::is_sorted(sheet.samples.begin(), sheet.samples.end(),
                               [](const Sample& a, const Sample& b) { return a.cipher() < b.cipher(); }));
        for (const Sample& s : sheet.samples)
            m_b1Samples << s.id();
        const int s0 = m_b1Samples[0], s1 = m_b1Samples[1], s2 = m_b1Samples[2];

        // частичное сохранение — допускается
        Evaluation partial(0, s0);
        partial.setScore(Criterion::Appearance, 80);
        ts.save(m_basket1, {partial});
        sheet = ts.openBasket(m_basket1);
        QCOMPARE(sheet.saved.size(), 1);
        QCOMPARE(*sheet.saved[0].score(Criterion::Appearance), 80);
        QVERIFY(!sheet.savedFor(s0).isComplete());

        // контрольная точка: черновик в базе и в локальном файле
        QVector<Evaluation> evals = {full(s0, 80, 70, 90, 85), full(s1, 60, 65, 70, 75, QStringLiteral("Слегка засахарен"))};
        QVERIFY(ts.saveCheckpoint(m_basket1, evals));
        QCOMPARE(count(QStringLiteral("score_draft")), 2);
        sheet = ts.openBasket(m_basket1);
        QCOMPARE(sheet.draft.size(), 2);
        m_db->execScript(QStringLiteral("DELETE FROM score_draft"));   // сбой: в базе черновика нет
        sheet = ts.openBasket(m_basket1);
        QCOMPARE(sheet.draft.size(), 2);                                 // восстановлен из файла
        ts.discardDraft(m_basket1);
        QVERIFY(ts.openBasket(m_basket1).draft.isEmpty());

        // завершить корзину можно только при полных оценках всех образцов
        QVERIFY_EXCEPTION_THROWN(ts.finish(m_basket1, evals), DomainError);
        QVERIFY(!ts.openBasket(m_basket1).readOnly());
        QCOMPARE(ts.openBasket(m_basket1).saved.size(), 1);             // неудачное завершение ничего не сохранило
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE evaluation SET taste = 101")), DbError);

        evals << full(s2, 90, 90, 95, 100);
        ts.finish(m_basket1, evals);
        sheet = ts.openBasket(m_basket1);
        QVERIFY(sheet.readOnly());
        QCOMPARE(sheet.saved.size(), 3);
        QCOMPARE(sheet.savedFor(s1).note(), QStringLiteral("Слегка засахарен"));
        QVERIFY_EXCEPTION_THROWN(ts.save(m_basket1, evals), DomainError);
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE evaluation SET taste = 10 WHERE taster_id = %1")
                                                      .arg(login.taster.id())),
                                 DbError);   // триггер: корзина завершена

        // оценённые образцы и корзины защищены от изменений
        BasketService baskets(*m_db, *m_audit);
        QVERIFY_EXCEPTION_THROWN(baskets.removeSample(m_basket1, s0), DomainError);
        QVERIFY_EXCEPTION_THROWN(baskets.assignTeam(m_basket1, m_team2), DomainError);
        QVERIFY_EXCEPTION_THROWN(baskets.removeBasket(m_basket1), DomainError);
        QVERIFY_EXCEPTION_THROWN(SampleService(*m_db, *m_audit).removeSample(s0), DomainError);
        QVERIFY_EXCEPTION_THROWN(TeamService(*m_db, *m_audit).removeTeam(m_team1), DomainError);
        QVERIFY_EXCEPTION_THROWN(TeamService(*m_db, *m_audit).removeTaster(login.taster.id()), DomainError);

        // корзина 2: равенство баллов с образцом корзины 1 (63,75)
        sheet = ts.openBasket(m_basket2);
        for (const Sample& s : sheet.samples)
            m_b2Samples << s.id();
        ts.finish(m_basket2, {full(m_b2Samples[0], 60, 65, 65, 65), full(m_b2Samples[1], 50, 50, 50, 50)});
        QVERIFY(ts.allFinished());

        // повторный вход после завершения — только при разрешении (решение В14)
        QVERIFY(!ts.canEnter());
        SessionService ss(*m_db, *m_audit);
        TastingSession session = ss.session();
        session.setAllowReentry(true);
        session.setStartsAt(QDateTime(QDate(2026, 10, 24), QTime(10, 0)));
        ss.saveSession(session);
        QVERIFY(ts.canEnter());
        QCOMPARE(ss.session().startsAt(), QDateTime(QDate(2026, 10, 24), QTime(10, 0)));
    }

    void sessionTakeover()
    {
        AuthService auth(*m_db, *m_audit);
        const QString drafts = m_tmp.filePath(QStringLiteral("drafts"));
        const auto pc1 = auth.loginTaster(QStringLiteral("Иванова2"), m_creds[1].pin);
        TastingService first(*m_db, *m_audit, pc1.taster, pc1.sessionToken, drafts);
        first.save(m_basket1, {full(m_b1Samples[0], 70, 70, 70, 70)});

        // вход с другого компьютера: оценки загружаются с сервера, первый сеанс закрыт
        const auto pc2 = auth.loginTaster(QStringLiteral("иванова2"), m_creds[1].pin);
        TastingService second(*m_db, *m_audit, pc2.taster, pc2.sessionToken, drafts);
        QVERIFY(!first.isSessionCurrent());
        QVERIFY(second.isSessionCurrent());
        QCOMPARE(second.openBasket(m_basket1).saved.size(), 1);
        QVERIFY_EXCEPTION_THROWN(first.save(m_basket1, {full(m_b1Samples[1], 1, 1, 1, 1)}), DomainError);
        QVERIFY(!first.saveCheckpoint(m_basket1, {full(m_b1Samples[1], 1, 1, 1, 1)}));   // только локально

        second.finish(m_basket1, {full(m_b1Samples[1], 60, 60, 60, 60), full(m_b1Samples[2], 100, 100, 100, 100)});
        QVERIFY(second.openBasket(m_basket1).readOnly());
        auth.logoutTaster(pc2.taster.id(), pc2.sessionToken);
        QVERIFY(!second.isSessionCurrent());
    }

    // ----------------------------------------------------------------- итоги
    void resultsAndExport()
    {
        ResultService rs(*m_db, *m_audit);
        const QVector<NominationResult> nominations = rs.compute();
        QCOMPARE(nominations.size(), 3);
        QCOMPARE(nominations[0].name, QStringLiteral("Гречишный"));
        QCOMPARE(nominations[1].name, QStringLiteral("Липовый"));
        const NominationResult& lime = nominations[1];
        QCOMPARE(lime.results.size(), 5);
        QCOMPARE(lime.tasters.size(), 2);

        // средние: 96,875 → 96,88; 75,625 → 75,63; 63,75; 63,75; 50 — места 1, 2, 3, 3, 4
        const QVector<qint64> cents = {9688, 7563, 6375, 6375, 5000};
        const QVector<int> places = {1, 2, 3, 3, 4};
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(*lime.results[i].averageCents(), cents[i]);
            QCOMPARE(lime.results[i].place(), places[i]);
        }
        QCOMPARE(lime.results[0].sample().id(), m_b1Samples[2]);
        QCOMPARE(lime.results[0].completeCount(), 2);
        QCOMPARE(nominations[0].results[0].place(), 0);   // без оценок — без места

        const QString path = m_tmp.filePath(QStringLiteral("Итоги.xlsx"));
        rs.exportXlsx(path);
        QXlsx::Document doc(path);
        QVERIFY(doc.load());
        QCOMPARE(doc.sheetNames(), (QStringList{QStringLiteral("Итоги"), QStringLiteral("Детализация")}));
        doc.selectSheet(QStringLiteral("Итоги"));
        int row = 0;
        for (int r = 1; r <= doc.dimension().lastRow(); ++r)
            if (doc.read(r, 2).toString() == lime.results[0].sample().cipher())
                row = r;
        QVERIFY(row > 0);
        QCOMPARE(doc.read(row, 1).toInt(), 1);
        QCOMPARE(doc.read(row, 3).toString(), lime.results[0].owner().fullName());
        QCOMPARE(doc.read(row, 7).toDouble(), 96.88);
        doc.selectSheet(QStringLiteral("Детализация"));
        QCOMPARE(doc.read(1, 8).toString(), criterionTitle(Criterion::Appearance));
        QVERIFY(doc.dimension().lastRow() >= 1 + 8);   // 8 оценок + строки образцов без оценок
    }

    void monitoring()
    {
        const MonitoringSnapshot snap = MonitoringService(*m_db).snapshot();
        QCOMPARE(snap.samplesTotal, 7);
        QCOMPARE(snap.samplesInBaskets, 6);
        QCOMPARE(snap.basketsTotal, 3);
        QCOMPARE(snap.basketsUnassigned, 1);
        QCOMPARE(snap.teams.size(), 2);
        const TeamProgress& t1 = snap.teams[0];
        QCOMPARE(t1.basketCount, 2);
        QCOMPARE(t1.sampleCount, 5);
        QCOMPARE(t1.tasters.size(), 3);
        QCOMPARE(t1.evaluatedTotal(), 5 + 3);
        QCOMPARE(t1.expectedTotal(), 15);
        QVERIFY(!t1.isFinished());
        for (const TasterProgressRow& r : t1.tasters)
            if (r.tasterName.startsWith(QStringLiteral("Иванова А")))
                QCOMPARE(r.basketsFinished, 2);
    }

    void auditCsv()
    {
        const QString path = m_tmp.filePath(QStringLiteral("journal.csv"));
        const int n = m_audit->exportCsv(path);
        QVERIFY(n > 20);
        const auto rows = CsvReader::readFile(path);
        QCOMPARE(rows.first().first(), QStringLiteral("Дата и время"));
        QCOMPARE(rows.size(), n + 1);
        QStringList actions;
        for (const QStringList& r : rows)
            actions << r.value(4);
        for (const char* a : {"Вход администратора", "Загрузка реестра участников", "Регистрация образцов",
                              "Создание команды", "Сохранение оценок", "Завершение дегустации корзины",
                              "Блокировка входа дегустатора", "Экспорт итоговой ведомости"})
            QVERIFY2(actions.contains(QString::fromUtf8(a)), a);

        // сервер недоступен — запись откладывается в локальный файл и переносится позже
        Database offline(QStringLiteral("sedm_test_offline"));
        AuditService pending(offline, m_tmp.filePath(QStringLiteral("pending.csv")));
        pending.log(QStringLiteral("Проверка буфера"), QStringLiteral("нет связи"));
        QCOMPARE(pending.pendingCount(), 1);
        AuditService online(*m_db, m_tmp.filePath(QStringLiteral("pending.csv")));
        online.flushPending();
        QCOMPARE(online.pendingCount(), 0);
        QCOMPARE(m_db->scalar(QStringLiteral("SELECT count(*) FROM audit_log WHERE action = 'Проверка буфера'")).toInt(), 1);
    }

    // -------------------------------------------------- резервное копирование
    void sqlScriptLexer()
    {
        const QString src = QStringLiteral(
            "\\restrict abc\n"
            "SET transaction_timeout = 0;\nSET lock_timeout = 0;\n"
            "INSERT INTO t VALUES ('строка\n\\не метакоманда');\n"
            "CREATE FUNCTION f() RETURNS int AS $body$\n\\тоже не метакоманда\n$body$;\n"
            "-- комментарий \\x\n/* блок\n\\внутри */\n"
            "  \\connect other\n"
            "\\unrestrict abc\n");
        const QString out = SqlScript::stripPsqlMetaCommands(src);
        QVERIFY(!out.contains(QStringLiteral("\\restrict")));
        QVERIFY(!out.contains(QStringLiteral("\\unrestrict")));
        QVERIFY(!out.contains(QStringLiteral("\\connect")));
        QVERIFY(out.contains(QStringLiteral("\\не метакоманда")));
        QVERIFY(out.contains(QStringLiteral("\\тоже не метакоманда")));
        QVERIFY(out.contains(QStringLiteral("\\внутри")));

        const QStringList statements = SqlScript::splitStatements(
            src + QStringLiteral("SELECT 'a;b', E'it\\'s;', \"col;umn\" FROM t; -- конец; без команды\n;;"));
        QCOMPARE(statements.size(), 5);
        QCOMPARE(statements[0], QStringLiteral("SET transaction_timeout = 0"));
        QCOMPARE(statements[2], QStringLiteral("INSERT INTO t VALUES ('строка\n\\не метакоманда')"));
        QVERIFY(statements[3].startsWith(QLatin1String("CREATE FUNCTION f()")));
        QVERIFY(statements[3].endsWith(QLatin1String("$body$")));
        QCOMPARE(statements[4], QStringLiteral("SELECT 'a;b', E'it\\'s;', \"col;umn\" FROM t"));

        QVERIFY(SqlScript::containsCopyFromStdin(QStringLiteral("SET x = 1;\nCOPY sedm.t (a, b) FROM stdin;\n1\t2\n\\.\n")));
        QVERIFY(!SqlScript::containsCopyFromStdin(QStringLiteral("INSERT INTO t VALUES ('COPY x FROM stdin;');")));

        // ошибка во второй и последующих командах сценария обнаруживается
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("SELECT 1; SELECT * FROM no_such_table; SELECT 2")),
                                 DbError);
    }

    void backupAndRestore()
    {
        BackupService backup(*m_db, m_audit.get());
        const QString safety = m_tmp.filePath(QStringLiteral("safety"));
        const int participants = count(QStringLiteral("participant"));
        const int evaluations = count(QStringLiteral("evaluation"));
        const int maxSeq = m_db->scalar(QStringLiteral("SELECT max(seq_number) FROM sample")).toInt();

        const QString builtin = writeFile(QStringLiteral("builtin.sql"), backup.builtinDump().toUtf8());
        const BackupResult r = backup.createBackup(m_tmp.filePath(QStringLiteral("copy.sql")));
        QVERIFY(QFileInfo(r.path).size() > 1000);
        qInfo().noquote() << "Резервная копия:" << (r.usedPgDump ? "pg_dump" : "встроенный модуль") << r.note;

        RegistryService reg(*m_db, *m_audit);
        reg.addNomination(QStringLiteral("Донниковый"));
        m_db->execScript(QStringLiteral("DELETE FROM score_draft; UPDATE participant SET region = 'Изменено'"));

        backup.restoreBackup(r.path, safety);
        QCOMPARE(count(QStringLiteral("participant")), participants);
        QCOMPARE(count(QStringLiteral("evaluation")), evaluations);
        QCOMPARE(nominationId(QStringLiteral("Донниковый")), 0);
        QCOMPARE(m_db->scalar(QStringLiteral("SELECT count(*) FROM participant WHERE region = 'Изменено'")).toInt(), 0);
        QCOMPARE(BackupService::listBackups(safety).size(), 1);   // страховочная копия
        // правила базы данных действуют и после восстановления
        QVERIFY_EXCEPTION_THROWN(m_db->execScript(QStringLiteral("UPDATE evaluation SET taste = 10")), DbError);
        QCOMPARE(AuthService(*m_db, *m_audit).loginAdmin(QStringLiteral("Новый2026")).status, LoginStatus::Ok);

        // восстановление встроенной копии; последовательности продолжают нумерацию
        reg.addNomination(QStringLiteral("Донниковый"));
        backup.restoreBackup(builtin, safety);
        QCOMPARE(nominationId(QStringLiteral("Донниковый")), 0);
        QCOMPARE(count(QStringLiteral("evaluation")), evaluations);
        const Sample s = SampleService(*m_db, *m_audit).registerSample(participantId(QStringLiteral("Иванов Пётр Сергеевич")),
                                                                         nominationId(QStringLiteral("Липовый")));
        QVERIFY(s.seqNumber() > maxSeq);
        SampleService(*m_db, *m_audit).removeSample(s.id());

        // посторонний файл и повреждённая копия — данные не меняются
        QVERIFY_EXCEPTION_THROWN(backup.restoreBackup(writeFile(QStringLiteral("other.sql"), "SELECT 1;"), safety),
                                 DomainError);
        QByteArray broken = backup.builtinDump().toUtf8();
        broken.replace("-- Последовательности", "INSERT INTO nomination (id, name) VALUES (1, 'дубликат');");
        QVERIFY_EXCEPTION_THROWN(backup.restoreBackup(writeFile(QStringLiteral("broken.sql"), broken), safety),
                                 DomainError);
        QCOMPARE(count(QStringLiteral("participant")), participants);
        QVERIFY(SchemaInstaller::isInstalled(*m_db));

        // автокопии: хранятся 12 последних
        const QString autoDir = m_tmp.filePath(QStringLiteral("auto"));
        QDir().mkpath(autoDir);
        for (int i = 0; i < 14; ++i)
            writeFile(QStringLiteral("auto/sedm_auto_20260101_0000%1.sql").arg(i, 2, 10, QLatin1Char('0')), "--");
        const QString marker = backup.changeMarker();
        backup.createAutoBackup(autoDir);
        QCOMPARE(BackupService::listBackups(autoDir).size(), BackupService::kAutoBackupKeep);
        QVERIFY(!QFile::exists(m_tmp.filePath(QStringLiteral("auto/sedm_auto_20260101_000000.sql"))));
        QCOMPARE(backup.changeMarker(), marker);   // копия не меняет данных
    }

    void newProject()
    {
        SessionService(*m_db, *m_audit).newProject();
        for (const char* t : {"participant", "nomination", "sample", "basket", "team", "taster", "evaluation",
                              "score_draft", "basket_progress"})
            QCOMPARE(count(QString::fromLatin1(t)), 0);
        QCOMPARE(count(QStringLiteral("audit_log")), 1);
        QVERIFY(AuthService(*m_db, *m_audit).adminExists());
        QVERIFY(!SessionService(*m_db, *m_audit).session().allowReentry());

        RegistryService reg(*m_db, *m_audit);
        const Nomination n = reg.addNomination(QStringLiteral("Липовый"));
        Participant p;
        p.setFullName(QStringLiteral("Новиков Олег"));
        p.setRegion(QStringLiteral("Тульская область"));
        p.setNominationId(n.id());
        const Participant saved = reg.addParticipant(p);
        QCOMPARE(SampleService(*m_db, *m_audit).registerSample(saved.id(), n.id()).seqNumber(), 1);
    }
};

QTEST_GUILESS_MAIN(TestServices)
#include "tst_services.moc"
