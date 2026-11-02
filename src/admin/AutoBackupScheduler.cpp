/**
 * @file AutoBackupScheduler.cpp
 * @brief Реализация автоматических резервных копий в фоновом потоке.
 */
#include "AutoBackupScheduler.h"
#include "BackupService.h"
#include "Database.h"
#include "DbError.h"
#include "DomainError.h"

#include <QFileInfo>
#include <QThread>
#include <memory>

namespace sedm::admin {

namespace {
/// Итог работы фонового потока (передаётся в основной поток после завершения).
struct WorkResult {
    bool created = false;
    QString marker;
    QString message;
};
}  // namespace

AutoBackupScheduler::AutoBackupScheduler(const ConnectionSettings& settings, const QString& dir, QObject* parent)
    : QObject(parent), m_settings(settings), m_dir(dir)
{
    m_timer.setInterval(BackupService::kAutoIntervalMinutes * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &AutoBackupScheduler::runNow);
}

AutoBackupScheduler::~AutoBackupScheduler()
{
    if (m_worker) {
        m_worker->wait(30000);   // дождаться окончания текущей копии
        delete m_worker;
    }
}

void AutoBackupScheduler::start()
{
    m_timer.start();
}

void AutoBackupScheduler::runNow()
{
    if (m_worker)   // предыдущая копия ещё создаётся
        return;
    auto result = std::make_shared<WorkResult>();
    const ConnectionSettings settings = m_settings;
    const QString dir = m_dir;
    const QString pgBinDir = m_pgBinDir;
    const QString previous = m_marker;
    m_worker = QThread::create([=]() {
        try {
            Database db(QStringLiteral("sedm_autobackup"));   // своё подключение в своём потоке
            db.open(settings);
            BackupService backup(db);
            backup.setPgBinDir(pgBinDir);
            result->marker = backup.changeMarker();
            if (result->marker == previous) {
                result->message = QStringLiteral("данные не менялись");
                return;
            }
            const BackupResult r = backup.createAutoBackup(dir);
            result->created = true;
            result->message = QFileInfo(r.path).fileName();
        } catch (const DbError& e) {
            result->message = e.message();
        } catch (const DomainError& e) {
            result->message = e.message();
        }
    });
    connect(m_worker, &QThread::finished, this, [this, result]() {
        m_worker->deleteLater();
        m_worker = nullptr;
        if (result->created) {
            m_marker = result->marker;
            m_lastBackup = QDateTime::currentDateTime();
        } else if (!result->marker.isEmpty()) {
            m_marker = result->marker;
        }
        emit finished(result->created, result->message);
    });
    m_worker->start(QThread::LowPriority);
}

}  // namespace sedm::admin
