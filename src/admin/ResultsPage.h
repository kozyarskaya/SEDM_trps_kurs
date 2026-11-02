/**
 * @file ResultsPage.h
 * @brief Окно «Итоги и ведомость»: места в номинациях и экспорт XLSX.
 */
#pragma once

#include "AdminPage.h"
#include "RankingCalculator.h"

class QComboBox;
class QLabel;
class QTableWidget;

namespace sedm::admin {

/// Итоги (п. 4.3.1.1, 4.3.5 ТЗ): по выбранной номинации — место, шифр, участник,
/// регион, итоговые баллы дегустаторов, средний балл; экспорт итоговой ведомости
/// в файл XLSX (листы «Итоги» и «Детализация»).
class ResultsPage : public AdminPage {
    Q_OBJECT
public:
    explicit ResultsPage(AdminContext& ctx, QWidget* parent = nullptr);
    void refresh() override;
    QString helpTopic() const override { return QStringLiteral("results.html"); }

    void exportXlsx();   ///< меню «Дегустация → Экспорт ведомости XLSX»

private:
    void showNomination();

    QVector<NominationResult> m_results;
    QComboBox* m_nomination;
    QTableWidget* m_table;
    QLabel* m_note;
};

}  // namespace sedm::admin
