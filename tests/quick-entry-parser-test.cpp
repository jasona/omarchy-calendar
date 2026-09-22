#include "quickentryparser.h"

#include <QCoreApplication>
#include <QTimeZone>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDateTime now(QDate(2026, 9, 21), QTime(16, 10), QTimeZone("America/Phoenix"));

    const QVariantMap sync = QuickEntryParser::parseAt(
        QStringLiteral("Team sync tomorrow 10am for 45m"), now);
    if (!sync.value("valid").toBool() || sync.value("title") != QStringLiteral("Team sync")
        || sync.value("date") != QStringLiteral("2026-09-22")
        || sync.value("startTime") != QStringLiteral("10:00")
        || sync.value("endTime") != QStringLiteral("10:45")
        || sync.value("durationMinutes").toInt() != 45) return 2;

    const QVariantMap lunch = QuickEntryParser::parseAt(
        QStringLiteral("Lunch next Friday noon for 1h 30m"), now);
    if (!lunch.value("valid").toBool() || lunch.value("title") != QStringLiteral("Lunch")
        || lunch.value("date") != QStringLiteral("2026-10-02")
        || lunch.value("startTime") != QStringLiteral("12:00")
        || lunch.value("endTime") != QStringLiteral("13:30")) return 3;

    const QVariantMap iso = QuickEntryParser::parseAt(
        QStringLiteral("Release review 2026-09-30 at 14:30"), now);
    if (!iso.value("valid").toBool() || iso.value("title") != QStringLiteral("Release review")
        || iso.value("date") != QStringLiteral("2026-09-30")
        || iso.value("startTime") != QStringLiteral("14:30")) return 4;

    const QVariantMap defaultTime = QuickEntryParser::parseAt(QStringLiteral("Write brief"), now);
    if (!defaultTime.value("valid").toBool()
        || defaultTime.value("startTime") != QStringLiteral("16:30")) return 5;

    const QVariantMap invalid = QuickEntryParser::parseAt(QStringLiteral("tomorrow 10am"), now);
    if (invalid.value("valid").toBool() || invalid.value("error").toString().isEmpty()) return 6;
    return 0;
}
