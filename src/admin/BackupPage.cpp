/**
 * @file BackupPage.cpp
 * @brief Реализация окна «Резервные копии».
 */
#include "BackupPage.h"
#include "AppPaths.h"
#include "AutoBackupScheduler.h"
#include "BackupService.h"
#include "Messages.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

BackupPage::BackupPage(AdminContext& ctx, AutoBackupScheduler& scheduler, QWidget* parent)
    : AdminPage(ctx, QStringLiteral("Резервные копии"), parent), m_scheduler(scheduler)
{
    setSubtitle(QStringLiteral("SQL-дамп базы данных (формат pg_dump); восстановление — в одной транзакции"));
    auto* restore = addAction(QStringLiteral("Восстановить из копии…"));
    auto* create = addAction(QStringLiteral("Создать резервную копию…"), true, QStringLiteral("backup"));
    connect(restore, &QPushButton::clicked, this, &BackupPage::restoreBackup);
    connect(create, &QPushButton::clicked, this, &BackupPage::createBackup);

    auto* methodCard = new QFrame;
    methodCard->setProperty("card", "true");
    auto* m = new QVBoxLayout(methodCard);
    m->setContentsMargins(18, 14, 18, 14);
    auto* methodTitle = new QLabel(QStringLiteral("Способ копирования"));
    QFont bold = methodTitle->font();
    bold.setBold(true);
    methodTitle->setFont(bold);
    m_method = new QLabel;
    m_method->setWordWrap(true);
    auto* toolsRow = new QHBoxLayout;
    m_toolsDir = new QLineEdit;
    m_toolsDir->setReadOnly(true);
    m_toolsDir->setPlaceholderText(QStringLiteral("определяется автоматически"));
    auto* choose = new QPushButton(QStringLiteral("Выбрать…"));
    auto* reset = new QPushButton(QStringLiteral("Автоматически"));
    toolsRow->addWidget(new QLabel(QStringLiteral("Каталог утилит PostgreSQL:")));
    toolsRow->addWidget(m_toolsDir, 1);
    toolsRow->addWidget(choose);
    toolsRow->addWidget(reset);
    m->addWidget(methodTitle);
    m->addWidget(m_method);
    m->addLayout(toolsRow);
    body()->addWidget(methodCard);

    auto* autoCard = new QFrame;
    autoCard->setProperty("card", "true");
    auto* a = new QVBoxLayout(autoCard);
    a->setContentsMargins(18, 14, 18, 14);
    auto* autoTitle = new QLabel(QStringLiteral("Автоматические копии (контрольные точки)"));
    autoTitle->setFont(bold);
    m_autoStatus = new QLabel;
    m_autoStatus->setWordWrap(true);
    m_autoStatus->setProperty("role", "muted");
    m_autoTable = new QTableWidget;
    m_autoTable->setColumnCount(3);
    m_autoTable->setHorizontalHeaderLabels({QStringLiteral("Создана"), QStringLiteral("Размер"),
                                            QStringLiteral("Файл")});
    setupTable(m_autoTable);
    m_autoTable->setSortingEnabled(false);
    auto* autoButtons = new QHBoxLayout;
    auto* now = new QPushButton(QStringLiteral("Создать автокопию сейчас"));
    auto* restoreSelected = new QPushButton(QStringLiteral("Восстановить выбранную…"));
    auto* openDir = new QPushButton(QStringLiteral("Открыть папку"));
    autoButtons->addWidget(now);
    autoButtons->addWidget(restoreSelected);
    autoButtons->addStretch(1);
    autoButtons->addWidget(openDir);
    a->addWidget(autoTitle);
    a->addWidget(m_autoStatus);
    a->addWidget(m_autoTable, 1);
    a->addLayout(autoButtons);
    body()->addWidget(autoCard, 1);

    connect(choose, &QPushButton::clicked, this, &BackupPage::chooseToolsDir);
    connect(reset, &QPushButton::clicked, this, [this]() {
        AppPaths::settings()->remove(QStringLiteral("tools/pgBinDir"));
        m_scheduler.setPgBinDir(QString());
        refresh();
    });
    connect(now, &QPushButton::clicked, &m_scheduler, &AutoBackupScheduler::runNow);
    connect(restoreSelected, &QPushButton::clicked, this, [this]() {
        const auto items = m_autoTable->selectedItems();
        if (items.isEmpty()) {
            ui::Messages::info(this, QStringLiteral("Копия не выбрана"), QStringLiteral("Выберите копию в таблице."));
            return;
        }
        restoreFrom(m_autoTable->item(items.first()->row(), 2)->data(Qt::UserRole).toString());
    });
    connect(openDir, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_scheduler.directory()));
    });
    connect(&m_scheduler, &AutoBackupScheduler::finished, this, [this]() {
        if (isVisible())
            refresh();
    });
}

QString BackupPage::pgBinDir() const
{
    return AppPaths::settings()->value(QStringLiteral("tools/pgBinDir")).toString();
}

