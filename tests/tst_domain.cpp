/**
 * @file tst_domain.cpp
 * @brief Модульные тесты классов предметной области СЭДМ (Qt Test).
 *
 * Проверяются правила ТЗ: шкала 0–100 и полнота оценок, правила корзин,
 * завершение дегустации корзины, расчёт среднего балла и мест (1, 2, 2, 3),
 * уникальность шифров, логины и PIN-коды, bcrypt, защита от подбора.
 */
#include "Account.h"
#include "Basket.h"
#include "BasketProgress.h"
#include "CipherGenerator.h"
#include "DomainError.h"
#include "Evaluation.h"
#include "LoginGenerator.h"
#include "Participant.h"
#include "PasswordHasher.h"
#include "PinGenerator.h"
#include "RankingCalculator.h"
#include "Sample.h"
#include "TastingSession.h"
#include "Taster.h"

#include <QtTest>

using namespace sedm;

namespace {

Evaluation makeEvaluation(int tasterId, int sampleId, int a, int c, int r, int t)
{
    Evaluation e(tasterId, sampleId);
    e.setScore(Criterion::Appearance, a);
    e.setScore(Criterion::Consistency, c);
    e.setScore(Criterion::Aroma, r);
    e.setScore(Criterion::Taste, t);
    return e;
}

Sample makeSample(int id, int nominationId, const QString& cipher, int seq = 1)
{
    Sample s;
    s.setId(id);
    s.setNominationId(nominationId);
    s.setCipher(cipher);
    s.setSeqNumber(seq);
    return s;
}

SampleResult makeResult(int seq, const QVector<Evaluation>& evaluations)
{
    SampleResult result(makeSample(seq, 1, QString::number(1000 + seq), seq), Participant(), "Липовый");
    for (const Evaluation& e : evaluations)
        result.addScore({e.tasterId(), QString(), 1, e});
    return result;
}

}  // namespace

