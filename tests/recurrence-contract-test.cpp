#include "database.h"
#include "recurrence.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QTimeZone newYork("America/New_York");
    const qint64 weeklyStart = QDateTime(QDate(2026, 3, 2), QTime(9, 0), newYork).toMSecsSinceEpoch();
    const qint64 weeklyEnd = QDateTime(QDate(2026, 3, 2), QTime(10, 0), newYork).toMSecsSinceEpoch();
    const auto weekly = Recurrence::expand(QStringLiteral("RRULE:FREQ=WEEKLY;BYDAY=MO,WE;COUNT=5"),
                                            weeklyStart, weeklyEnd, QStringLiteral("America/New_York"),
                                            QDate(2026, 3, 1), QDate(2026, 3, 31));
    const QList<QDate> weeklyDates { QDate(2026, 3, 2), QDate(2026, 3, 4), QDate(2026, 3, 9),
                                     QDate(2026, 3, 11), QDate(2026, 3, 16) };
    if (weekly.size() != weeklyDates.size()) return 2;
    for (int index = 0; index < weekly.size(); ++index) {
        const QDateTime occurrence = QDateTime::fromMSecsSinceEpoch(weekly.at(index).startMs, newYork);
        if (weekly.at(index).date != weeklyDates.at(index) || occurrence.time() != QTime(9, 0)) return 3;
    }
    if (weekly.at(1).startMs - weekly.at(2).startMs != -119LL * 60 * 60 * 1000) return 4;

    const auto daily = Recurrence::expand(QStringLiteral("FREQ=DAILY;INTERVAL=2;COUNT=3"),
        weeklyStart, weeklyEnd, QStringLiteral("America/New_York"), QDate(2026, 3, 1), QDate(2026, 3, 20));
    if (daily.size() != 3 || daily.at(2).date != QDate(2026, 3, 6)) return 5;

    const qint64 monthlyStart = QDateTime(QDate(2026, 1, 31), QTime(9, 0), newYork).toMSecsSinceEpoch();
    const auto monthly = Recurrence::expand(QStringLiteral("FREQ=MONTHLY;COUNT=3"),
        monthlyStart, monthlyStart + 3600000, QStringLiteral("America/New_York"),
        QDate(2026, 1, 1), QDate(2026, 6, 30));
    if (monthly.size() != 3 || monthly.at(0).date != QDate(2026, 1, 31)
        || monthly.at(1).date != QDate(2026, 3, 31) || monthly.at(2).date != QDate(2026, 5, 31)) return 6;

    const qint64 leapStart = QDateTime(QDate(2024, 2, 29), QTime(9, 0), newYork).toMSecsSinceEpoch();
    const auto yearly = Recurrence::expand(QStringLiteral("FREQ=YEARLY;COUNT=3"),
        leapStart, leapStart + 3600000, QStringLiteral("America/New_York"),
        QDate(2024, 1, 1), QDate(2032, 12, 31));
    if (yearly.size() != 3 || yearly.at(1).date != QDate(2028, 2, 29)
        || yearly.at(2).date != QDate(2032, 2, 29)) return 7;

    QTemporaryDir temporary;
    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:recurrence-test");
    if (!temporary.isValid() || !database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("recurrence-test"),
                                   QStringLiteral("Recurrence Test"), QStringLiteral("repeat@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") },
            { QStringLiteral("accessRole"), QStringLiteral("owner") },
            { QStringLiteral("selected"), true }
        } })) return 8;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value(QStringLiteral("id")).toString();
    const QJsonObject ordinary {
        { QStringLiteral("id"), QStringLiteral("instance-1") },
        { QStringLiteral("recurringEventId"), QStringLiteral("series-1") },
        { QStringLiteral("summary"), QStringLiteral("Weekly review") },
        { QStringLiteral("originalStartTime"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-03-09T09:00:00-04:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        } },
        { QStringLiteral("start"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-03-09T09:00:00-04:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        } },
        { QStringLiteral("end"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-03-09T10:00:00-04:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        } },
        { QStringLiteral("status"), QStringLiteral("confirmed") }
    };
    QJsonObject moved = ordinary;
    moved.insert(QStringLiteral("id"), QStringLiteral("instance-2"));
    moved.insert(QStringLiteral("originalStartTime"), QJsonObject {
        { QStringLiteral("dateTime"), QStringLiteral("2026-03-11T09:00:00-04:00") },
        { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
    });
    moved.insert(QStringLiteral("start"), QJsonObject {
        { QStringLiteral("dateTime"), QStringLiteral("2026-03-11T11:00:00-04:00") },
        { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
    });
    moved.insert(QStringLiteral("end"), QJsonObject {
        { QStringLiteral("dateTime"), QStringLiteral("2026-03-11T12:00:00-04:00") },
        { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
    });
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { ordinary, moved },
                                    QStringLiteral("recurrence-sync"), true)) return 9;
    const QJsonArray events = database.eventsForRange(QStringLiteral("2026-03-09"),
                                                      QStringLiteral("2026-03-11")).array();
    if (events.size() != 2 || !events.at(0).toObject().value(QStringLiteral("isRecurring")).toBool()
        || events.at(0).toObject().value(QStringLiteral("seriesId")) != QStringLiteral("series-1")
        || events.at(0).toObject().value(QStringLiteral("isException")).toBool()
        || !events.at(1).toObject().value(QStringLiteral("isException")).toBool()) return 10;
    if (!database.updatePendingEvent(QJsonObject {
            { QStringLiteral("id"), QStringLiteral("instance-2") }, { QStringLiteral("calendarId"), calendarId },
            { QStringLiteral("title"), QStringLiteral("Moved review") },
            { QStringLiteral("startMs"), events.at(1).toObject().value(QStringLiteral("startMs")) },
            { QStringLiteral("endMs"), events.at(1).toObject().value(QStringLiteral("endMs")) },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") },
            { QStringLiteral("scope"), QStringLiteral("series") },
            { QStringLiteral("seriesId"), QStringLiteral("series-1") },
            { QStringLiteral("scopeBaseStartMs"), events.at(1).toObject().value(QStringLiteral("startMs")) }
        })) return 11;
    const QJsonObject seriesMutation = database.nextPendingMutation(accountId).object();
    if (seriesMutation.value(QStringLiteral("operation")) != QStringLiteral("update-series")
        || seriesMutation.value(QStringLiteral("payload")).toObject().value(QStringLiteral("seriesId"))
            != QStringLiteral("series-1")
        || !seriesMutation.value(QStringLiteral("baseEtag")).toString().isEmpty()) return 15;
    const QJsonArray edited = database.eventsForRange(QStringLiteral("2026-03-11"),
                                                      QStringLiteral("2026-03-11")).array();
    if (edited.size() != 1 || edited.at(0).toObject().value(QStringLiteral("seriesId"))
        != QStringLiteral("series-1") || !edited.at(0).toObject().value(QStringLiteral("isException")).toBool()) return 12;
    const QString deletion = database.deletePendingEvent(calendarId, QStringLiteral("instance-2"));
    if (deletion.isEmpty() || !database.undoPendingDelete(deletion)) return 13;
    const QJsonArray restored = database.eventsForRange(QStringLiteral("2026-03-11"),
                                                        QStringLiteral("2026-03-11")).array();
    if (restored.size() != 1
        || restored.at(0).toObject().value(QStringLiteral("seriesId")) != QStringLiteral("series-1")
        || !restored.at(0).toObject().value(QStringLiteral("isException")).toBool()) return 14;
    return 0;
}
