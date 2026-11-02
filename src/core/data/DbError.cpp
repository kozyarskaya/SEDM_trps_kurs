/**
 * @file DbError.cpp
 * @brief Перевод ошибок PostgreSQL в сообщения для пользователя.
 */
#include "DbError.h"

#include <QHash>
#include <QRegularExpression>

namespace sedm {

namespace {

/// Первая строка сообщения сервера без префикса уровня («ОШИБКА:», «ERROR:»).
QString primaryMessage(const QString& text)
{
    QString line = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    static const QRegularExpression prefix(
        QStringLiteral("^(ОШИБКА|ERROR|ВАЖНО|FATAL|ПАНИКА|PANIC)\\s*:\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    line.remove(prefix);
    // QPSQL дописывает в конец код и описание запроса: «… (42P01) QPSQL: …»
    const int qpsql = line.indexOf(QStringLiteral(" QPSQL:"));
    if (qpsql > 0)
        line = line.left(qpsql).trimmed();
    return line;
}

/// Имя ограничения целостности из текста сервера (в кавычках).
QString constraintName(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("\"([a-z_]+_(key|fkey|uq|check))\""));
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1) : QString();
}

const QHash<QString, QString>& constraintMessages()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("sample_cipher_key"), QStringLiteral("Шифр образца уже используется.")},
        {QStringLiteral("sample_seq_number_key"), QStringLiteral("Порядковый номер образца уже занят.")},
        {QStringLiteral("nomination_name_uq"), QStringLiteral("Номинация с таким названием уже есть.")},
        {QStringLiteral("team_number_key"), QStringLiteral("Команда с таким номером уже есть.")},
        {QStringLiteral("basket_number_key"), QStringLiteral("Корзина с таким номером уже есть.")},
        {QStringLiteral("taster_login_key_key"), QStringLiteral("Дегустатор с таким логином уже есть.")},
        {QStringLiteral("evaluation_sample_id_fkey"),
         QStringLiteral("Образец уже оценивался дегустаторами — удалить его нельзя.")},
        {QStringLiteral("evaluation_taster_id_fkey"),
         QStringLiteral("У дегустатора есть сохранённые оценки — удалить его (или его команду) нельзя.")},
        {QStringLiteral("sample_participant_id_fkey"),
         QStringLiteral("У участника есть зарегистрированные образцы — сначала удалите их.")},
        {QStringLiteral("participant_nomination_id_fkey"),
         QStringLiteral("Номинация используется в реестре участников — удалить её нельзя.")},
        {QStringLiteral("sample_nomination_id_fkey"),
         QStringLiteral("Номинация используется образцами — удалить её нельзя.")},
        {QStringLiteral("basket_nomination_id_fkey"),
         QStringLiteral("Номинация используется корзинами — удалить её нельзя.")},
    };
    return map;
}

}  // namespace

DbError::DbError(Kind kind, const QString& message, const QString& sqlState, const QString& details)
    : std::runtime_error(message.toUtf8().toStdString()),
      m_kind(kind), m_message(message), m_sqlState(sqlState), m_details(details)
{
}

DbError DbError::fromSqlError(const QSqlError& error)
{
    const QString state = error.nativeErrorCode();
    const QString server = error.databaseText().isEmpty() ? error.driverText() : error.databaseText();
    const QString primary = primaryMessage(server);

    if (error.type() == QSqlError::ConnectionError || state.startsWith(QLatin1String("08"))
        || state == QLatin1String("57P01") || state == QLatin1String("57P03")) {
        return DbError(Kind::Connection,
                       QStringLiteral("Нет связи с сервером базы данных. Проверьте, что сервер "
                                      "запущен и доступен по сети, и повторите действие."),
                       state, server);
    }
    if (state == QLatin1String("28P01") || state == QLatin1String("28000"))
        return DbError(Kind::Connection,
                       QStringLiteral("Сервер отклонил учётную запись приложения: неверное имя "
                                      "пользователя или пароль базы данных."), state, server);
    if (state == QLatin1String("3D000"))
        return DbError(Kind::Connection,
                       QStringLiteral("База данных СЭДМ на сервере не найдена. Выполните первичную "
                                      "настройку в программе администратора."), state, server);

    if (state.startsWith(QLatin1String("23"))) {   // нарушения целостности
        const QString name = constraintName(server);
        const auto& known = constraintMessages();
        if (known.contains(name))
            return DbError(Kind::Constraint, known.value(name), state, server);
        if (state == QLatin1String("23514") || state == QLatin1String("P0001"))
            return DbError(Kind::Constraint, primary, state, server);   // текст триггера
        if (state == QLatin1String("23505"))
            return DbError(Kind::Constraint, QStringLiteral("Такая запись уже существует."), state, server);
        if (state == QLatin1String("23503"))
            return DbError(Kind::Constraint,
                           QStringLiteral("Действие невозможно: есть связанные записи."), state, server);
        return DbError(Kind::Constraint, primary, state, server);
    }
    if (state == QLatin1String("P0001"))
        return DbError(Kind::Constraint, primary, state, server);
    // Если соединение пропало во время запроса, QPSQL не всегда возвращает код
    if (state.isEmpty() && server.contains(QStringLiteral("connection"), Qt::CaseInsensitive))
        return DbError(Kind::Connection,
                       QStringLiteral("Соединение с сервером базы данных потеряно. Повторите действие."),
                       state, server);
    return DbError(Kind::Other, QStringLiteral("Ошибка базы данных: %1").arg(primary), state, server);
}

}  // namespace sedm
