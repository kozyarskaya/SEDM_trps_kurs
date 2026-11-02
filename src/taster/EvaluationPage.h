/**
 * @file EvaluationPage.h
 * @brief Окно «Оценивание»: список шифров корзины и карточка образца (макет В1).
 */
#pragma once

#include "TastingService.h"

#include <QDateTime>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QVBoxLayout;

namespace sedm::taster {

class ScoreRow;

/// Составное состояние «Оценивание корзины» диаграммы состояний: восстановление
/// черновика, ввод оценок образца (ползунки, клавиатура, «−»/«+»), сохранение,
/// контрольная точка каждые 5 минут (п. 4.4.4 ТЗ), подтверждение завершения.
class EvaluationPage : public QWidget {
    Q_OBJECT
public:
    static constexpr int kCheckpointMs = 5 * 60 * 1000;   ///< период контрольной точки

    explicit EvaluationPage(QWidget* parent = nullptr);

    /// Открывает корзину; при наличии черновика предлагает его восстановить.
    void open(TastingService* service, Database* db, const BasketSheet& sheet);
    /// Есть ли оценки, не сохранённые в базе данных.
    bool isDirty() const;
    /// Перед уходом со страницы: «Сохранить изменения?» (состояние «Запрос
    /// сохранения»). false — пользователь выбрал «Отмена».
    bool confirmLeave();
    /// Остановка таймера контрольной точки при уходе со страницы.
    void stop();

signals:
    /// «К списку корзин» (решение о несохранённых изменениях уже принято).
    void leaveRequested();
    /// Дегустация корзины завершена.
    void basketFinished(int basketNumber);

private:
    void buildSampleList();
    void selectSample(int index);
    void onScoreChanged(Criterion criterion, int value);
    void onNoteChanged();
    void updateSampleButton(int index);
    void updateTotal();
    void updateProgress();
    void updateStatus();
    bool save();
    void finish();
    void checkpoint();
    QString statusOf(int index) const;

    TastingService* m_service = nullptr;
    Database* m_db = nullptr;
    BasketSheet m_sheet;
    QVector<Evaluation> m_current;   ///< оценки на экране (по образцам корзины)
    QVector<Evaluation> m_saved;     ///< оценки, сохранённые в базе данных
    int m_index = -1;
    QDateTime m_savedAt;
    QString m_checkpointNote;
    QTimer m_checkpointTimer;

    QLabel* m_title;
    QProgressBar* m_progress;
    QLabel* m_progressText;
    QVBoxLayout* m_sampleList;
    QButtonGroup* m_sampleButtons;
    QLabel* m_cipher;
    QLabel* m_position;
    QVector<ScoreRow*> m_rows;
    QPlainTextEdit* m_note;
    QLabel* m_total;
    QLabel* m_totalHint;
    QPushButton* m_prev;
    QPushButton* m_next;
    QLabel* m_status;
    QPushButton* m_save;
    QPushButton* m_finish;
};

}  // namespace sedm::taster
