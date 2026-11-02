/**
 * @file SqlScript.cpp
 * @brief Лексический анализатор SQL-сценариев (конечный автомат).
 *
 * Текст делится на фрагменты: код, строковый литерал, идентификатор в кавычках,
 * строка в долларовых кавычках, комментарий, метакоманда psql. Точка с запятой
 * завершает команду только во фрагменте кода; метакоманда распознаётся только
 * в коде в начале строки (после необязательных пробелов).
 */
#include "SqlScript.h"

#include <QRegularExpression>
#include <QVector>

namespace sedm {

namespace {

enum class Kind { Code, Literal, Identifier, Dollar, Comment, Meta };

struct Segment {
    Kind kind;
    int begin;   ///< начало фрагмента
    int end;     ///< позиция за концом фрагмента
};

bool isIdentChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('$');
}

/// Тег долларовых кавычек в позиции i («$$» или «$тег$»); пусто — не тег.
QString dollarTagAt(const QString& s, int i)
{
    if (s[i] != QLatin1Char('$') || (i > 0 && isIdentChar(s[i - 1])))
        return QString();
    int j = i + 1;
    if (j < s.size() && s[j].isDigit())   // $1 — параметр, а не кавычки
        return QString();
    while (j < s.size() && (s[j].isLetterOrNumber() || s[j] == QLatin1Char('_')))
        ++j;
    return (j < s.size() && s[j] == QLatin1Char('$')) ? s.mid(i, j - i + 1) : QString();
}

/// Конец строкового литерала, начинающегося кавычкой в позиции i.
/// escapes — строка вида E'…', где обратная косая черта экранирует символ.
int literalEnd(const QString& s, int i, bool escapes)
{
    int j = i + 1;
    while (j < s.size()) {
        if (escapes && s[j] == QLatin1Char('\\')) {
            j += 2;
            continue;
        }
        if (s[j] == QLatin1Char('\'')) {
            if (j + 1 < s.size() && s[j + 1] == QLatin1Char('\'')) {   // '' — апостроф внутри
                j += 2;
                continue;
            }
            return j + 1;
        }
        ++j;
    }
    return s.size();   // незакрытый литерал — до конца текста
}

/// Конец идентификатора в кавычках, начинающегося в позиции i ("" — кавычка внутри).
int identifierEnd(const QString& s, int i)
{
    int j = i + 1;
    while (j < s.size()) {
        if (s[j] == QLatin1Char('"')) {
            if (j + 1 < s.size() && s[j + 1] == QLatin1Char('"')) {
                j += 2;
                continue;
            }
            return j + 1;
        }
        ++j;
    }
    return s.size();
}

/// Конец комментария /* … */ (комментарии могут быть вложенными).
int blockCommentEnd(const QString& s, int i)
{
    int depth = 1;
    int j = i + 2;
    while (j < s.size() && depth > 0) {
        if (s[j] == QLatin1Char('/') && j + 1 < s.size() && s[j + 1] == QLatin1Char('*')) {
            ++depth;
            j += 2;
        } else if (s[j] == QLatin1Char('*') && j + 1 < s.size() && s[j + 1] == QLatin1Char('/')) {
            --depth;
            j += 2;
        } else {
            ++j;
        }
    }
    return j;
}

QVector<Segment> lex(const QString& s)
{
    QVector<Segment> out;
    const int n = s.size();
    int codeStart = 0;
    bool lineStart = true;
    auto push = [&](Kind kind, int begin, int end) {
        if (begin > codeStart)
            out.append({Kind::Code, codeStart, begin});
        out.append({kind, begin, end});
        codeStart = end;
    };

    int i = 0;
    while (i < n) {
        const QChar ch = s[i];
        const QChar next = i + 1 < n ? s[i + 1] : QChar();
        int end = -1;
        Kind kind = Kind::Code;
        if (lineStart && ch == QLatin1Char('\\')) {   // метакоманда psql — до конца строки
            const int nl = s.indexOf(QLatin1Char('\n'), i);
            end = nl < 0 ? n : nl + 1;
            kind = Kind::Meta;
        } else if (ch == QLatin1Char('\'')) {
            end = literalEnd(s, i, false);
            kind = Kind::Literal;
        } else if ((ch == QLatin1Char('E') || ch == QLatin1Char('e')) && next == QLatin1Char('\'')
                   && (i == 0 || !isIdentChar(s[i - 1]))) {
            end = literalEnd(s, i + 1, true);
            kind = Kind::Literal;
        } else if (ch == QLatin1Char('"')) {
            end = identifierEnd(s, i);
            kind = Kind::Identifier;
        } else if (ch == QLatin1Char('-') && next == QLatin1Char('-')) {
            const int nl = s.indexOf(QLatin1Char('\n'), i);
            end = nl < 0 ? n : nl;   // перевод строки остаётся в коде
            kind = Kind::Comment;
        } else if (ch == QLatin1Char('/') && next == QLatin1Char('*')) {
            end = blockCommentEnd(s, i);
            kind = Kind::Comment;
        } else if (ch == QLatin1Char('$')) {
            const QString tag = dollarTagAt(s, i);
            if (!tag.isEmpty()) {
                const int close = s.indexOf(tag, i + tag.size());
                end = close < 0 ? n : close + tag.size();
                kind = Kind::Dollar;
            }
        }
        if (end >= 0) {
            push(kind, i, end);
            i = end;
            lineStart = (kind == Kind::Meta);
            continue;
        }
        if (ch == QLatin1Char('\n'))
            lineStart = true;
        else if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t') && ch != QLatin1Char('\r'))
            lineStart = false;
        ++i;
    }
    if (n > codeStart)
        out.append({Kind::Code, codeStart, n});
    return out;
}

}  // namespace

