/**
 * @file CardPrinter.cpp
 * @brief Отрисовка карточек дегустаторов (QPainter на QPrinter или QPdfWriter).
 */
#include "CardPrinter.h"
#include "AdminPage.h"
#include "Messages.h"
#include "Theme.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace sedm::admin {

void CardPrinter::paint(QPagedPaintDevice* device, const QVector<TasterCredentials>& cards, const QString& session)
{
    QPainter p(device);
    p.setRenderHint(QPainter::Antialiasing);
    const double mm = device->logicalDpiX() / 25.4;
    const double cardW = 90 * mm, cardH = 62 * mm, gap = 6 * mm;
    const double left = (device->width() - (2 * cardW + gap)) / 2;
    const double top = 12 * mm;
    QSvgRenderer logo(QStringLiteral(":/widgets/logo_light.svg"));
    auto font = [&](double sizeMm, bool bold) {
        QFont f(QStringLiteral("Segoe UI"));
        f.setPixelSize(qRound(sizeMm * mm));
        f.setBold(bold);
        return f;
    };

    for (int i = 0; i < cards.size(); ++i) {
        if (i > 0 && i % 8 == 0)
            device->newPage();
        const int slot = i % 8;
        const QRectF r(left + (slot % 2) * (cardW + gap), top + (slot / 2) * (cardH + gap), cardW, cardH);
        const TasterCredentials& c = cards[i];

        QPainterPath frame;
        frame.addRoundedRect(r, 4 * mm, 4 * mm);
        p.setPen(QPen(QColor(0xC9, 0xB4, 0x8F), 0.4 * mm));
        p.setBrush(Qt::white);
        p.drawPath(frame);
        // медовая полоса заголовка
        p.save();
        p.setClipPath(frame);
        p.fillRect(QRectF(r.left(), r.top(), r.width(), 12 * mm), QColor(0xFC, 0xEF, 0xD2));
        p.restore();
        logo.render(&p, QRectF(r.left() + 4 * mm, r.top() + 1.8 * mm, 9.3 * mm, 8 * mm));
        p.setPen(ui::palette::kBrown);
        p.setFont(font(3.4, true));
        p.drawText(QRectF(r.left() + 15 * mm, r.top() + 1 * mm, r.width() - 18 * mm, 6 * mm),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("СЭДМ · карточка дегустатора"));
        p.setFont(font(2.6, false));
        p.drawText(QRectF(r.left() + 15 * mm, r.top() + 6 * mm, r.width() - 18 * mm, 5 * mm),
                   Qt::AlignLeft | Qt::AlignVCenter, session);

        p.setPen(ui::palette::kText);
        double y = r.top() + 15 * mm;
        p.setFont(font(3.2, false));
        p.drawText(QRectF(r.left() + 5 * mm, y, r.width() - 10 * mm, 5 * mm), Qt::AlignLeft, c.teamTitle);
        y += 5.5 * mm;
        p.setFont(font(3.6, true));
        p.drawText(QRectF(r.left() + 5 * mm, y, r.width() - 10 * mm, 6 * mm), Qt::AlignLeft, c.fullName);
        y += 8.5 * mm;
        p.setFont(font(3.0, false));
        p.setPen(ui::palette::kMuted);
        p.drawText(QRectF(r.left() + 5 * mm, y, 30 * mm, 5 * mm), Qt::AlignLeft, QStringLiteral("Логин"));
        p.drawText(QRectF(r.left() + 52 * mm, y, 30 * mm, 5 * mm), Qt::AlignLeft, QStringLiteral("PIN-код"));
        y += 4.5 * mm;
        p.setPen(ui::palette::kText);
        p.setFont(font(6.0, true));
        p.drawText(QRectF(r.left() + 5 * mm, y, 46 * mm, 9 * mm), Qt::AlignLeft | Qt::AlignVCenter, c.login);
        QFont pin = font(7.5, true);
        pin.setLetterSpacing(QFont::AbsoluteSpacing, 1.5 * mm);
        p.setFont(pin);
        p.drawText(QRectF(r.left() + 52 * mm, y, 34 * mm, 9 * mm), Qt::AlignLeft | Qt::AlignVCenter, c.pin);
        p.setPen(ui::palette::kMuted);
        p.setFont(font(2.4, false));
        p.drawText(QRectF(r.left() + 5 * mm, r.bottom() - 9 * mm, r.width() - 10 * mm, 7 * mm),
                   Qt::AlignLeft | Qt::TextWordWrap,
                   QStringLiteral("Храните PIN-код в тайне. Если забыли PIN-код, обратитесь к администратору."));
    }
}

