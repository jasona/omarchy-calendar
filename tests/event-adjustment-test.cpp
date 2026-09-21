#include "database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    Database database(temporary.filePath("calendar.db"));
    const QString accountId = QStringLiteral("google:adjust-test");
    if (!temporary.isValid() || !database.open()
        || !database.upsertAccount(accountId, "google", "adjust-test", "Adjust Test",
                                   "adjust@example.com", "connected")
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { "id", "primary@example.com" }, { "summary", "Primary" },
            { "timeZone", "America/Phoenix" }, { "accessRole", "owner" }, { "selected", true }
        } })) return 2;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value("id").toString();
    const QJsonObject remote {
        { "id", "adjust-id" }, { "summary", "Adjust me" },
        { "description", "Keep fields" }, { "location", "Studio" },
        { "start", QJsonObject { { "dateTime", "2026-09-22T09:00:00-07:00" },
                                  { "timeZone", "America/Phoenix" } } },
        { "end", QJsonObject { { "dateTime", "2026-09-22T10:00:00-07:00" },
                                { "timeZone", "America/Phoenix" } } },
        { "etag", "adjust-etag" }, { "status", "confirmed" },
        { "updated", "2026-09-22T01:00:00Z" }
    };
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { remote }, "adjust-sync", true))
        return 3;

    QTimeZone zone("America/Phoenix");
    const qint64 movedStart = QDateTime(QDate(2026, 9, 23), QTime(9, 15), zone).toMSecsSinceEpoch();
    auto payload = QJsonObject {
        { "id", "adjust-id" }, { "calendarId", calendarId }, { "title", "Adjust me" },
        { "description", "Keep fields" }, { "location", "Studio" },
        { "startMs", double(movedStart) }, { "endMs", double(movedStart + 3600000) },
        { "timeZone", "America/Phoenix" }, { "allDay", false }
    };
    if (!database.updatePendingEvent(payload)) return 4;
    const QJsonObject firstMutation = database.nextPendingMutation(accountId).object();
    const QString mutationId = firstMutation.value("id").toString();
    if (mutationId.isEmpty() || firstMutation.value("operation") != QStringLiteral("update")
        || !database.eventsForRange("2026-09-22", "2026-09-22").array().isEmpty()
        || database.eventsForRange("2026-09-23", "2026-09-23").array().size() != 1) return 5;

    payload.insert("endMs", double(movedStart + 90 * 60000));
    if (!database.updatePendingEvent(payload)) return 6;
    payload.insert("startMs", double(movedStart + 15 * 60000));
    payload.insert("endMs", double(movedStart + 105 * 60000));
    if (!database.updatePendingEvent(payload)) return 7;
    const QJsonObject coalesced = database.nextPendingMutation(accountId).object();
    const QJsonObject latest = coalesced.value("payload").toObject();
    if (coalesced.value("id").toString() != mutationId
        || qint64(latest.value("startMs").toDouble()) != movedStart + 15 * 60000
        || qint64(latest.value("endMs").toDouble()) != movedStart + 105 * 60000
        || database.status().object().value("pendingMutationCount").toInt() != 1) return 8;
    return 0;
}
