/**
 * @file HelpWindow.h
 * @brief Справочная система, вызываемая из главного меню (п. 4.3.2, 4.6.4 ТЗ).
 */
#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QTextBrowser;

namespace sedm::ui {

/// Окно справки: оглавление разделов слева, текст раздела справа, поиск по тексту.
/// Разделы — HTML-файлы в ресурсах программы (:/help/); перечень разделов —
/// файл :/help/contents.txt (строки «файл.html|Название раздела»).
class HelpWindow : public QDialog {
    Q_OBJECT
public:
    /// Показывает справку (одно окно на программу); page — файл раздела.
    static void showTopic(QWidget* parent, const QString& page = QStringLiteral("index.html"));

private:
    explicit HelpWindow(QWidget* parent);
    void open(const QString& page);
    void findNext();

    QListWidget* m_contents;
    QTextBrowser* m_browser;
    QLineEdit* m_search;
};

}  // namespace sedm::ui