void CardPrinter::preview(QWidget* parent, const QVector<TasterCredentials>& cards, const QString& session)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    QPrintPreviewDialog dlg(&printer, parent);
    dlg.setWindowTitle(QStringLiteral("Печать карточек дегустаторов"));
    QObject::connect(&dlg, &QPrintPreviewDialog::paintRequested, [&](QPrinter* p) { paint(p, cards, session); });
    dlg.exec();
}

bool CardPrinter::savePdf(const QString& path, const QVector<TasterCredentials>& cards, const QString& session)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(300);
    writer.setTitle(QStringLiteral("Карточки дегустаторов СЭДМ"));
    paint(&writer, cards, session);
    return true;
}

CredentialsDialog::CredentialsDialog(const QVector<TasterCredentials>& cards, const QString& session, QWidget* parent)
    : QDialog(parent), m_cards(cards), m_session(session)
{
    setWindowTitle(QStringLiteral("Логины и PIN-коды дегустаторов"));
    resize(760, 480);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);
    auto* warn = new QLabel(QStringLiteral(
        "PIN-коды показываются только сейчас: в базе данных хранится лишь их хэш. Распечатайте карточки "
        "или сохраните их в PDF и раздайте дегустаторам. Если карточка потеряна, выдайте новый PIN-код."));
    warn->setWordWrap(true);
    warn->setProperty("role", "warn");
    layout->addWidget(warn);
    auto* table = new QTableWidget(cards.size(), 4);
    table->setHorizontalHeaderLabels({QStringLiteral("Команда"), QStringLiteral("ФИО"), QStringLiteral("Логин"),
                                      QStringLiteral("PIN-код")});
    setupTable(table);
    table->setSortingEnabled(false);
    for (int r = 0; r < cards.size(); ++r) {
        table->setItem(r, 0, new QTableWidgetItem(cards[r].teamTitle));
        table->setItem(r, 1, new QTableWidgetItem(cards[r].fullName));
        table->setItem(r, 2, new QTableWidgetItem(cards[r].login));
        auto* pin = new QTableWidgetItem(cards[r].pin);
        QFont bold = pin->font();
        bold.setBold(true);
        pin->setFont(bold);
        table->setItem(r, 3, pin);
    }
    table->resizeColumnsToContents();
    layout->addWidget(table, 1);

    auto* buttons = new QDialogButtonBox;
    auto* print = buttons->addButton(QStringLiteral("Печать карточек…"), QDialogButtonBox::ActionRole);
    print->setProperty("kind", "primary");
    auto* pdf = buttons->addButton(QStringLiteral("Сохранить PDF…"), QDialogButtonBox::ActionRole);
    buttons->addButton(QStringLiteral("Готово"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(print, &QPushButton::clicked, this, [this]() {
        CardPrinter::preview(this, m_cards, m_session);
        m_printed = true;
    });
    connect(pdf, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Карточки дегустаторов"),
                                                          QStringLiteral("Карточки дегустаторов.pdf"),
                                                          QStringLiteral("Документ PDF (*.pdf)"));
        if (path.isEmpty())
            return;
        CardPrinter::savePdf(path, m_cards, m_session);
        m_printed = true;
        ui::Messages::info(this, QStringLiteral("Карточки сохранены"),
                           QStringLiteral("Файл «%1» создан. Удалите его после печати: в нём PIN-коды.").arg(path));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &CredentialsDialog::reject);
}

void CredentialsDialog::reject()
{
    if (!m_printed && !m_cards.isEmpty()
        && !ui::Messages::confirm(this, QStringLiteral("Карточки не распечатаны"),
                                  QStringLiteral("Вы не распечатали и не сохранили карточки. После закрытия окна "
                                                 "PIN-коды больше не будут показаны (можно выдать новые). Закрыть?"),
                                  QStringLiteral("Закрыть"), QStringLiteral("Вернуться"), true))
        return;
    QDialog::reject();
}

}  // namespace sedm::admin
