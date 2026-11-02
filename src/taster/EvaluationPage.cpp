/**
 * @file EvaluationPage.cpp
 * @brief Реализация окна «Оценивание».
 */
#include "EvaluationPage.h"
#include "Icons.h"
#include "LogoWidget.h"
#include "Messages.h"
#include "ScoreRow.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::taster {

namespace {
QString scoreText(double value)
{
    return QLocale(QLocale::Russian).toString(value, 'f', 2);
}
}  // namespace

EvaluationPage::EvaluationPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- шапка: корзина, ход оценивания, возврат к списку корзин
    auto* header = new QFrame;
    header->setObjectName(QStringLiteral("header"));
    auto* h = new QHBoxLayout(header);
    h->setContentsMargins(22, 14, 22, 14);
    h->setSpacing(16);
    h->addWidget(new ui::LogoWidget(true, 43));
    m_title = new QLabel;
    m_title->setProperty("role", "title");
    h->addWidget(m_title);
    m_progress = new QProgressBar;
    m_progress->setObjectName(QStringLiteral("headerBar"));
    m_progress->setMaximumWidth(260);
    m_progress->setMinimumWidth(140);
    h->addWidget(m_progress, 1);
    m_progressText = new QLabel;
    h->addWidget(m_progressText);
    h->addStretch(1);
    auto* back = new QPushButton(QStringLiteral("← К списку корзин"));
    back->setProperty("kind", "onDark");
    back->setProperty("big", true);
    h->addWidget(back);
    layout->addWidget(header);

    // --- середина: список шифров слева, карточка образца справа
    auto* middle = new QHBoxLayout;
    middle->setSpacing(0);
    auto* listScroll = new QScrollArea;
    listScroll->setObjectName(QStringLiteral("sampleListArea"));
    listScroll->setWidgetResizable(true);
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setMinimumWidth(270);
    listScroll->setMaximumWidth(320);
    auto* listHost = new QWidget;
    listHost->setObjectName(QStringLiteral("sampleList"));
    m_sampleList = new QVBoxLayout(listHost);
    m_sampleList->setContentsMargins(12, 12, 12, 12);
    m_sampleList->setSpacing(8);
    listScroll->setWidget(listHost);
    middle->addWidget(listScroll);
    m_sampleButtons = new QButtonGroup(this);
    m_sampleButtons->setExclusive(true);

    auto* cardScroll = new QScrollArea;
    cardScroll->setWidgetResizable(true);
    cardScroll->setFrameShape(QFrame::NoFrame);
    auto* card = new QWidget;
    auto* c = new QVBoxLayout(card);
    c->setContentsMargins(26, 12, 26, 12);
    c->setSpacing(8);
    auto* cipherLine = new QHBoxLayout;
    cipherLine->setSpacing(14);
    auto* sampleWord = new QLabel(QStringLiteral("Образец"));
    sampleWord->setProperty("role", "muted");
    m_cipher = new QLabel;
    m_cipher->setObjectName(QStringLiteral("sampleCipher"));
    m_position = new QLabel;
    m_position->setProperty("role", "muted");
    cipherLine->addWidget(sampleWord, 0, Qt::AlignBottom);
    cipherLine->addWidget(m_cipher, 0, Qt::AlignBottom);
    cipherLine->addWidget(m_position, 0, Qt::AlignBottom);
    cipherLine->addStretch(1);
    c->addLayout(cipherLine);
    for (Criterion criterion : kAllCriteria) {
        auto* row = new ScoreRow(criterion);
        connect(row, &ScoreRow::valueChanged, this, &EvaluationPage::onScoreChanged);
        m_rows.append(row);
        c->addWidget(row);
    }
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(16);
    auto* noteBox = new QVBoxLayout;
    auto* noteLabel = new QLabel(QStringLiteral("Примечание (необязательно)"));
    m_note = new QPlainTextEdit;
    m_note->setMaximumHeight(84);
    m_note->setPlaceholderText(QStringLiteral("Например: лёгкая горчинка в послевкусии"));
    noteLabel->setBuddy(m_note);
    noteBox->addWidget(noteLabel);
    noteBox->addWidget(m_note);
    bottom->addLayout(noteBox, 3);
    auto* totalCard = new QFrame;
    totalCard->setProperty("card", "dark");
    totalCard->setMinimumWidth(220);
    auto* t = new QVBoxLayout(totalCard);
    t->setContentsMargins(18, 12, 18, 12);
    t->setSpacing(2);
    auto* totalCaption = new QLabel(QStringLiteral("Итоговый балл"));
    totalCaption->setProperty("role", "muted");
    m_total = new QLabel(QStringLiteral("—"));
    m_total->setProperty("role", "big");
    m_totalHint = new QLabel;
    m_totalHint->setProperty("role", "muted");
    m_totalHint->setWordWrap(true);
    t->addWidget(totalCaption);
    t->addWidget(m_total);
    t->addWidget(m_totalHint);
    bottom->addWidget(totalCard, 1);
    c->addLayout(bottom);
    c->addStretch(1);
    cardScroll->setWidget(card);
    middle->addWidget(cardScroll, 1);
    layout->addLayout(middle, 1);

    // --- нижняя панель: навигация, состояние сохранения, «Сохранить», «Завершить»
    auto* footer = new QFrame;
    footer->setObjectName(QStringLiteral("footer"));
    auto* f = new QHBoxLayout(footer);
    f->setContentsMargins(22, 12, 22, 12);
    f->setSpacing(12);
    m_prev = new QPushButton(QStringLiteral("← Предыдущий"));
    m_next = new QPushButton(QStringLiteral("Следующий →"));
    m_status = new QLabel;
    m_status->setProperty("role", "muted");
    m_status->setWordWrap(true);
    m_save = new QPushButton(QStringLiteral("Сохранить"));
    m_save->setProperty("kind", "primary");
    m_save->setToolTip(QStringLiteral("Записать оценки на сервер (Ctrl+S)"));
    m_finish = new QPushButton(QStringLiteral("Завершить корзину"));
    m_finish->setProperty("kind", "strong");
    for (QPushButton* b : {m_prev, m_next, m_save, m_finish})
        b->setProperty("big", true);
    f->addWidget(m_prev);
    f->addWidget(m_next);
    f->addWidget(m_status, 1);
    f->addWidget(m_save);
    f->addWidget(m_finish);
    layout->addWidget(footer);

    connect(back, &QPushButton::clicked, this, [this]() {
        if (confirmLeave())
            emit leaveRequested();
    });
    connect(m_prev, &QPushButton::clicked, this, [this]() { selectSample(m_index - 1); });
    connect(m_next, &QPushButton::clicked, this, [this]() { selectSample(m_index + 1); });
    connect(m_save, &QPushButton::clicked, this, [this]() { save(); });
    connect(m_finish, &QPushButton::clicked, this, &EvaluationPage::finish);
    connect(m_note, &QPlainTextEdit::textChanged, this, &EvaluationPage::onNoteChanged);
    connect(m_sampleButtons, &QButtonGroup::idClicked, this, &EvaluationPage::selectSample);
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this]() { save(); });
    auto* nextShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageDown), this);
    connect(nextShortcut, &QShortcut::activated, this, [this]() { selectSample(m_index + 1); });
    auto* prevShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_PageUp), this);
    connect(prevShortcut, &QShortcut::activated, this, [this]() { selectSample(m_index - 1); });

    m_checkpointTimer.setInterval(kCheckpointMs);
    connect(&m_checkpointTimer, &QTimer::timeout, this, &EvaluationPage::checkpoint);
}