void BackupPage::refresh()
{
    const QString dir = pgBinDir();
    m_toolsDir->setText(QDir::toNativeSeparators(dir));
    const QString pgDump = BackupService::findPgTool(QStringLiteral("pg_dump"), dir);
    if (pgDump.isEmpty()) {
        m_method->setText(QStringLiteral("Утилита pg_dump не найдена — копии создаёт встроенный модуль программы "
                                         "(тот же формат SQL-дампа). Укажите каталог bin PostgreSQL, если утилиты "
                                         "установлены в нестандартное место."));
    } else {
        m_method->setText(QStringLiteral("Копии создаёт утилита %1 (%2). Если она завершится с ошибкой, копию "
                                         "создаст встроенный модуль программы.")
                              .arg(BackupService::toolVersion(pgDump), QDir::toNativeSeparators(pgDump)));
    }
    const QLocale ru(QLocale::Russian);
    const QVector<BackupFileInfo> files = BackupService::listBackups(m_scheduler.directory());
    m_autoTable->setRowCount(files.size());
    for (int r = 0; r < files.size(); ++r) {
        m_autoTable->setItem(r, 0, new QTableWidgetItem(files[r].createdAt.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))));
        m_autoTable->setItem(r, 1, new QTableWidgetItem(ru.formattedDataSize(files[r].size)));
        auto* name = new QTableWidgetItem(QFileInfo(files[r].path).fileName());
        name->setData(Qt::UserRole, files[r].path);
        m_autoTable->setItem(r, 2, name);
    }
    m_autoTable->resizeColumnsToContents();
    m_autoTable->horizontalHeader()->setStretchLastSection(true);
    m_autoStatus->setText(QStringLiteral("Каждые %1 минут, если данные изменились; хранятся %2 последних копий. "
                                         "Последняя автокопия в этом сеансе программы: %3. Каталог: %4")
                              .arg(BackupService::kAutoIntervalMinutes).arg(BackupService::kAutoBackupKeep)
                              .arg(m_scheduler.lastBackup().isValid()
                                       ? m_scheduler.lastBackup().toString(QStringLiteral("HH:mm"))
                                       : QStringLiteral("ещё не создавалась"),
                                   QDir::toNativeSeparators(m_scheduler.directory())));
}

void BackupPage::chooseToolsDir()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Каталог bin PostgreSQL"),
                                                          QStringLiteral("C:/Program Files/PostgreSQL"));
    if (dir.isEmpty())
        return;
    if (BackupService::findPgTool(QStringLiteral("pg_dump"), dir) != QDir(dir).filePath(QStringLiteral("pg_dump.exe"))) {
        ui::Messages::warning(this, QStringLiteral("Утилиты не найдены"),
                              QStringLiteral("В каталоге «%1» нет файла pg_dump.exe.").arg(QDir::toNativeSeparators(dir)));
        return;
    }
    AppPaths::settings()->setValue(QStringLiteral("tools/pgBinDir"), dir);
    m_scheduler.setPgBinDir(dir);
    refresh();
}

void BackupPage::createBackup()
{
    const QString name = QStringLiteral("sedm_%1.sql").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Резервная копия базы данных"), name,
                                                      QStringLiteral("SQL-дамп (*.sql)"));
    if (path.isEmpty())
        return;
    BackupResult result;
    BackupService service(m_ctx.db, &m_ctx.audit);
    service.setPgBinDir(pgBinDir());
    if (!run([&]() { result = service.createBackup(path); }, false))
        return;
    ui::Messages::info(this, QStringLiteral("Резервная копия создана"),
                       QStringLiteral("Копия сохранена в файл «%1».%2").arg(QDir::toNativeSeparators(path),
                           result.note.isEmpty() ? QString() : QStringLiteral("\n") + result.note));
}

void BackupPage::restoreBackup()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Восстановление из резервной копии"),
                                                      m_scheduler.directory(), QStringLiteral("SQL-дамп (*.sql)"));
    if (!path.isEmpty())
        restoreFrom(path);
}

void BackupPage::restoreFrom(const QString& path)
{
    const QFileInfo info(path);
    if (!ui::Messages::confirm(
            this, QStringLiteral("Восстановление базы данных"),
            QStringLiteral("Все текущие данные будут заменены данными из копии «%1» от %2.\n"
                           "Перед восстановлением программа сохранит страховочную копию текущего состояния.\n"
                           "Дегустаторам на время восстановления лучше не сохранять оценки. Продолжить?")
                .arg(info.fileName(), info.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm"))),
            QStringLiteral("Восстановить"), QStringLiteral("Отмена"), true))
        return;
    BackupService service(m_ctx.db, &m_ctx.audit);
    service.setPgBinDir(pgBinDir());
    if (!run([&]() { service.restoreBackup(path, m_scheduler.directory()); }))
        return;
    ui::Messages::info(this, QStringLiteral("База данных восстановлена"),
                       QStringLiteral("Данные восстановлены из копии «%1».").arg(info.fileName()));
    refresh();
    emit databaseRestored();
}

}  // namespace sedm::admin
