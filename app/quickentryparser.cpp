#include "quickentryparser.h"

#include <QRegularExpression>
#include <QTimeZone>

namespace {
QString cleanTitle(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    value.remove(QRegularExpression(QStringLiteral("^(?:on|at|for)\\s+"),
                                    QRegularExpression::CaseInsensitiveOption));
    value.remove(QRegularExpression(QStringLiteral("\\s+(?:on|at|for)$"),
                                    QRegularExpression::CaseInsensitiveOption));
    return value.trimmed();
}

QTime parsedTime(QString token)
{
    token = token.trimmed().toLower();
    if (token == QStringLiteral("noon")) return QTime(12, 0);
    if (token == QStringLiteral("midnight")) return QTime(0, 0);
    QRegularExpressionMatch match = QRegularExpression(
        QStringLiteral("^(\\d{1,2})(?::(\\d{2}))?\\s*(am|pm)?$"),
        QRegularExpression::CaseInsensitiveOption).match(token);
    if (!match.hasMatch()) return {};
    int hour = match.captured(1).toInt();
    const int minute = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    const QString meridiem = match.captured(3).toLower();
    if (!meridiem.isEmpty()) {
        if (hour < 1 || hour > 12) return {};
        if (hour == 12) hour = 0;
        if (meridiem == QStringLiteral("pm")) hour += 12;
    }
    if (hour > 23 || minute > 59) return {};
    return QTime(hour, minute);
}
}

QuickEntryParser::QuickEntryParser(QObject *parent) : QObject(parent) {}

QVariantMap QuickEntryParser::parse(const QString &text) const
{
    return parseAt(text, QDateTime::currentDateTime());
}

QVariantMap QuickEntryParser::parseAt(const QString &input, const QDateTime &now)
{
    QString working = input.trimmed();
    QVariantMap result { { QStringLiteral("valid"), false } };
    if (working.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Type an event, date, and time."));
        return result;
    }

    int durationMinutes = 60;
    QRegularExpression durationPattern(
        QStringLiteral("\\bfor\\s+(?:(\\d+)\\s*(?:h|hr|hrs|hour|hours))?(?:\\s*(\\d+)\\s*(?:m|min|mins|minute|minutes))?\\b"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch durationMatch = durationPattern.match(working);
    if (durationMatch.hasMatch() && (!durationMatch.captured(1).isEmpty()
                                     || !durationMatch.captured(2).isEmpty())) {
        durationMinutes = durationMatch.captured(1).toInt() * 60 + durationMatch.captured(2).toInt();
        working.remove(durationMatch.capturedStart(), durationMatch.capturedLength());
    }
    if (durationMinutes < 5 || durationMinutes > 24 * 60) {
        result.insert(QStringLiteral("error"), QStringLiteral("Use a duration between 5 minutes and 24 hours."));
        return result;
    }

    QDate date = now.date();
    bool explicitDate = false;
    QRegularExpression simpleDate(QStringLiteral("\\b(today|tomorrow)\\b"),
                                  QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch dateMatch = simpleDate.match(working);
    if (dateMatch.hasMatch()) {
        explicitDate = true;
        if (dateMatch.captured(1).compare(QStringLiteral("tomorrow"), Qt::CaseInsensitive) == 0)
            date = date.addDays(1);
        working.remove(dateMatch.capturedStart(), dateMatch.capturedLength());
    } else {
        QRegularExpression isoDate(QStringLiteral("\\b(\\d{4}-\\d{2}-\\d{2})\\b"));
        dateMatch = isoDate.match(working);
        if (dateMatch.hasMatch()) {
            const QDate candidate = QDate::fromString(dateMatch.captured(1), Qt::ISODate);
            if (!candidate.isValid()) {
                result.insert(QStringLiteral("error"), QStringLiteral("That date is not valid."));
                return result;
            }
            date = candidate;
            explicitDate = true;
            working.remove(dateMatch.capturedStart(), dateMatch.capturedLength());
        } else {
            const QStringList weekdays { QStringLiteral("monday"), QStringLiteral("tuesday"),
                QStringLiteral("wednesday"), QStringLiteral("thursday"), QStringLiteral("friday"),
                QStringLiteral("saturday"), QStringLiteral("sunday") };
            QRegularExpression weekdayPattern(
                QStringLiteral("\\b(next\\s+)?(monday|tuesday|wednesday|thursday|friday|saturday|sunday)\\b"),
                QRegularExpression::CaseInsensitiveOption);
            dateMatch = weekdayPattern.match(working);
            if (dateMatch.hasMatch()) {
                const int target = weekdays.indexOf(dateMatch.captured(2).toLower()) + 1;
                int days = (target - date.dayOfWeek() + 7) % 7;
                if (days == 0 || !dateMatch.captured(1).isEmpty()) days += 7;
                date = date.addDays(days);
                explicitDate = true;
                working.remove(dateMatch.capturedStart(), dateMatch.capturedLength());
            }
        }
    }

    QTime time;
    QRegularExpression timePattern(
        QStringLiteral("\\b(?:at\\s+)?(noon|midnight|(?:[01]?\\d|2[0-3]):[0-5]\\d\\s*(?:am|pm)?|(?:1[0-2]|0?[1-9])\\s*(?:am|pm))\\b"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch timeMatch = timePattern.match(working);
    if (timeMatch.hasMatch()) {
        time = parsedTime(timeMatch.captured(1));
        working.remove(timeMatch.capturedStart(), timeMatch.capturedLength());
    }
    if (!time.isValid()) {
        if (explicitDate && date > now.date()) {
            time = QTime(9, 0);
        } else {
            QTime rounded = now.time();
            const int nextHalfHour = ((rounded.hour() * 60 + rounded.minute()) / 30 + 1) * 30;
            if (nextHalfHour >= 24 * 60) {
                date = date.addDays(1);
                time = QTime(0, 0);
            } else {
                time = QTime(nextHalfHour / 60, nextHalfHour % 60);
            }
        }
    }

    const QString title = cleanTitle(working);
    if (title.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Add a title, such as “Team sync tomorrow 10am”."));
        return result;
    }
    const QDateTime start(date, time, now.timeZone());
    const QDateTime end = start.addSecs(durationMinutes * 60);
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("title"), title);
    result.insert(QStringLiteral("date"), date.toString(Qt::ISODate));
    result.insert(QStringLiteral("endDate"), end.date().toString(Qt::ISODate));
    result.insert(QStringLiteral("startTime"), time.toString(QStringLiteral("HH:mm")));
    result.insert(QStringLiteral("endTime"), end.time().toString(QStringLiteral("HH:mm")));
    result.insert(QStringLiteral("durationMinutes"), durationMinutes);
    result.insert(QStringLiteral("summary"), QStringLiteral("%1 · %2–%3 · %4 min")
        .arg(date.toString(QStringLiteral("ddd, MMM d")), time.toString(QStringLiteral("h:mm AP")),
             end.time().toString(QStringLiteral("h:mm AP")), QString::number(durationMinutes)));
    return result;
}