class TestDomain : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- оценки
    void evaluationScoreRange_data()
    {
        QTest::addColumn<int>("value");
        QTest::addColumn<bool>("valid");
        QTest::newRow("ниже шкалы") << -1 << false;
        QTest::newRow("граница 0") << 0 << true;
        QTest::newRow("середина") << 50 << true;
        QTest::newRow("граница 100") << 100 << true;
        QTest::newRow("выше шкалы") << 101 << false;
    }
    void evaluationScoreRange()
    {
        QFETCH(int, value);
        QFETCH(bool, valid);
        Evaluation e(1, 1);
        if (valid) {
            e.setScore(Criterion::Taste, value);
            QCOMPARE(*e.score(Criterion::Taste), value);
        } else {
            QVERIFY_EXCEPTION_THROWN(e.setScore(Criterion::Taste, value), DomainError);
            QVERIFY(!e.score(Criterion::Taste).has_value());
        }
    }

    void evaluationTotalIsMeanOfFour()
    {
        Evaluation e(1, 1);
        e.setScore(Criterion::Appearance, 85);
        e.setScore(Criterion::Consistency, 78);
        e.setScore(Criterion::Aroma, 84);
        QVERIFY(!e.isComplete());
        QVERIFY(!e.total().has_value());           // нет итога при 3 оценках из 4
        e.setScore(Criterion::Taste, 83);
        QVERIFY(e.isComplete());
        QCOMPARE(*e.totalTimesFour(), 330);
        QCOMPARE(*e.total(), 82.5);
    }

    // ---------------------------------------------------------------- корзины
    void basketRules()
    {
        Basket b;
        b.setNumber(3);
        b.setNominationId(7);
        b.setDesiredCount(2);
        const Sample s1 = makeSample(1, 7, "4827");
        const Sample s2 = makeSample(2, 7, "6802");
        const Sample s3 = makeSample(3, 7, "3390");
        const Sample alien = makeSample(4, 8, "1593");
        Sample elsewhere = makeSample(5, 7, "7205");
        elsewhere.setBasketId(99);

        QString reason;
        QVERIFY(!b.canAccept(alien, &reason));       // чужая номинация
        QVERIFY(reason.contains("другой номинации"));
        QVERIFY(!b.canAccept(elsewhere, &reason));   // уже в другой корзине
        b.addSample(s1);
        QVERIFY_EXCEPTION_THROWN(b.addSample(s1), DomainError);  // повтор
        b.addSample(s2);
        QVERIFY(b.isFull());
        QVERIFY_EXCEPTION_THROWN(b.addSample(s3), DomainError);  // сверх желаемого
        QVERIFY_EXCEPTION_THROWN(b.setDesiredCount(1), DomainError);
        QVERIFY_EXCEPTION_THROWN(b.setNominationId(8), DomainError);  // не пустая
        b.removeSample(1);
        QCOMPARE(b.freeSlots(), 1);
        QVERIFY_EXCEPTION_THROWN(b.removeSample(1), DomainError);
    }

    void finishBasketRequiresAllScores()
    {
        Basket b;
        b.setNumber(1);
        b.setNominationId(1);
        b.setDesiredCount(2);
        b.addSample(makeSample(1, 1, "4827"));
        b.addSample(makeSample(2, 1, "6802"));

        Evaluation partial(10, 2);
        partial.setScore(Criterion::Taste, 90);
        QVector<Evaluation> evaluations{makeEvaluation(10, 1, 85, 78, 84, 83), partial};

        BasketProgress progress(10, 1);
        QCOMPARE(BasketProgress::incompleteSampleIds(b, evaluations), QVector<int>{2});
        QVERIFY_EXCEPTION_THROWN(progress.finish(b, evaluations, QDateTime::currentDateTime()), DomainError);
        QVERIFY(!progress.isFinished());

        evaluations[1] = makeEvaluation(10, 2, 70, 70, 70, 70);
        progress.finish(b, evaluations, QDateTime::currentDateTime());
        QVERIFY(progress.isFinished());
        QVERIFY_EXCEPTION_THROWN(progress.finish(b, evaluations, QDateTime::currentDateTime()), DomainError);
    }

    // ---------------------------------------------------------- баллы и места
    void averageRoundsToCents()
    {
        // итоговые баллы 82,50 и 76,25 -> среднее 79,375 -> 79,38 (половина вверх)
        const SampleResult r = makeResult(1, {makeEvaluation(1, 1, 85, 78, 84, 83),
                                              makeEvaluation(2, 1, 80, 72, 75, 78)});
        QCOMPARE(r.completeCount(), 2);
        QCOMPARE(*r.averageCents(), 7938);
        QCOMPARE(*r.average(), 79.38);
    }

    void incompleteScoresAreNotCounted()
    {
        Evaluation partial(3, 1);
        partial.setScore(Criterion::Taste, 10);
        const SampleResult r = makeResult(1, {makeEvaluation(1, 1, 90, 90, 90, 90), partial});
        QCOMPARE(r.completeCount(), 1);
        QCOMPARE(*r.average(), 90.0);
    }

    void denseRanking()
    {
        QVector<SampleResult> results{
            makeResult(1, {makeEvaluation(1, 1, 80, 80, 80, 80)}),   // 80
            makeResult(2, {makeEvaluation(1, 2, 90, 90, 90, 90)}),   // 90
            makeResult(3, {}),                                        // без оценок
            makeResult(4, {makeEvaluation(1, 4, 80, 80, 80, 80)}),   // 80
            makeResult(5, {makeEvaluation(1, 5, 70, 70, 70, 70)}),   // 70
        };
        RankingCalculator::assignPlaces(results);
        QCOMPARE(results[0].sample().seqNumber(), 2);
        QCOMPARE(results[0].place(), 1);
        QCOMPARE(results[1].place(), 2);   // № 1 и № 4 делят второе место
        QCOMPARE(results[2].place(), 2);
        QCOMPARE(results[3].place(), 3);   // следующее место — 3 (не 4)
        QCOMPARE(results[4].sample().seqNumber(), 3);
        QCOMPARE(results[4].place(), 0);   // без оценок — без места, в конце
    }

    // ---------------------------------------------------------------- шифры
    void cipherFormat()
    {
        QVERIFY(Sample::isValidCipher("1000"));
        QVERIFY(Sample::isValidCipher("9999"));
        QVERIFY(!Sample::isValidCipher("0999"));
        QVERIFY(!Sample::isValidCipher("999"));
        QVERIFY(!Sample::isValidCipher("12a4"));
        Sample s;
        QVERIFY_EXCEPTION_THROWN(s.setCipher("12345"), DomainError);
    }

    void ciphersAreUnique()
    {
        CipherGenerator gen;
        QSet<QString> seen;
        for (int i = 0; i < 500; ++i) {
            const QString c = gen.next();
            QVERIFY(Sample::isValidCipher(c));
            QVERIFY2(!seen.contains(c), qPrintable("повтор шифра " + c));
            seen.insert(c);
        }
        QCOMPARE(gen.remaining(), CipherGenerator::kCapacity - 500);
    }

    void cipherGeneratorFillsAllCodes()
    {
        // почти заполненное множество: генератор переходит к выбору из свободных
        QSet<QString> used;
        for (int code = 1000; code <= 9989; ++code)
            used.insert(QString::number(code));
        CipherGenerator gen(used);
        QSet<QString> rest;
        for (int i = 0; i < 10; ++i)
            rest.insert(gen.next());
        QCOMPARE(rest.size(), 10);
        QVERIFY_EXCEPTION_THROWN(gen.next(), DomainError);
    }

    // -------------------------------------------------- логины и PIN-коды
    void loginsFromSurnames()
    {
        QCOMPARE(LoginGenerator::makeLogin("иванова мария петровна", {}), QString("Иванова"));
        QCOMPARE(LoginGenerator::makeLogin("Иванова Анна", {"Иванова"}), QString("Иванова2"));
        QCOMPARE(LoginGenerator::makeLogin("Иванова Ольга", {"ИВАНОВА", "Иванова2"}), QString("Иванова3"));
        QCOMPARE(LoginGenerator::makeLogin("Ёлкина Вера", {"Елкина"}), QString("Ёлкина2"));
        QCOMPARE(LoginGenerator::normalizedForLookup("  Ёлкина2 "), QString("елкина2"));
    }

    void pinCodes()
    {
        for (const char* trivial : {"0000", "1111", "1234", "4321", "6789", "9876"})
            QVERIFY(PinGenerator::isTrivial(trivial));
        QVERIFY(!PinGenerator::isTrivial("1357"));
        QVERIFY(!PinGenerator::isWellFormed("12a4"));
        for (int i = 0; i < 200; ++i) {
            const QString pin = PinGenerator::generate();
            QVERIFY(PinGenerator::isWellFormed(pin));
            QVERIFY(!PinGenerator::isTrivial(pin));
        }
    }

    // ---------------------------------------------------------------- bcrypt
    void bcryptHashAndVerify()
    {
        const QString hash = PasswordHasher::hash("2580", 4);  // малая трудоёмкость — быстрый тест
        QCOMPARE(hash.size(), 60);
        QVERIFY(hash.startsWith("$2b$04$"));
        QVERIFY(PasswordHasher::verify("2580", hash));
        QVERIFY(!PasswordHasher::verify("2581", hash));
        QVERIFY(!PasswordHasher::verify("2580", "не хэш"));
        QVERIFY(PasswordHasher::hash("2580", 4) != hash);    // разная соль
    }

    void bcryptKnownVector()
    {
        // контрольный пример из набора тестов OpenBSD/crypt_blowfish
        QVERIFY(PasswordHasher::verify("U*U", "$2a$05$CCCCCCCCCCCCCCCCCCCCC.E5YPO9kmyuRGyh0XouQYb4YMJKvyOeW"));
    }

    // ------------------------------------------------- защита от подбора
    void accountLockout()
    {
        Taster t;
        const QDateTime now = QDateTime::currentDateTime();
        for (int i = 0; i < Account::kMaxFailedAttempts - 1; ++i)
            t.registerFailedAttempt(now);
        QVERIFY(!t.isLockedAt(now));
        t.registerFailedAttempt(now);                       // пятая ошибка подряд
        QVERIFY(t.isLockedAt(now.addSecs(30)));
        QVERIFY(!t.isLockedAt(now.addSecs(Account::kLockSeconds + 1)));
        t.registerSuccess();
        QVERIFY(!t.isLockedAt(now));
        QCOMPARE(t.failedAttempts(), 0);
    }

    // ------------------------------------------------------- прочие правила
    void participantValidation()
    {
        Participant p;
        p.setFullName("  Гордеев   Павел Викторович ");
        p.setPhone("+7 (912) 345-67-89");
        p.setEmail("gordeev@mail.ru");
        p.setRegion("Краснодарский край");
        p.setNominationId(1);
        QVERIFY2(p.validate().isEmpty(), qPrintable(p.validate().join(' ')));
        QCOMPARE(p.fullName(), QString("Гордеев Павел Викторович"));
        QCOMPARE(p.shortName(), QString("Гордеев П. В."));
        p.setFullName("Гордеев");
        p.setPhone("12-34");
        p.setEmail("gordeev@");
        QCOMPARE(p.validate().size(), 3);
    }

    void reentryRule()
    {
        TastingSession s;
        QVERIFY(s.canTasterEnter(false));
        QVERIFY(!s.canTasterEnter(true));     // все корзины завершены, повторный вход запрещён
        s.setAllowReentry(true);
        QVERIFY(s.canTasterEnter(true));      // только просмотр
    }
};

QTEST_APPLESS_MAIN(TestDomain)
#include "tst_domain.moc"