void EvaluationPage::open(TastingService* service, Database* db, const BasketSheet& sheet)
{
    m_service = service;
    m_db = db;
    m_sheet = sheet;
    m_current.clear();
    m_saved.clear();
    for (const Sample& s : sheet.samples) {
        Evaluation e = sheet.savedFor(s.id());
        e.setTasterId(service->taster().id());
        m_saved.append(e);
        m_current.append(e);
    }
    m_savedAt = QDateTime();
    m_checkpointNote.clear();
    m_title->setText(QStringLiteral("Корзина № %1 · %2").arg(sheet.info.basket.number()).arg(sheet.info.nominationName));
    buildSampleList();

    // Состояние «Восстановление черновика»: черновик новее сохранённых оценок
    if (!sheet.draft.isEmpty()) {
        const QString when = sheet.draftSavedAt.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
        const int answer = ui::Messages::choose(
            this, QStringLiteral("Найден черновик"),
            QStringLiteral("Найдены несохранённые оценки этой корзины (контрольная точка от %1).\n"
                           "Восстановить их?").arg(when),
            {QStringLiteral("Восстановить"), QStringLiteral("Отклонить")}, 0, 0);
        if (answer == 0) {
            for (const Evaluation& d : sheet.draft)
                for (int i = 0; i < m_current.size(); ++i)
                    if (m_current[i].sampleId() == d.sampleId()) {
                        m_current[i] = d;
                        m_current[i].setTasterId(service->taster().id());
                    }
            m_checkpointNote = QStringLiteral("Восстановлен черновик от %1 — не забудьте сохранить").arg(when);
        } else {
            ui::Messages::guard(this, *m_db, [&]() { m_service->discardDraft(sheet.info.basket.id()); });
        }
    }
    for (int i = 0; i < m_current.size(); ++i)
        updateSampleButton(i);
    // первым показывается первый неоценённый образец
    int first = 0;
    for (int i = 0; i < m_current.size(); ++i)
        if (!m_current[i].isComplete()) {
            first = i;
            break;
        }
    m_index = -1;
    selectSample(first);
    updateProgress();
    updateStatus();
    m_checkpointTimer.start();
}

