/**
 * @file sedm_screens.cpp
 * @brief Снимки окон и диалогов СЭДМ для подробных руководств администратора и дегустатора.
 *
 * Программа создаёт на тестовом сервере временную базу (параметры — TestDatabase.h),
 * заполняет её демонстрационными данными, по очереди открывает окна, меню и диалоги
 * программы и сохраняет их изображения в PNG; над изображением рисуется строка
 * заголовка окна. После работы временная база удаляется.
 * Запуск: sedm_screens admin|taster <каталог для снимков>
 */
#include "AboutDialog.h"
#include "AdminWindow.h"
#include "AuditService.h"
#include "AuthService.h"
#include "BasketsPage.h"
#include "ConnectionDialog.h"
#include "CoreResources.h"
#include "Database.h"
#include "DbError.h"
#include "DemoSeeder.h"
#include "EvaluationPage.h"
#include "HelpWindow.h"
#include "HoneySlider.h"
#include "Icons.h"
#include "ImportDialog.h"
#include "Localization.h"
#include "LoginPage.h"
#include "Messages.h"
#include "ParticipantsPage.h"
#include "RegistryService.h"
#include "SampleService.h"
#include "SamplesPage.h"
#include "ScoreRow.h"
#include "SessionService.h"
#include "SetupWizard.h"
#include "TasterWindow.h"
#include "TastingService.h"
#include "TeamService.h"
#include "TeamsPage.h"
#include "TestDatabase.h"
#include "Theme.h"
#include "UiTestUtil.h"
#include "WidgetsResources.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QtTest>
#include <functional>

using namespace sedm;
using sedm::test::buttonWithText;
using sedm::test::TestDatabase;

static void initAdminResources()
{
    Q_INIT_RESOURCE(admin);
}

static void initTasterResources()
{
    Q_INIT_RESOURCE(taster);
}

namespace {

QString g_dir;     ///< каталог снимков
int g_failures = 0;

// ------------------------------------------------------------------ снимки
/// Изображение окна со строкой заголовка (платформа offscreen рамок окон не рисует).
QImage framed(QWidget* w)
{
    const QPixmap body = w->grab();
    const int bar = 32;
    QImage img(body.width() + 2, body.height() + bar + 1, QImage::Format_ARGB32);
    img.fill(QColor(0xB8, 0xAD, 0x9A));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(QRect(1, 1, body.width(), bar - 1), QColor(0xF4, 0xEF, 0xE6));
    p.drawPixmap(8, 7, ui::Icons::appIcon().pixmap(18, 18));
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(QColor(0x24, 0x1A, 0x0E));
    p.drawText(QRect(34, 1, body.width() - 160, bar - 1), Qt::AlignVCenter | Qt::AlignLeft, w->windowTitle());
    // кнопки окна: свернуть, развернуть, закрыть
    const int right = body.width() + 1;
    p.setPen(QPen(QColor(0x4A, 0x40, 0x34), 1));
    const int cy = bar / 2;
    p.drawLine(right - 128, cy, right - 118, cy);
    p.drawRect(right - 82, cy - 5, 10, 10);
    p.drawLine(right - 36, cy - 5, right - 26, cy + 5);
    p.drawLine(right - 36, cy + 5, right - 26, cy - 5);
    p.drawPixmap(1, bar, body);
    return img;
}

void saveImage(const QImage& img, const QString& name)
{
    const QString path = QDir(g_dir).filePath(name + QStringLiteral(".png"));
    if (img.save(path))
        qInfo().noquote() << "снимок" << name;
    else {
        qWarning().noquote() << "НЕ СОХРАНЁН" << path;
        ++g_failures;
    }
}

void shot(QWidget* w, const QString& name)
{
    QTest::qWait(250);   // отложенные события компоновки
    saveImage(framed(w), name);
}

/// Снимок главного окна с раскрытым меню menuTitle (без «&»).
void shotMenu(QMainWindow* w, const QString& menuTitle, const QString& name)
{
    for (QAction* a : w->menuBar()->actions()) {
        if (a->text().remove(QLatin1Char('&')) != menuTitle || !a->menu())
            continue;
        QMenu* menu = a->menu();
        const QRect r = w->menuBar()->actionGeometry(a);
        const QPoint pos = w->menuBar()->mapTo(w, r.bottomLeft());
        menu->popup(w->mapToGlobal(pos));
        QTest::qWait(300);
        QPixmap base = w->grab();
        {
            QPainter p(&base);
            p.fillRect(QRect(w->menuBar()->mapTo(w, r.topLeft()), r.size()), QColor(0xF5, 0xC5, 0x62, 120));
            p.drawPixmap(pos, menu->grab());
            p.setPen(QColor(0xB8, 0xAD, 0x9A));
            p.drawRect(QRect(pos, menu->size()).adjusted(0, 0, -1, -1));
        }
        menu->hide();
        QImage img = framed(w);
        QPainter p(&img);
        p.drawPixmap(1, 32, base);
        p.end();
        // только левая верхняя часть окна: строка заголовка, меню и раскрытый список команд
        const int width = qMin(img.width(), qMax(pos.x() + menu->width() + 120, 520));
        const int height = qMin(img.height(), 32 + pos.y() + menu->height() + 40);
        saveImage(img.copy(0, 0, width, height), name);
        return;
    }
    qWarning().noquote() << "нет меню" << menuTitle;
    ++g_failures;
}

// ------------------------------------------------------- модальные окна
/// Действие с модальным окном: дождаться окна с заголовком title, подготовить,
/// снять (если задано имя снимка) и закрыть кнопкой button.
/// button: текст кнопки; пусто — reject(); «@accept» — accept().
struct Step {
    QString title;
    QString shot;
    std::function<void(QWidget*)> prepare;
    QString button;
};

class ModalScript {
public:
    explicit ModalScript(QVector<Step> steps) : m_steps(std::move(steps))
    {
        m_timer.setInterval(60);
        QObject::connect(&m_timer, &QTimer::timeout, [this]() { tick(); });
        m_timer.start();
        m_idle.start();
    }
    bool done() const { return m_steps.isEmpty() && !m_busy; }

private:
    void tick()
    {
        if (m_busy || m_steps.isEmpty())
            return;
        QWidget* d = QApplication::activeModalWidget();
        if (!d) {
            if (m_idle.elapsed() > 20000) {
                qWarning().noquote() << "окно не появилось:" << m_steps.first().title;
                ++g_failures;
                m_steps.clear();
            }
            return;
        }
        const Step& s = m_steps.first();
        const bool same = d == m_last && d->windowTitle() == m_lastTitle;
        if (same || !d->windowTitle().contains(s.title)) {
            if (m_idle.elapsed() > 20000) {   // ожидаемое окно не появилось — закрыть текущее
                qWarning().noquote() << "ожидалось окно" << s.title << ", открыто" << d->windowTitle();
                ++g_failures;
                m_steps.removeFirst();
                if (auto* dlg = qobject_cast<QDialog*>(d))
                    dlg->reject();
                m_idle.restart();
            }
            return;
        }
        m_busy = true;
        const Step step = m_steps.takeFirst();
        if (step.prepare)
            step.prepare(d);
        QPointer<QWidget> dialog(d);
        QTimer::singleShot(350, [this, dialog, step]() {
            if (!dialog) {
                m_busy = false;
                return;
            }
            if (!step.shot.isEmpty())
                saveImage(framed(dialog), step.shot);
            m_last = dialog;
            m_lastTitle = dialog->windowTitle();
            m_idle.restart();
            m_busy = false;
            // закрытие — из очереди событий: кнопка может открыть следующее окно
            QTimer::singleShot(0, [dialog, step]() {
                if (!dialog)
                    return;
                if (step.button.isEmpty()) {
                    if (auto* dlg = qobject_cast<QDialog*>(dialog.data()))
                        dlg->reject();
                    else
                        dialog->close();
                } else if (step.button == QLatin1String("@accept")) {
                    if (auto* dlg = qobject_cast<QDialog*>(dialog.data()))
                        dlg->accept();
                } else if (QPushButton* b = buttonWithText(dialog, step.button)) {
                    b->click();
                } else {
                    qWarning().noquote() << "нет кнопки" << step.button << "в окне" << dialog->windowTitle();
                    ++g_failures;
                    if (auto* dlg = qobject_cast<QDialog*>(dialog.data()))
                        dlg->reject();
                }
            });
        });
    }

