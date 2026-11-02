/**
 * @file AutoBackupScheduler.h
 * @brief Автоматические резервные копии базы данных каждые 5 минут (контрольные точки).
 */
#pragma once

#include "ConnectionSettings.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>

class QThread;

namespace sedm::admin {

/// Контрольные точки состояния (п. 4.4.4 ТЗ, решение В15): каждые 5 минут, если
/// данные изменились, создаётся копия базы данных в каталоге автокопий
/// (хранятся 12 последних). Копирование идёт в фоновом потоке со своим
/// подключением к серверу, поэтому окно администратора не «замирает».
class AutoBackupScheduler : public QObject {
    Q_OBJECT
public:
    AutoBackupScheduler(const ConnectionSettings& settings, const QString& dir, QObject* parent = nullptr);
    ~AutoBackupScheduler() override;

    void setPgBinDir(const QString& dir) { m_pgBinDir = dir; }
    void start();
    /// Создать автокопию сейчас (если данные изменились).
    void runNow();
    const QDateTime& lastBackup() const { return m_lastBackup; }
    const QString& directory() const { return m_dir; }

signals:
    /// Копирование завершено: created — копия создана (false — данные не менялись или ошибка).
    void finished(bool created, const QString& message);

private:
    ConnectionSettings m_settings;
    QString m_dir;
    QString m_pgBinDir;
    QString m_marker;          ///< метка данных при последней копии
    QDateTime m_lastBackup;
    QTimer m_timer;
    QThread* m_worker = nullptr;
};

}  // namespace sedm::admin