void EvaluationPage::stop()
{
    m_checkpointTimer.stop();
}

void EvaluationPage::buildSampleList()
{
    for (QAbstractButton* b : m_sampleButtons->buttons()) {
        m_sampleButtons->removeButton(b);
        b->deleteLater();
    }
    while (QLayoutItem* item = m_sampleList->takeAt(0))
        delete item;
    for (int i = 0; i < m_sheet.samples.size(); ++i) {
        auto* b = new QPushButton;
        b->setProperty("sample", true);
        b->setCheckable(true);
        auto* v = new QVBoxLayout(b);
        v->setContentsMargins(14, 8, 14, 8);
        v->setSpacing(2);
        auto* cipher = new QLabel(m_sheet.samples[i].cipher());
        cipher->setObjectName(QStringLiteral("cipher"));
        cipher->setProperty("role", "cipher");
        auto* status = new QLabel;
        status->setObjectName(QStringLiteral("status"));
        status->setProperty("role", "sampleStatus");
        for (QLabel* l : {cipher, status})
            l->setAttribute(Qt::WA_TransparentForMouseEvents);
        v->addWidget(cipher);
        v->addWidget(status);
        m_sampleButtons->addButton(b, i);
        m_sampleList->addWidget(b);
    }
    m_sampleList->addStretch(1);
}

QString EvaluationPage::statusOf(int index) const
{
    const Evaluation& e = m_current[index];
    QString text;
    if (e.isComplete())
        text = QStringLiteral("Оценён · %1").arg(scoreText(*e.total()));
    else if (e.ratedCount() > 0)
        text = QStringLiteral("Частично · %1 из %2").arg(e.ratedCount()).arg(kCriteriaCount);
    else
        text = QStringLiteral("Не оценён");
    if (!e.sameContentAs(m_saved[index]))
        text += QStringLiteral("\nне сохранено");   // отдельной строкой: в одну строку списка не помещается
    return text;
}

void EvaluationPage::updateSampleButton(int index)
{
    auto* button = m_sampleButtons->button(index);
    if (!button)
        return;
    auto* status = button->findChild<QLabel*>(QStringLiteral("status"));
    const Evaluation& e = m_current[index];
    const QString state = e.isComplete() ? QStringLiteral("done")
                          : e.ratedCount() > 0 ? QStringLiteral("partial") : QStringLiteral("none");
    status->setText(statusOf(index));
    ui::Theme::setProp(status, "state", state);
    // вторая строка «не сохранено» — кнопка образца растёт по высоте содержимого
    button->setMinimumHeight(button->layout()->sizeHint().height());
    button->setAccessibleName(QStringLiteral("Образец %1, %2").arg(m_sheet.samples[index].cipher(), statusOf(index)));
}

void EvaluationPage::selectSample(int index)
{
    if (index < 0 || index >= m_current.size())
        return;
    m_index = index;
    if (auto* b = m_sampleButtons->button(index))
        b->setChecked(true);
    const Evaluation& e = m_current[index];
    m_cipher->setText(m_sheet.samples[index].cipher());
    m_position->setText(QStringLiteral("%1 из %2").arg(index + 1).arg(m_current.size()));
    for (ScoreRow* row : m_rows)
        row->setValue(e.score(row->criterion()));
    {
        const QSignalBlocker block(m_note);
        m_note->setPlainText(e.note());
    }
    m_prev->setEnabled(index > 0);
    m_next->setEnabled(index + 1 < m_current.size());
    updateTotal();
}

void EvaluationPage::onScoreChanged(Criterion criterion, int value)
{
    if (m_index < 0)
        return;
    m_current[m_index].setScore(criterion, value);
    updateTotal();
    updateSampleButton(m_index);
    updateProgress();
    updateStatus();
}

void EvaluationPage::onNoteChanged()
{
    if (m_index < 0)
        return;
    QString text = m_note->toPlainText();
    if (text.size() > Evaluation::kMaxNoteLength) {   // ограничение длины примечания
        text.truncate(Evaluation::kMaxNoteLength);
        const QSignalBlocker block(m_note);
        m_note->setPlainText(text);
        m_note->moveCursor(QTextCursor::End);
    }
    m_current[m_index].setNote(text);
    updateSampleButton(m_index);
    updateStatus();
}