    QVector<Step> m_steps;
    QTimer m_timer;
    QElapsedTimer m_idle;
    QPointer<QWidget> m_last;
    QString m_lastTitle;
    bool m_busy = false;
};

/// Выполнить действие, открывающее модальные окна, и обработать их по шагам.
void modal(const std::function<void()>& action, QVector<Step> steps)
{
    ModalScript script(std::move(steps));
    QTimer::singleShot(0, action);
    if (!QTest::qWaitFor([&]() { return script.done(); }, 60000)) {
        qWarning() << "сценарий окон не завершён";
        ++g_failures;
    }
    QTest::qWait(200);
}

void click(QWidget* root, const QString& text)
{
    if (QPushButton* b = buttonWithText(root, text))
        QTest::mouseClick(b, Qt::LeftButton);
    else {
        qWarning().noquote() << "нет кнопки" << text;
        ++g_failures;
    }
}

/// Нажатие кнопки с модальными окнами: action — щелчок по кнопке text в root.
void clickModal(QWidget* root, const QString& text, QVector<Step> steps)
{
    QPointer<QWidget> r(root);
    modal([r, text]() {
        if (!r)
            return;
        if (QPushButton* b = buttonWithText(r, text))
            b->click();
        else {
            qWarning().noquote() << "нет кнопки" << text;
            ++g_failures;
        }
    }, std::move(steps));
}

void triggerAction(QWidget* w, const QString& text)
{
    for (QAction* a : w->findChildren<QAction*>())
        if (a->text() == text) {
            a->trigger();
            return;
        }
    qWarning().noquote() << "нет команды меню" << text;
    ++g_failures;
}

std::function<void(QWidget*)> chooseFile(const QString& path)
{
    return [path](QWidget* d) {
        if (auto* fd = qobject_cast<QFileDialog*>(d)) {
            fd->setDirectory(QFileInfo(path).absolutePath());
            // каталог читается асинхронно: имя файла — прямо в поле ввода окна
            if (auto* name = fd->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
                name->setText(QFileInfo(path).fileName());
            else
                fd->selectFile(QFileInfo(path).absoluteFilePath());
        }
    };
}

std::function<void(QWidget*)> inputText(const QString& text)
{
    return [text](QWidget* d) {
        if (auto* in = qobject_cast<QInputDialog*>(d))
            in->setTextValue(text);
    };
}

std::function<void(QWidget*)> resizeTo(int w, int h)
{
    return [w, h](QWidget* d) { d->resize(w, h); };
}

QWidget* currentPage(QWidget* window)
{
    return window->findChild<QStackedWidget*>()->currentWidget();
}

QPushButton* nav(QWidget* window, const QString& text)
{
    for (QPushButton* b : window->findChildren<QPushButton*>())
        if (b->property("nav").toBool() && b->accessibleName() == text)
            return b;
    return nullptr;
}

void selectFirstRow(QAbstractItemView* view)
{
    if (view && view->model() && view->model()->rowCount() > 0) {
        view->setCurrentIndex(view->model()->index(0, 0));
        if (auto* t = qobject_cast<QTableWidget*>(view))
            t->selectRow(0);
    }
}

/// Сообщение «нет подключения» в том виде, в каком его показывает программа при запуске.
QString connectionProblem()
{
    ConnectionSettings wrong;
    wrong.port = 1;   // заведомо закрытый порт
    wrong.password = QStringLiteral("x");
    try {
        Database db(QStringLiteral("sedm_screens_wrong"));
        db.open(wrong);
    } catch (const DbError& e) {
        return e.message();
    }
    return QStringLiteral("Сервер базы данных недоступен.");
}

/// Оценки всех образцов корзины (score — функция номера образца и критерия).
QVector<Evaluation> scoresFor(const BasketSheet& sheet, int tasterId, int base)
{
    QVector<Evaluation> list;
    int i = 0;
    for (const Sample& s : sheet.samples) {
        Evaluation e(tasterId, s.id());
        int c = 0;
        for (Criterion criterion : kAllCriteria)
            e.setScore(criterion, qBound(0, base + ((i * 7 + c++ * 5 + tasterId * 3) % 19) - 6, 100));
        list << e;
        ++i;
    }
    return list;
}

// ===================================================================== администратор
int runAdmin()
{
    TestDatabase tdb(QStringLiteral("sedm_guide"));
    tdb.create();
    Database db(QStringLiteral("sedm_guide_conn"));
    db.open(tdb.app());
    QTemporaryDir tmp;
    AuditService audit(db, tmp.filePath(QStringLiteral("pending.csv")));
    AuthService auth(db, audit);
    QDir examples(QDir(g_dir).filePath(QStringLiteral("../Примеры")));
    examples.mkpath(QStringLiteral("."));

    // --- запуск: нет связи с сервером
    modal([]() {
        ui::Messages::choose(nullptr, QStringLiteral("Нет подключения к серверу"), connectionProblem(),
                             {QStringLiteral("Повторить"), QStringLiteral("Параметры подключения…"),
                              QStringLiteral("Первичная настройка…"), QStringLiteral("Выход")}, 0, 3);
    }, {{QStringLiteral("Нет подключения"), QStringLiteral("a01_no_connection"), nullptr, QStringLiteral("Выход")}});

    // --- мастер первичной настройки (параметры по умолчанию)
    {
        admin::SetupWizard wizard(db, ConnectionSettings(), admin::SetupWizard::Start::Choose);
        wizard.show();
        shot(&wizard, QStringLiteral("a02_wizard_choose"));
        click(&wizard, QStringLiteral("Далее →"));
        for (QLineEdit* e : wizard.findChildren<QLineEdit*>())
            if (e->echoMode() == QLineEdit::Password && e->isVisible() && e->text().isEmpty())
                e->setText(QStringLiteral("********"));
        shot(&wizard, QStringLiteral("a03_wizard_install"));
        click(&wizard, QStringLiteral("← Назад"));
        for (QRadioButton* r : wizard.findChildren<QRadioButton*>())
            if (r->text().startsWith(QStringLiteral("Подключиться")))
                r->setChecked(true);
        click(&wizard, QStringLiteral("Далее →"));
        shot(&wizard, QStringLiteral("a04_wizard_connect"));
        wizard.reject();
    }
    {
        admin::SetupWizard wizard(db, db.settings(), admin::SetupWizard::Start::AdminPassword);
        wizard.show();
        for (QLineEdit* e : wizard.findChildren<QLineEdit*>())
            if (e->echoMode() == QLineEdit::Password && e->isVisible())
                e->setText(QStringLiteral("Мёд2026"));
        shot(&wizard, QStringLiteral("a05_wizard_password"));
        click(&wizard, QStringLiteral("Сохранить пароль"));
        shot(&wizard, QStringLiteral("a06_wizard_done"));
        wizard.accept();
    }

    // --- вход администратора
    {
        admin::AdminLoginDialog login(auth);
        login.show();
        shot(&login, QStringLiteral("a07_login"));
        auto* field = login.findChild<QLineEdit*>();
        field->setText(QStringLiteral("неверный1"));
        click(&login, QStringLiteral("Войти"));
        shot(&login, QStringLiteral("a08_login_error"));
        field->setText(QStringLiteral("Мёд2026"));
        click(&login, QStringLiteral("Войти"));
    }

    // --- демонстрационные данные и ход дегустации
    DemoOptions options;
    options.participants = 30;
    options.teams = 4;
    options.tastersPerTeam = 3;
    options.basketSize = 5;
    const DemoResult demo = DemoSeeder::seed(db, audit, options);
    // команда 1 завершила все корзины, команда 2 — сохранила часть оценок, 3 — начала, 4 — не начинала
    for (int i = 0; i < demo.credentials.size(); ++i) {
        const int team = i / options.tastersPerTeam;
        if (team >= 3)
            break;
        const auto login = auth.loginTaster(demo.credentials[i].login, demo.credentials[i].pin);
        if (login.status != LoginStatus::Ok)
            continue;
        TastingService tasting(db, audit, login.taster, login.sessionToken, tmp.filePath(QStringLiteral("drafts")));
        const auto rows = tasting.baskets();
        for (int b = 0; b < rows.size(); ++b) {
            const BasketSheet sheet = tasting.openBasket(rows[b].basket.id());
            QVector<Evaluation> list = scoresFor(sheet, login.taster.id(), 78 + b * 3);
            if (team == 0) {
                tasting.finish(rows[b].basket.id(), list);
            } else if (team == 1) {
                if (b > 0)
                    list.resize(list.size() / 2);
                tasting.save(rows[b].basket.id(), list);
                if (b == 0 && i % 3 != 2)
                    tasting.finish(rows[b].basket.id(), list);
            } else if (b == 0) {
                list.resize(qMin(2, list.size()));
                tasting.save(rows[b].basket.id(), list);
            }
        }
        auth.logoutTaster(login.taster.id(), login.sessionToken);
    }

    admin::AdminWindow win(db, audit);
    win.resize(1366, 768);
    win.show();
    if (!QTest::qWaitForWindowExposed(&win))
        ++g_failures;
    QTest::qWait(500);

    // --- главное окно и меню
    shot(&win, QStringLiteral("a10_main_participants"));
    shotMenu(&win, QStringLiteral("Файл"), QStringLiteral("a11_menu_file"));
    shotMenu(&win, QStringLiteral("Данные"), QStringLiteral("a12_menu_data"));
    shotMenu(&win, QStringLiteral("Дегустация"), QStringLiteral("a13_menu_tasting"));
    shotMenu(&win, QStringLiteral("Сервис"), QStringLiteral("a14_menu_service"));
    shotMenu(&win, QStringLiteral("Справка"), QStringLiteral("a15_menu_help"));

    // --- участники и номинации
    QTest::mouseClick(nav(&win, QStringLiteral("Участники")), Qt::LeftButton);
    auto* participants = qobject_cast<admin::ParticipantsPage*>(currentPage(&win));
    clickModal(participants, QStringLiteral("+ Добавить участника"),
               {{QStringLiteral("Новый участник"), QStringLiteral("a16_participant_new"),
                 [](QWidget* d) {
                     const auto edits = d->findChildren<QLineEdit*>();
                     edits[0]->setText(QStringLiteral("Петрова Анна Сергеевна"));
                     edits[1]->setText(QStringLiteral("+7 915 204-33-18"));
                     edits[2]->setText(QStringLiteral("petrova@mail.ru"));
                     auto* region = d->findChildren<QComboBox*>().first();
                     region->setCurrentText(QStringLiteral("Тульская область"));
                 },
                 QStringLiteral("Сохранить")}});
    selectFirstRow(participants->findChild<QTableView*>());
    clickModal(participants, QStringLiteral("Изменить…"),
               {{QStringLiteral("Изменение данных"), QStringLiteral("a17_participant_edit"), nullptr, QString()}});
    {
        QPushButton* remove = nullptr;   // «Удалить» под таблицей участников (у номинаций — своя)
        for (QPushButton* b : participants->findChildren<QPushButton*>())
            if (b->text() == QStringLiteral("Удалить")
                && (!remove || b->mapTo(participants, QPoint()).x() > remove->mapTo(participants, QPoint()).x()))
                remove = b;
        modal([remove]() { remove->click(); },
              {{QStringLiteral("Удаление участника"), QStringLiteral("a18_participant_remove"), nullptr,
                QStringLiteral("Отмена")}});
    }
    // номинации: добавить
    {
        const auto buttons = participants->findChildren<QPushButton*>();
        QPushButton* add = nullptr;
        for (QPushButton* b : buttons)
            if (b->text() == QStringLiteral("Добавить"))
                add = b;
        modal([add]() { add->click(); },
              {{QStringLiteral("Новая номинация"), QStringLiteral("a19_nomination_new"),
                inputText(QStringLiteral("Мёд с пасеки года")), QStringLiteral("@accept")}});
    }
    // поиск и отбор по номинации
    {
        auto* list = participants->findChild<QListWidget*>();
        list->setCurrentRow(2);
        shot(&win, QStringLiteral("a20_participants_nomination"));
        list->setCurrentRow(0);
        auto* search = participants->findChildren<QLineEdit*>().first();
        search->setText(QStringLiteral("Тульская"));
        shot(&win, QStringLiteral("a21_participants_search"));
        search->clear();
    }
    // импорт реестра: файл с правильными строками, ошибкой, повтором и новой номинацией
    {
        const Participant existing = RegistryService(db, audit).participants().first();
        // пример файла реестра сохраняется рядом с руководствами (каталог «Примеры»)
        const QString csv = examples.filePath(QStringLiteral("Пример реестра участников.csv"));
        QFile f(csv);
        f.open(QIODevice::WriteOnly);
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QStringLiteral("ФИО;Телефон;E-mail;Регион;Номинация\n")
            << QStringLiteral("Гончаров Илья Петрович;+7 910 111-22-33;goncharov@mail.ru;Рязанская область;Липовый мёд\n")
            << QStringLiteral("Фомина Ольга Ивановна;+7 920 444-55-66;;Тамбовская область;Гречишный мёд\n")
            << QStringLiteral("Белов Сергей Андреевич;+7 903 777-88-99;belov@yandex.ru;Алтайский край;Падевый мёд\n")
            << QStringLiteral(";+7 900 000-00-00;;Орловская область;Липовый мёд\n")
            << QStringLiteral("Кравец Мария Олеговна;12345;kravets@;Курская область;Цветочный мёд\n")
            << existing.fullName() << ';' << existing.phone() << ";;" << existing.region()
            << QStringLiteral(";Липовый мёд\n");
        f.close();
        modal([participants]() { participants->importRegistry(); },
              {{QStringLiteral("Загрузка реестра"), QStringLiteral("a22_import_preview"),
                [csv](QWidget* d) {
                    qobject_cast<admin::ImportDialog*>(d)->openFile(csv);
                    d->resize(1320, 640);
                },
                QStringLiteral("Загрузить")},
               {QStringLiteral("Реестр загружен"), QStringLiteral("a23_import_done"), nullptr,
                QStringLiteral("Понятно")}});
    }

    // --- образцы
    QTest::mouseClick(nav(&win, QStringLiteral("Образцы")), Qt::LeftButton);
    auto* samples = qobject_cast<admin::SamplesPage*>(currentPage(&win));
    selectFirstRow(samples->findChild<QTableView*>());
    shot(&win, QStringLiteral("a24_samples"));
    clickModal(samples, QStringLiteral("+ Зарегистрировать образец"),
               {{QStringLiteral("Регистрация образца"), QStringLiteral("a25_sample_register"), nullptr,
                 QStringLiteral("Сохранить")},
                {QStringLiteral("Образец зарегистрирован"), QStringLiteral("a26_sample_registered"), nullptr,
                 QStringLiteral("Понятно")}});
    clickModal(samples, QStringLiteral("Образцы для всех участников…"),
               {{QStringLiteral("Образцы для всех"), QStringLiteral("a27_samples_for_all"), nullptr,
                 QStringLiteral("Зарегистрировать")},
                {QStringLiteral("Образцы зарегистрированы"), QStringLiteral("a28_samples_for_all_done"), nullptr,
                 QStringLiteral("Понятно")}});
    {
        auto combos = samples->findChildren<QComboBox*>();
        QComboBox* basketFilter = combos.size() > 1 ? combos[1] : nullptr;
        if (basketFilter)
            basketFilter->setCurrentIndex(1);   // «не распределены»
        selectFirstRow(samples->findChild<QTableView*>());
        shot(&win, QStringLiteral("a29_samples_unassigned"));
        clickModal(samples, QStringLiteral("Добавить в корзину…"),
                   {{QStringLiteral("орзин"), QStringLiteral("a30_sample_to_basket"), nullptr, QString()}});
        clickModal(samples, QStringLiteral("Изменить"),
                   {{QStringLiteral("Образец №"), QStringLiteral("a31_sample_edit"), nullptr, QString()}});
        clickModal(samples, QStringLiteral("Удалить"),
                   {{QStringLiteral("Удаление образца"), QStringLiteral("a32_sample_remove"), nullptr,
                     QStringLiteral("Отмена")}});
        if (basketFilter)
            basketFilter->setCurrentIndex(0);
    }
    clickModal(samples, QStringLiteral("Печать списка"),
               {{QStringLiteral("Печать списка"), QStringLiteral("a33_samples_print"), resizeTo(1100, 760), QString()}});

    // --- корзины
    QTest::mouseClick(nav(&win, QStringLiteral("Корзины")), Qt::LeftButton);
    auto* baskets = qobject_cast<admin::BasketsPage*>(currentPage(&win));
    shot(&win, QStringLiteral("a34_baskets"));
    QString freeNomination;   // номинация, в которой есть нераспределённые образцы
    for (const Sample& s : SampleService(db, audit).samples())
        if (!s.isInBasket())
            for (const Nomination& n : RegistryService(db, audit).nominations())
                if (n.id() == s.nominationId())
                    freeNomination = n.name();
    clickModal(baskets, QStringLiteral("+ Создать корзину"),
               {{QStringLiteral("Новая корзина"), QStringLiteral("a35_basket_new"),
                 [freeNomination](QWidget* d) {
                     for (QComboBox* box : d->findChildren<QComboBox*>()) {
                         const int i = box->findText(freeNomination);
                         if (i >= 0)
                             box->setCurrentIndex(i);
                     }
                 },
                 QStringLiteral("Создать")}});
    shot(&win, QStringLiteral("a36_basket_created"));
    clickModal(baskets, QStringLiteral("Заполнить из пула"),
               {{QStringLiteral("Корзина заполнена"), QStringLiteral("a37_basket_filled"), nullptr,
                 QStringLiteral("Понятно")}});
    shot(&win, QStringLiteral("a38_basket_after_fill"));
    clickModal(baskets, QStringLiteral("Удалить корзину"),
               {{QStringLiteral("Удаление корзины"), QStringLiteral("a39_basket_remove"), nullptr,
                 QStringLiteral("Отмена")}});

    // --- команды и дегустаторы
    QTest::mouseClick(nav(&win, QStringLiteral("Команды")), Qt::LeftButton);
    auto* teams = qobject_cast<admin::TeamsPage*>(currentPage(&win));
    shot(&win, QStringLiteral("a40_teams"));
    clickModal(teams, QStringLiteral("+ Создать команду"),
               {{QStringLiteral("Новая команда"), QStringLiteral("a41_team_new"),
                 [](QWidget* d) {
                     d->findChild<QLineEdit*>()->setText(QStringLiteral("Эксперты"));
                     d->findChild<QPlainTextEdit*>()->setPlainText(
                         QStringLiteral("Медведева Анна Викторовна\nУваров Илья Сергеевич\nКоролёва Ирина"));
                 },
                 QStringLiteral("Создать команду")},
                {QStringLiteral("Логины и PIN-коды"), QStringLiteral("a42_team_credentials"), nullptr,
                 QStringLiteral("Печать карточек…")},
                {QStringLiteral("Печать карточек"), QStringLiteral("a43_cards_print"), resizeTo(1000, 900), QString()},
                {QStringLiteral("Логины и PIN-коды"), QString(), nullptr, QStringLiteral("Готово")}});
    shot(&win, QStringLiteral("a44_team_created"));
    clickModal(teams, QStringLiteral("Добавить дегустатора…"),
               {{QStringLiteral("Новый дегустатор"), QStringLiteral("a45_taster_add"),
                 inputText(QStringLiteral("Лаврова Нина Петровна")), QStringLiteral("@accept")},
                {QStringLiteral("Логины и PIN-коды"), QString(), nullptr, QStringLiteral("Готово")},
                {QStringLiteral("Карточки не распечатаны"), QStringLiteral("a46_cards_not_printed"), nullptr,
                 QStringLiteral("Закрыть")}});
    {
        auto tables = teams->findChildren<QTableWidget*>();
        if (tables.size() > 1)
            selectFirstRow(tables[1]);   // дегустаторы команды
    }
    clickModal(teams, QStringLiteral("Изменить ФИО…"),
               {{QStringLiteral("Изменение ФИО"), QStringLiteral("a47_taster_rename"), nullptr, QString()}});
    clickModal(teams, QStringLiteral("Новый PIN-код…"),
               {{QStringLiteral("Новый PIN-код"), QStringLiteral("a48_taster_pin"), nullptr, QStringLiteral("Отмена")}});
    clickModal(teams, QStringLiteral("Назначить корзины…"),
               {{QStringLiteral("орзин"), QStringLiteral("a49_team_assign"),
                 [](QWidget* d) {
                     if (auto* list = d->findChild<QListWidget*>())
                         if (list->count() > 0)
                             list->item(0)->setCheckState(Qt::Checked);
                 },
                 QString()}});
    clickModal(teams, QStringLiteral("Карточки дегустаторов…"),
               {{QStringLiteral("Карточки дегустаторов"), QStringLiteral("a50_cards_select"), nullptr, QString()}});

    // --- сеанс, мониторинг, итоги
    QTest::mouseClick(nav(&win, QStringLiteral("Сеанс дегустации")), Qt::LeftButton);
    shot(&win, QStringLiteral("a51_session"));
    QTest::mouseClick(nav(&win, QStringLiteral("Мониторинг")), Qt::LeftButton);
    shot(&win, QStringLiteral("a52_monitoring"));
    QTest::mouseClick(nav(&win, QStringLiteral("Итоги и ведомость")), Qt::LeftButton);
    shot(&win, QStringLiteral("a53_results"));
    const QString xlsx = examples.filePath(QStringLiteral("Пример итоговой ведомости.xlsx"));
    QFile::remove(xlsx);
    clickModal(currentPage(&win), QStringLiteral("Экспорт ведомости XLSX…"),
               {{QStringLiteral("Экспорт итоговой"), QStringLiteral("a54_results_export_file"), chooseFile(xlsx),
                 QStringLiteral("@accept")},
                {QStringLiteral("Ведомость сформирована"), QStringLiteral("a55_results_exported"), nullptr,
                 QStringLiteral("Не открывать")}});

    // --- резервные копии
    QTest::mouseClick(nav(&win, QStringLiteral("Резервные копии")), Qt::LeftButton);
    click(currentPage(&win), QStringLiteral("Создать автокопию сейчас"));
    QTest::qWait(4000);
    shot(&win, QStringLiteral("a56_backup"));
    const QString dump = examples.filePath(QStringLiteral("sedm_копия.sql"));
    clickModal(currentPage(&win), QStringLiteral("Создать резервную копию…"),
               {{QStringLiteral("Резервная копия базы"), QString(), chooseFile(dump), QStringLiteral("@accept")},
                {QStringLiteral("Резервная копия создана"), QStringLiteral("a57_backup_created"), nullptr,
                 QStringLiteral("Понятно")}});
    // восстановление — из выбранной автокопии (окно выбора файла то же, что при экспорте)
    {
        auto tables = currentPage(&win)->findChildren<QTableWidget*>();
        if (!tables.isEmpty())
            selectFirstRow(tables.first());
    }
    clickModal(currentPage(&win), QStringLiteral("Восстановить выбранную…"),
               {{QStringLiteral("Восстановление базы"), QStringLiteral("a58_restore_confirm"), nullptr,
                 QStringLiteral("Отмена")}});

    // копии и ведомости с именами по умолчанию нужны только для снимков
    for (const QFileInfo& f : examples.entryInfoList({QStringLiteral("*.sql"), QStringLiteral("Итоговая ведомость*")}))
        QFile::remove(f.absoluteFilePath());

    // --- сервис, файл, справка
    modal([&win]() { triggerAction(&win, QStringLiteral("Журнал действий…")); },
          {{QStringLiteral("Журнал действий"), QStringLiteral("a60_audit"), nullptr, QStringLiteral("Закрыть")}});
    modal([&win]() { triggerAction(&win, QStringLiteral("Подключение к БД…")); },
          {{QStringLiteral("Подключение к серверу"), QStringLiteral("a61_connection"),
            [](QWidget* d) {
                if (QPushButton* b = buttonWithText(d, QStringLiteral("Проверить подключение")))
                    b->click();
            },
            QString()}});
    modal([&win]() { triggerAction(&win, QStringLiteral("Сменить пароль администратора…")); },
          {{QStringLiteral("Смена пароля"), QStringLiteral("a62_change_password"), nullptr, QString()}});
    modal([&win]() { triggerAction(&win, QStringLiteral("Новый проект…")); },
          {{QStringLiteral("Новый проект"), QStringLiteral("a63_new_project"), nullptr, QStringLiteral("Отмена")}});
    modal([&win]() { triggerAction(&win, QStringLiteral("О программе")); },
          {{QStringLiteral("О программе"), QStringLiteral("a64_about"), nullptr, QStringLiteral("Закрыть")}});
    QTest::mouseClick(nav(&win, QStringLiteral("Образцы")), Qt::LeftButton);
    triggerAction(&win, QStringLiteral("Содержание справки"));
    QTest::qWait(500);
    for (QWidget* w : QApplication::topLevelWidgets())
        if (qobject_cast<ui::HelpWindow*>(w) && w->isVisible()) {
            shot(w, QStringLiteral("a65_help"));
            w->close();
        }

    tdb.drop();
    return g_failures;
}

// ===================================================================== дегустатор
QPushButton* basketCard(QWidget* window, bool finished)
{
    for (QPushButton* b : window->findChildren<QPushButton*>())
        if (b->property("basket").toBool() && b->isVisible() && b->property("finished").toBool() == finished)
            return b;
    return nullptr;
}

void loginUi(taster::TasterWindow& win, const QString& surname, const QString& pin, bool submit = true)
{
    auto* page = win.findChild<taster::LoginPage*>();
    page->findChild<QLineEdit*>(QStringLiteral("loginField"))->setText(surname);
    page->findChild<QLineEdit*>(QStringLiteral("pinField"))->clear();
    for (QChar d : pin)
        QTest::mouseClick(buttonWithText(page, QString(d)), Qt::LeftButton);
    if (submit)
        QTest::mouseClick(buttonWithText(page, QStringLiteral("Войти")), Qt::LeftButton);
}

/// Поставить на текущем образце четыре оценки ползунками (End, затем PageDown steps раз).
void rateCurrent(QWidget* eval, int downSteps, int criteria = 4)
{
    int k = 0;
    for (taster::ScoreRow* row : eval->findChildren<taster::ScoreRow*>()) {
        if (k++ >= criteria)
            break;
        auto* s = row->findChild<ui::HoneySlider*>();
        s->setFocus();
        QTest::keyClick(s, Qt::Key_End);
        for (int i = 0; i < downSteps + k % 2; ++i)
            QTest::keyClick(s, Qt::Key_PageDown);
        QTest::keyClick(s, Qt::Key_Left);
    }
}

int runTaster()
{
    TestDatabase tdb(QStringLiteral("sedm_guide_t"));
    tdb.create();
    Database db(QStringLiteral("sedm_guide_t_conn"));
    db.open(tdb.app());
    QTemporaryDir tmp;
    AuditService audit(db, tmp.filePath(QStringLiteral("pending.csv")));
    AuthService auth(db, audit);

    // --- первый запуск: окно «Подключение к серверу» и сообщение о нет связи
    modal([]() {
        ui::ConnectionDialog dlg(ConnectionSettings{});
        dlg.exec();
    }, {{QStringLiteral("Подключение к серверу"), QStringLiteral("t01_connection"),
         [](QWidget* d) {
             const auto edits = d->findChildren<QLineEdit*>();
             if (!edits.isEmpty())
                 edits.first()->setText(QStringLiteral("192.168.1.10"));
         },
         QString()}});
    modal([]() {
        ui::Messages::choose(nullptr, QStringLiteral("Нет подключения к серверу"), connectionProblem(),
                             {QStringLiteral("Повторить"), QStringLiteral("Параметры подключения…"),
                              QStringLiteral("Выход")}, 0, 2);
    }, {{QStringLiteral("Нет подключения"), QStringLiteral("t02_no_connection"), nullptr, QStringLiteral("Выход")}});

    DemoOptions options;
    options.participants = 16;
    options.teams = 2;
    options.tastersPerTeam = 3;
    options.basketSize = 4;
    const DemoResult demo = DemoSeeder::seed(db, audit, options);
    // команда без корзин — для снимка «корзины не назначены»
    const auto spare = TeamService(db, audit).createTeam(9, QStringLiteral("Резерв"),
                                                          {QStringLiteral("Широкова Вера")});
    const TasterCredentials a = demo.credentials[0];   // основной сценарий
    const TasterCredentials b = demo.credentials[1];   // черновик
    const TasterCredentials c = demo.credentials[2];   // вход с другого компьютера
    const TasterCredentials locked = demo.credentials[3];

    taster::TasterWindow win(db);
    win.resize(1366, 768);
    win.show();
    if (!QTest::qWaitForWindowExposed(&win))
        ++g_failures;
    QTest::qWait(400);

    // --- вход
    shot(&win, QStringLiteral("t03_login"));
    loginUi(win, a.login, a.pin.left(2), false);
    shot(&win, QStringLiteral("t04_login_typing"));
    QString wrong = a.pin;
    wrong[3] = QChar('0' + (wrong[3].digitValue() + 1) % 10);
    loginUi(win, a.login, wrong);
    shot(&win, QStringLiteral("t05_login_error"));
    for (int i = 0; i < 5; ++i) {
        QString w = locked.pin;
        w[0] = QChar('0' + (w[0].digitValue() + 1 + i % 2) % 10);
        loginUi(win, locked.login, w);
    }
    shot(&win, QStringLiteral("t06_login_locked"));
    QTest::qWait(61000);   // блокировка 60 с
    shotMenu(&win, QStringLiteral("Файл"), QStringLiteral("t07_menu_file"));
    shotMenu(&win, QStringLiteral("Вид"), QStringLiteral("t08_menu_view"));
    shotMenu(&win, QStringLiteral("Справка"), QStringLiteral("t09_menu_help"));

    // --- корзины и оценивание
    loginUi(win, a.login, a.pin);
    shot(&win, QStringLiteral("t10_baskets"));
    QTest::mouseClick(basketCard(&win, false), Qt::LeftButton);
    QWidget* eval = currentPage(&win);
    shot(&win, QStringLiteral("t11_eval_empty"));
    rateCurrent(eval, 1);
    shot(&win, QStringLiteral("t12_eval_one_scored"));
    click(eval, QStringLiteral("Следующий →"));
    rateCurrent(eval, 2, 2);   // частично: две оценки
    eval->findChild<QPlainTextEdit*>()->setPlainText(QStringLiteral("Лёгкая горчинка в послевкусии"));
    shot(&win, QStringLiteral("t13_eval_partial"));
    click(eval, QStringLiteral("Сохранить"));
    shot(&win, QStringLiteral("t14_eval_saved"));
    clickModal(eval, QStringLiteral("Завершить корзину"),
               {{QStringLiteral("не оценена полностью"), QStringLiteral("t15_finish_incomplete"), nullptr,
                 QStringLiteral("Понятно")}});
    rateCurrent(eval, 2);
    clickModal(eval, QStringLiteral("← К списку корзин"),
               {{QStringLiteral("Несохранённые"), QStringLiteral("t16_leave_unsaved"), nullptr,
                 QStringLiteral("Сохранить")}});
    shot(&win, QStringLiteral("t17_baskets_progress"));

    // завершить первую корзину
    QTest::mouseClick(basketCard(&win, false), Qt::LeftButton);
    eval = currentPage(&win);
    for (int guard = 0; guard < 20; ++guard) {
        rateCurrent(eval, 1 + guard % 3);
        QPushButton* next = buttonWithText(eval, QStringLiteral("Следующий →"));
        if (!next || !next->isEnabled())
            break;
        QTest::mouseClick(next, Qt::LeftButton);
    }
    clickModal(eval, QStringLiteral("Завершить корзину"),
               {{QStringLiteral("Завершение дегустации"), QStringLiteral("t18_finish_confirm"), nullptr,
                 QStringLiteral("Да, завершить")},
                {QStringLiteral("Корзина завершена"), QStringLiteral("t19_finish_done"), nullptr,
                 QStringLiteral("Понятно")}});
    shot(&win, QStringLiteral("t20_baskets_finished"));
    QTest::mouseClick(basketCard(&win, true), Qt::LeftButton);
    shot(&win, QStringLiteral("t21_review"));
    click(currentPage(&win), QStringLiteral("← К списку корзин"));

    // крупный шрифт и справка
    QTest::mouseClick(basketCard(&win, false), Qt::LeftButton);
    click(&win, QStringLiteral("A+"));
    click(&win, QStringLiteral("A+"));
    shot(&win, QStringLiteral("t22_eval_large_font"));
    click(&win, QStringLiteral("A−"));
    click(&win, QStringLiteral("A−"));
    triggerAction(&win, QStringLiteral("Содержание справки"));
    QTest::qWait(500);
    for (QWidget* w : QApplication::topLevelWidgets())
        if (qobject_cast<ui::HelpWindow*>(w) && w->isVisible()) {
            shot(w, QStringLiteral("t23_help"));
            w->close();
        }
    modal([&win]() { triggerAction(&win, QStringLiteral("О программе")); },
          {{QStringLiteral("О программе"), QStringLiteral("t24_about"), nullptr, QStringLiteral("Закрыть")}});
    click(currentPage(&win), QStringLiteral("← К списку корзин"));
    click(currentPage(&win), QStringLiteral("Выйти"));

    // --- все корзины завершены: повторный вход запрещён, затем разрешён
    {
        const auto login = auth.loginTaster(a.login, a.pin);
        TastingService tasting(db, audit, login.taster, login.sessionToken, tmp.filePath(QStringLiteral("d")));
        for (const TasterBasketRow& r : tasting.baskets())
            if (!r.finished)
                tasting.finish(r.basket.id(), scoresFor(tasting.openBasket(r.basket.id()), login.taster.id(), 80));
        auth.logoutTaster(login.taster.id(), login.sessionToken);
    }
    loginUi(win, a.login, a.pin);
    shot(&win, QStringLiteral("t25_work_finished"));
    {
        SessionService sessions(db, audit);
        TastingSession s = sessions.session();
        s.setAllowReentry(true);
        sessions.saveSession(s);
    }
    loginUi(win, a.login, a.pin);
    shot(&win, QStringLiteral("t26_review_reentry"));
    click(currentPage(&win), QStringLiteral("Выйти"));

    // --- черновик контрольной точки (как после сбоя программы)
    {
        const auto login = auth.loginTaster(b.login, b.pin);
        TastingService tasting(db, audit, login.taster, login.sessionToken, tmp.filePath(QStringLiteral("d")));
        const auto rows = tasting.baskets();
        BasketSheet sheet = tasting.openBasket(rows.first().basket.id());
        QVector<Evaluation> draft = scoresFor(sheet, login.taster.id(), 84);
        draft.resize(2);
        tasting.saveCheckpoint(rows.first().basket.id(), draft);
        auth.logoutTaster(login.taster.id(), login.sessionToken);
    }
    loginUi(win, b.login, b.pin);
    modal([&win]() { QTest::mouseClick(basketCard(&win, false), Qt::LeftButton); },
          {{QStringLiteral("Найден черновик"), QStringLiteral("t27_draft_found"), nullptr,
            QStringLiteral("Восстановить")}});
    shot(&win, QStringLiteral("t28_draft_restored"));
    clickModal(currentPage(&win), QStringLiteral("← К списку корзин"),
               {{QStringLiteral("Несохранённые"), QString(), nullptr, QStringLiteral("Не сохранять")}});
    click(currentPage(&win), QStringLiteral("Выйти"));

    // --- команда без корзин
    if (!spare.isEmpty()) {
        loginUi(win, spare.first().login, spare.first().pin);
        shot(&win, QStringLiteral("t29_no_baskets"));
        click(currentPage(&win), QStringLiteral("Выйти"));
    }

    // --- вход под той же фамилией на другом компьютере (последний сценарий: окно не закрывается)
    loginUi(win, c.login, c.pin);
    QTest::mouseClick(basketCard(&win, false), Qt::LeftButton);
    eval = currentPage(&win);
    rateCurrent(eval, 1);
    {
        const auto other = auth.loginTaster(c.login, c.pin);   // второй вход — прежний сеанс недействителен
        Q_UNUSED(other);
    }
    modal([eval]() {
        if (QPushButton* s = buttonWithText(eval, QStringLiteral("Сохранить")))
            s->click();
    }, {{QString(), QStringLiteral("t30_session_taken"), nullptr, QString()}});
    shot(&win, QStringLiteral("t31_after_session_taken"));

    tdb.drop();
    return g_failures;
}

}  // namespace

/// Журнал работы — в файл sedm_screens.log рядом со снимками (консоль Windows может отсутствовать).
void logToFile(QtMsgType type, const QMessageLogContext&, const QString& text)
{
    static QFile file(QDir(g_dir).filePath(QStringLiteral("sedm_screens.log")));
    if (!file.isOpen())
        file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    const char* kind = type == QtWarningMsg                            ? "ПРЕДУПРЕЖДЕНИЕ "
                       : type == QtCriticalMsg || type == QtFatalMsg ? "ОШИБКА "
                                                                     : "";
    file.write(QByteArray(kind) + text.toUtf8() + '\n');
    file.flush();
}

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SEDM"));
    QStandardPaths::setTestModeEnabled(true);   // настройки и черновики — не в рабочем каталоге
    if (argc < 3 || !TestDatabase::configured()) {
        qCritical("Запуск: sedm_screens admin|taster <каталог>; нужен SEDM_TEST_PGSUPERPASS");
        return 2;
    }
    const QString mode = QString::fromLocal8Bit(argv[1]);
    g_dir = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(g_dir);
    QFile::remove(QDir(g_dir).filePath(QStringLiteral("sedm_screens.log")));
    qInstallMessageHandler(logToFile);
    initCoreResources();
    ui::initWidgetsResources();
    ui::installRussian(app);
    QApplication::setWindowIcon(ui::Icons::appIcon());
    if (mode == QLatin1String("admin")) {
        initAdminResources();
        QApplication::setApplicationName(QStringLiteral("СЭДМ — администратор"));
        ui::Theme::apply(app, 14);
        return runAdmin();
    }
    initTasterResources();
    QApplication::setApplicationName(QStringLiteral("СЭДМ — дегустатор"));
    ui::Theme::apply(app, 18, 1.0, ui::Theme::resourceText(QStringLiteral(":/taster/taster.qss")));
    return runTaster();
}
