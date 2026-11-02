/**
 * @file DraftStore.cpp
 * @brief Реализация локальных черновиков оценок (формат JSON).
 */
#include "DraftStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace sedm {

namespace {
const char* kCriterionKeys[kCriteriaCount] = {"appearance", "consistency", "aroma", "taste"};
}

DraftStore::DraftStore(const QString& dir, const QString& serverKey)
    : m_dir(dir), m_serverKey(serverKey)
{
}

QString DraftStore::path(int tasterId, int basketId) const
{
    const QByteArray server = QCryptographicHash::hash(m_serverKey.toUtf8(), QCryptographicHash::Sha1).toHex().left(10);
    return QStringLiteral("%1/draft_%2_%3_%4.json").arg(m_dir, QString::fromLatin1(server))
        .arg(tasterId).arg(basketId);
}

bool DraftStore::save(int tasterId, int basketId, const QVector<Evaluation>& evaluations) const
{
    QDir().mkpath(m_dir);
    QJsonArray items;
    for (const Evaluation& e : evaluations) {
        QJsonObject item;
        item.insert(QStringLiteral("sample_id"), e.sampleId());
        for (int i = 0; i < kCriteriaCount; ++i) {
            const auto v = e.score(kAllCriteria[static_cast<size_t>(i)]);
            item.insert(QString::fromLatin1(kCriterionKeys[i]), v ? QJsonValue(*v) : QJsonValue());
        }
        item.insert(QStringLiteral("note"), e.note());
        items.append(item);
    }
    QJsonObject root;
    root.insert(QStringLiteral("server"), m_serverKey);
    root.insert(QStringLiteral("taster_id"), tasterId);
    root.insert(QStringLiteral("basket_id"), basketId);
    root.insert(QStringLiteral("saved_at"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("evaluations"), items);

    QSaveFile file(path(tasterId, basketId));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QVector<Evaluation> DraftStore::load(int tasterId, int basketId, QDateTime* savedAt) const
{
    QVector<Evaluation> result;
    QFile file(path(tasterId, basketId));
    if (!file.open(QIODevice::ReadOnly))
        return result;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("server")).toString() != m_serverKey
        || root.value(QStringLiteral("taster_id")).toInt() != tasterId
        || root.value(QStringLiteral("basket_id")).toInt() != basketId)
        return result;
    const QDateTime at = QDateTime::fromString(root.value(QStringLiteral("saved_at")).toString(), Qt::ISODateWithMs);
    for (const QJsonValue& v : root.value(QStringLiteral("evaluations")).toArray()) {
        const QJsonObject item = v.toObject();
        Evaluation e(tasterId, item.value(QStringLiteral("sample_id")).toInt());
        try {
            for (int i = 0; i < kCriteriaCount; ++i) {
                const QJsonValue score = item.value(QString::fromLatin1(kCriterionKeys[i]));
                if (score.isDouble())
                    e.setScore(kAllCriteria[static_cast<size_t>(i)], score.toInt());
            }
            e.setNote(item.value(QStringLiteral("note")).toString());
        } catch (...) {
            continue;   // повреждённая запись черновика пропускается
        }
        e.setSavedAt(at);
        result.append(e);
    }
    if (savedAt)
        *savedAt = at;
    return result;
}

void DraftStore::remove(int tasterId, int basketId) const
{
    QFile::remove(path(tasterId, basketId));
}

}  // namespace sedm