void EvaluationPage::updateTotal()
{
    const Evaluation& e = m_current[m_index];
    if (const auto total = e.total()) {
        m_total->setText(scoreText(*total));
        m_totalHint->setText(QStringLiteral("среднее четырёх оценок"));
    } else {
        m_total->setText(QStringLiteral("—"));
        m_totalHint->setText(QStringLiteral("поставьте все четыре оценки (поставлено %1)").arg(e.ratedCount()));
    }
}

void EvaluationPage::updateProgress()
{
    int complete = 0;
    for (const Evaluation& e : m_current)
        complete += e.isComplete() ? 1 : 0;
    m_progress->setRange(0, qMax(1, m_current.size()));
    m_progress->setValue(complete);
    m_progressText->setText(QStringLiteral("Оценено %1 из %2").arg(complete).arg(m_current.size()));
}

bool EvaluationPage::isDirty() const
{
    for (int i = 0; i < m_current.size(); ++i)
        if (!m_current[i].sameContentAs(m_saved[i]))
            return true;
    return false;
}

void EvaluationPage::updateStatus()
{
    QStringList parts;
    if (m_savedAt.isValid())
        parts << QStringLiteral("Сохранено в %1").arg(m_savedAt.toString(QStringLiteral("HH:mm")));
    if (isDirty())
        parts << QStringLiteral("есть несохранённые изменения");
    if (!m_checkpointNote.isEmpty())
        parts << m_checkpointNote;
    if (parts.isEmpty())
        parts << QStringLiteral("Изменений нет");
    m_status->setText(parts.join(QStringLiteral(" · ")));
    m_save->setEnabled(isDirty());
}

bool EvaluationPage::save()
{
    if (!m_service || !isDirty())
        return true;
    const bool ok = ui::Messages::guard(this, *m_db, [&]() {
        m_service->save(m_sheet.info.basket.id(), m_current);
    }, QStringLiteral("Оценки не сохранены"));
    if (ok) {
        m_saved = m_current;
        m_savedAt = QDateTime::currentDateTime();
        m_checkpointNote.clear();
        for (int i = 0; i < m_current.size(); ++i)
            updateSampleButton(i);
        updateStatus();
    }
    return ok;
}

void EvaluationPage::finish()
{
    // Завершить корзину [есть неполные оценки] / сообщение «Не оценены образцы: …»
    QStringList incomplete;
    for (int i = 0; i < m_current.size(); ++i)
        if (!m_current[i].isComplete())
            incomplete << m_sheet.samples[i].cipher();
    if (!incomplete.isEmpty()) {
        ui::Messages::warning(this, QStringLiteral("Корзина не оценена полностью"),
                              QStringLiteral("Не оценены по всем четырём критериям образцы: %1.\n"
                                             "Завершить дегустацию корзины можно после оценки всех образцов.")
                                  .arg(incomplete.join(QStringLiteral(", "))));
        return;
    }
    // Состояние «Подтверждение завершения»
    if (!ui::Messages::confirm(this, QStringLiteral("Завершение дегустации"),
                               QStringLiteral("Завершить дегустацию корзины № %1?\n"
                                              "Изменить оценки после этого будет нельзя.")
                                   .arg(m_sheet.info.basket.number()),
                               QStringLiteral("Да, завершить"), QStringLiteral("Нет"), true))
        return;
    const bool ok = ui::Messages::guard(this, *m_db, [&]() {
        m_service->finish(m_sheet.info.basket.id(), m_current);
    }, QStringLiteral("Корзина не завершена"));
    if (ok) {
        m_saved = m_current;
        stop();
        emit basketFinished(m_sheet.info.basket.number());
    }
}

void EvaluationPage::checkpoint()
{
    // after(5 мин) [есть несохранённые изменения] / записать черновик
    if (!m_service || !isDirty())
        return;
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    bool toServer = false;
    ui::Messages::guard(this, [&]() { toServer = m_service->saveCheckpoint(m_sheet.info.basket.id(), m_current); });
    m_checkpointNote = toServer
                           ? QStringLiteral("черновик записан автоматически в %1").arg(time)
                           : QStringLiteral("нет связи с сервером — черновик сохранён на этом компьютере в %1").arg(time);
    updateStatus();
}

bool EvaluationPage::confirmLeave()
{
    if (!isDirty())
        return true;
    const int answer = ui::Messages::choose(this, QStringLiteral("Несохранённые изменения"),
                                            QStringLiteral("Есть несохранённые оценки. Сохранить изменения?"),
                                            {QStringLiteral("Сохранить"), QStringLiteral("Не сохранять"),
                                             QStringLiteral("Отмена")}, 0, 2);
    if (answer == 0)
        return save();
    if (answer == 1) {
        ui::Messages::guard(this, *m_db, [&]() { m_service->discardDraft(m_sheet.info.basket.id()); });
        return true;
    }
    return false;
}

}  // namespace sedm::taster
