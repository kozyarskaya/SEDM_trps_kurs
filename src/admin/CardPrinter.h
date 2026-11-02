/**
 * @file CardPrinter.h
 * @brief Печать карточек дегустаторов (логин и PIN-код) и окно выданных PIN-кодов.
 */
#pragma once

#include "TeamService.h"

#include <QDialog>

class QPagedPaintDevice;

namespace sedm::admin {

/// Карточки дегустаторов на листе A4 (2 × 4): логотип, сеанс, команда, ФИО,
/// логин и PIN-код. Печать с предварительным просмотром или сохранение в PDF.
class CardPrinter {
public:
    static void preview(QWidget* parent, const QVector<TasterCredentials>& cards, const QString& session);
    static bool savePdf(const QString& path, const QVector<TasterCredentials>& cards, const QString& session);

private:
    static void paint(QPagedPaintDevice* device, const QVector<TasterCredentials>& cards, const QString& session);
};

/// Окно выданных логинов и PIN-кодов. PIN-коды показываются только здесь —
/// в базе данных хранится лишь их bcrypt-хэш (п. 4.4.6 ТЗ).
class CredentialsDialog : public QDialog {
    Q_OBJECT
public:
    CredentialsDialog(const QVector<TasterCredentials>& cards, const QString& session, QWidget* parent = nullptr);

private:
    void reject() override;

    QVector<TasterCredentials> m_cards;
    QString m_session;
    bool m_printed = false;
};

}  // namespace sedm::admin
