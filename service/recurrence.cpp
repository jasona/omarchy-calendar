#include "recurrence.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QTimeZone>

namespace {
QHash<QString, QString> parseRule(QString rule)
{
    if (rule.startsWith(QStringLiteral("RRULE:"), Qt::CaseInsensitive))
        rule.remove(0, 6);
    QHash<QString, QString> values;
    for (const QString &part : rule.split(';', Qt::SkipEmptyParts)) {
        const int separator = part.indexOf('=');
        if (separator > 0)
            values.insert(part.left(separator).toUpper(), part.mid(separator + 1).toUpper());
    }
    return values;
}

int weekday(const QString &token)
{
    static const QHash<QString, int> days {
        { QStringLiteral("MO"), 1 }, { QStringLiteral("TU"), 2 },
        { QStringLiteral("WE"), 3 }, { QStringLiteral("TH"), 4 },
        { QStringLiteral("FR"), 5 }, { QStringLiteral("SA"), 6 },
        { QStringLiteral("SU"), 7 }
    };
    QString value = token;
    while (!value.isEmpty() && (value.front().isDigit() || value.front() == '-' || value.front() == '+'))
        value.remove(0, 1);
    return days.value(value, 0);
}

QDate untilDate(const QString &value)
{
    if (value.size() >= 8)
        return QDate::fromString(value.left(8), QStringLiteral("yyyyMMdd"));
    return {};
}
}

QList<RecurrenceOccurrence> Recurrence::expand(const QString &rule,
                                               qint64 startMs, qint64 endMs,
                                               const QString &timeZone,
                                               const QDate &rangeStart,
                                               const QDate &rangeEnd,
                                               int maximum)
{
    QList<RecurrenceOccurrence> result;
    const auto values = parseRule(rule);
    const QString frequency = values.value(QStringLiteral("FREQ"));
    if (frequency.isEmpty() || startMs <= 0 || endMs <= startMs
        || !rangeStart.isValid() || !rangeEnd.isValid() || rangeEnd < rangeStart || maximum < 1)
        return result;

    QTimeZone zone(timeZone.toUtf8());
    if (!zone.isValid()) zone = QTimeZone::systemTimeZone();
    const QDateTime seedStart = QDateTime::fromMSecsSinceEpoch(startMs, zone);
    const QDateTime seedEnd = QDateTime::fromMSecsSinceEpoch(endMs, zone);
    const QDate seedDate = seedStart.date();
    const int endDayOffset = seedDate.daysTo(seedEnd.date());
    const int interval = qMax(1, values.value(QStringLiteral("INTERVAL"), QStringLiteral("1")).toInt());
    const int countLimit = values.value(QStringLiteral("COUNT")).toInt();
    const QDate until = untilDate(values.value(QStringLiteral("UNTIL")));
    QSet<int> byDays;
    for (const QString &token : values.value(QStringLiteral("BYDAY")).split(',', Qt::SkipEmptyParts)) {
        const int day = weekday(token);
        if (day) byDays.insert(day);
    }
    if (frequency == QStringLiteral("WEEKLY") && byDays.isEmpty())
        byDays.insert(seedDate.dayOfWeek());

    int generated = 0;
    const QDate finalDate = until.isValid() && until < rangeEnd ? until : rangeEnd;
    for (QDate date = seedDate; date <= finalDate && result.size() < maximum; date = date.addDays(1)) {
        const int days = seedDate.daysTo(date);
        bool matches = false;
        if (frequency == QStringLiteral("DAILY"))
            matches = days % interval == 0;
        else if (frequency == QStringLiteral("WEEKLY"))
            matches = (days / 7) % interval == 0 && byDays.contains(date.dayOfWeek());
        else if (frequency == QStringLiteral("MONTHLY")) {
            const int months = (date.year() - seedDate.year()) * 12 + date.month() - seedDate.month();
            matches = months >= 0 && months % interval == 0 && date.day() == seedDate.day();
        } else if (frequency == QStringLiteral("YEARLY")) {
            matches = (date.year() - seedDate.year()) % interval == 0
                && date.month() == seedDate.month() && date.day() == seedDate.day();
        }
        if (!matches) continue;
        ++generated;
        if (countLimit > 0 && generated > countLimit) break;
        if (date < rangeStart) continue;

        const QDateTime occurrenceStart(date, seedStart.time(), zone,
                                        QDateTime::TransitionResolution::PreferAfter);
        const QDateTime occurrenceEnd(date.addDays(endDayOffset), seedEnd.time(), zone,
                                      QDateTime::TransitionResolution::PreferAfter);
        if (!occurrenceStart.isValid() || !occurrenceEnd.isValid() || occurrenceEnd <= occurrenceStart)
            continue;
        result.append({ date, occurrenceStart.toMSecsSinceEpoch(), occurrenceEnd.toMSecsSinceEpoch() });
    }
    return result;
}
