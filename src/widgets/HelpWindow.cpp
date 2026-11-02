/**
 * @file HelpWindow.cpp
 * @brief Реализация окна справки.
 */
#include "HelpWindow.h"
#include "Icons.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace sedm::ui {

namespace {
QPointer<HelpWindow> g_window;   // единственное окно справки программы
}

HelpWindow::HelpWindow(QWidget* parent)
    : QDialog(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("Справка — СЭДМ"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1000, 700);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    auto* top = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Поиск в разделе"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(Icons::icon(QStringLiteral("search"), palette::kMuted), QLineEdit::LeadingPosition);
    auto* find = new QPushButton(QStringLiteral("Найти далее"));
    top->addWidget(m_search, 1);
    top->addWidget(find);
    layout->addLayout(top);

    auto* splitter = new QSplitter;
    m_contents = new QListWidget;
    m_contents->setMinimumWidth(240);
    m_browser = new QTextBrowser;
    m_browser->setSearchPaths({QStringLiteral(":/help")});
    m_browser->setOpenExternalLinks(false);
    splitter->addWidget(m_contents);
    splitter->addWidget(m_browser);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 740});
    layout->addWidget(splitter, 1);

    // Оглавление: «файл.html|Название раздела»
    const QStringList lines = Theme::resourceText(QStringLiteral(":/help/contents.txt")).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString file = line.section(QLatin1Char('|'), 0, 0).trimmed();
        const QString title = line.section(QLatin1Char('|'), 1).trimmed();
        if (file.isEmpty() || title.isEmpty())
            continue;
        auto* item = new QListWidgetItem(title, m_contents);
        item->setData(Qt::UserRole, file);
    }
    connect(m_contents, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item)
            m_browser->setSource(QUrl(item->data(Qt::UserRole).toString()));
    });
    connect(m_browser, &QTextBrowser::sourceChanged, this, [this](const QUrl& url) {
        const QString file = url.path().section(QLatin1Char('/'), -1);
        for (int i = 0; i < m_contents->count(); ++i)
            if (m_contents->item(i)->data(Qt::UserRole).toString() == file) {
                const QSignalBlocker block(m_contents);
                m_contents->setCurrentRow(i);
            }
    });
    connect(find, &QPushButton::clicked, this, &HelpWindow::findNext);
    connect(m_search, &QLineEdit::returnPressed, this, &HelpWindow::findNext);
}

void HelpWindow::open(const QString& page)
{
    m_browser->setSource(QUrl(page));
}

void HelpWindow::findNext()
{
    const QString text = m_search->text().trimmed();
    if (text.isEmpty())
        return;
    if (!m_browser->find(text)) {
        // с начала раздела
        QTextCursor c = m_browser->textCursor();
        c.movePosition(QTextCursor::Start);
        m_browser->setTextCursor(c);
        if (!m_browser->find(text))
            m_search->setPlaceholderText(QStringLiteral("Не найдено"));
    }
}

void HelpWindow::showTopic(QWidget* parent, const QString& page)
{
    if (!g_window)
        g_window = new HelpWindow(parent);
    g_window->open(page);
    g_window->show();
    g_window->raise();
    g_window->activateWindow();
}

}  // namespace sedm::ui