QStringList SqlScript::splitStatements(const QString& script)
{
    QStringList statements;
    QString current;
    bool hasCode = false;   // в команде есть что-то кроме пробелов и комментариев
    auto finish = [&]() {
        const QString text = current.trimmed();
        if (hasCode && !text.isEmpty())
            statements << text;
        current.clear();
        hasCode = false;
    };
    for (const Segment& seg : lex(script)) {
        switch (seg.kind) {
        case Kind::Meta:
            break;
        case Kind::Comment:
            current += QLatin1Char(' ');   // комментарий разделяет лексемы
            break;
        case Kind::Literal:
        case Kind::Identifier:
        case Kind::Dollar:
            current += script.midRef(seg.begin, seg.end - seg.begin);
            hasCode = true;
            break;
        case Kind::Code: {
            int start = seg.begin;
            for (int i = seg.begin; i < seg.end; ++i) {
                if (script[i] == QLatin1Char(';')) {
                    current += script.midRef(start, i - start);
                    finish();
                    start = i + 1;
                } else if (!script[i].isSpace()) {
                    hasCode = true;
                }
            }
            current += script.midRef(start, seg.end - start);
            break;
        }
        }
    }
    finish();
    return statements;
}

QString SqlScript::stripPsqlMetaCommands(const QString& script)
{
    QString out;
    out.reserve(script.size());
    for (const Segment& seg : lex(script))
        if (seg.kind != Kind::Meta)
            out += script.midRef(seg.begin, seg.end - seg.begin);
    return out;
}

bool SqlScript::containsCopyFromStdin(const QString& script)
{
    // Ищется только среди команд: слово COPY внутри строки или комментария не учитывается.
    // Данные COPY идут после самой команды, поэтому первая команда COPY распознаётся верно.
    static const QRegularExpression re(QStringLiteral("^COPY\\s+[^;]*\\bFROM\\s+stdin$"),
                                       QRegularExpression::CaseInsensitiveOption);
    for (const QString& statement : splitStatements(script))
        if (re.match(statement).hasMatch())
            return true;
    return false;
}

}  // namespace sedm
