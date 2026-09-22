#include "database.h"
#include "googlemutations.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

namespace {
void respond(QTcpSocket *socket, const QByteArray &body, int status = 200,
             const QByteArray &reason = QByteArrayLiteral("OK"))
{
    socket->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason
                  + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                  + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    socket->disconnectFromHost();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    QTcpServer server;
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "test setup failed: temporary=%d path=%s server=%s\n",
                     temporary.isValid(), qPrintable(temporary.path()),
                     qPrintable(server.errorString()));
        return 2;
    }
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    bool validCreateRequest = false;
    bool validUpdateRequest = false;
    bool validConflictLookup = false;
    int updateAttempts = 0;
    int requestCount = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                const int headerEnd = request.indexOf("\r\n\r\n");
                if (headerEnd < 0) return;
                int contentLength = 0;
                for (const QByteArray &line : request.left(headerEnd).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (request.size() < headerEnd + 4 + contentLength) return;
                ++requestCount;
                const QJsonObject body = QJsonDocument::fromJson(
                    request.mid(headerEnd + 4, contentLength)).object();
                if (request.startsWith("POST /calendar/v3/calendars/primary%40example.com/events")) {
                    validCreateRequest = request.contains("Authorization: Bearer test-write-token")
                        && request.startsWith("POST /calendar/v3/calendars/primary%40example.com/events?")
                        && request.contains("sendUpdates=all")
                        && request.contains("conferenceDataVersion=1")
                        && body.value("id").toString().size() == 32
                        && body.value("summary").toString() == QStringLiteral("Upload contract")
                        && body.value("location").toString() == QStringLiteral("Before upload")
                        && body.value("recurrence").toArray() == QJsonArray {
                            QStringLiteral("RRULE:FREQ=WEEKLY;BYDAY=MO") }
                        && body.value("attendees").toArray().size() == 1
                        && body.value("reminders").toObject().value("overrides").toArray().size() == 2
                        && body.value("visibility") == QStringLiteral("private")
                        && body.value("transparency") == QStringLiteral("transparent")
                        && body.value("guestsCanModify").toBool()
                        && !body.value("conferenceData").toObject().value("createRequest").toObject()
                                .value("requestId").toString().isEmpty()
                        && body.value("conferenceData").toObject().value("createRequest").toObject()
                                .value("conferenceSolutionKey").toObject().value("type")
                                   == QStringLiteral("hangoutsMeet")
                        && body.value("start").toObject().value("timeZone").toString() == QStringLiteral("America/Phoenix");
                    respond(socket, R"({"id":"google-created-id","summary":"Upload contract","description":"Zoom https://example.zoom.us/j/123 and Teams https://teams.microsoft.com/l/meetup-join/abc","location":"Before upload","start":{"dateTime":"2026-09-21T09:00:00.000-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-21T10:00:00.000-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO"],"attendees":[{"email":"guest@example.com","displayName":"Guest","responseStatus":"accepted"}],"organizer":{"email":"upload@example.com","displayName":"Upload Test","self":true},"reminders":{"useDefault":false,"overrides":[{"method":"popup","minutes":10},{"method":"popup","minutes":60}]},"visibility":"private","transparency":"transparent","guestsCanInviteOthers":false,"guestsCanModify":true,"guestsCanSeeOtherGuests":true,"hangoutLink":"https://meet.google.com/abc-defg-hij","conferenceData":{"entryPoints":[{"entryPointType":"video","uri":"https://meet.google.com/abc-defg-hij"},{"entryPointType":"phone","uri":"tel:+15551234567","label":"+1 555-123-4567","pin":"987654"}]},"iCalUID":"created@example.com","htmlLink":"https://calendar.google.com/created","etag":"etag-1","updated":"2026-09-21T03:00:00Z"})");
                } else if (request.startsWith("GET /calendar/v3/calendars/primary%40example.com/events/google-created-id")) {
                    validConflictLookup = request.contains("Authorization: Bearer test-write-token");
                    respond(socket, R"({"id":"google-created-id","summary":"Upload contract","description":"Changed elsewhere","location":"Before upload","start":{"dateTime":"2026-09-21T09:00:00.000-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-21T10:00:00.000-07:00","timeZone":"America/Phoenix"},"attendees":[{"email":"guest@example.com","displayName":"Guest","responseStatus":"accepted"}],"organizer":{"email":"upload@example.com","displayName":"Upload Test","self":true},"reminders":{"useDefault":false,"overrides":[{"method":"popup","minutes":10},{"method":"popup","minutes":60}]},"visibility":"private","transparency":"transparent","guestsCanInviteOthers":false,"guestsCanModify":true,"guestsCanSeeOtherGuests":true,"hangoutLink":"https://meet.google.com/abc-defg-hij","iCalUID":"created@example.com","htmlLink":"https://calendar.google.com/created","etag":"etag-remote","updated":"2026-09-21T03:30:00Z"})");
                } else {
                    ++updateAttempts;
                    const bool common = request.startsWith("PATCH /calendar/v3/calendars/primary%40example.com/events/google-created-id")
                        && request.contains("Authorization: Bearer test-write-token")
                        && request.startsWith("PATCH /calendar/v3/calendars/primary%40example.com/events/google-created-id?sendUpdates=all")
                        && !body.contains("id")
                        && body.value("summary").toString() == QStringLiteral("Updated contract")
                        && body.value("location").toString() == QStringLiteral("Phoenix")
                        && body.value("attendees").toArray().size() == 1
                        && body.value("visibility") == QStringLiteral("private")
                        && body.value("transparency") == QStringLiteral("transparent");
                    if (updateAttempts == 1) {
                        validUpdateRequest = common && request.toLower().contains("if-match: etag-1");
                        respond(socket, R"({"error":{"message":"Precondition failed"}})", 412,
                                QByteArrayLiteral("Precondition Failed"));
                    } else {
                        validUpdateRequest = validUpdateRequest && common
                            && request.toLower().contains("if-match: etag-remote")
                            && body.value("description").toString() == QStringLiteral("Changed elsewhere");
                        respond(socket, R"({"id":"google-created-id","summary":"Updated contract","description":"Changed elsewhere","location":"Phoenix","start":{"dateTime":"2026-09-21T09:30:00.000-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-21T10:30:00.000-07:00","timeZone":"America/Phoenix"},"attendees":[{"email":"guest@example.com","displayName":"Guest","responseStatus":"accepted"}],"organizer":{"email":"upload@example.com","displayName":"Upload Test","self":true},"reminders":{"useDefault":false,"overrides":[{"method":"popup","minutes":10},{"method":"popup","minutes":60}]},"visibility":"private","transparency":"transparent","guestsCanInviteOthers":false,"guestsCanModify":true,"guestsCanSeeOtherGuests":true,"hangoutLink":"https://meet.google.com/abc-defg-hij","iCalUID":"created@example.com","htmlLink":"https://calendar.google.com/updated","etag":"etag-2","updated":"2026-09-21T04:00:00Z"})");
                    }
                }
            });
        }
    });

    Database database(temporary.filePath("calendar.db"));
    const QString accountId = QStringLiteral("google:upload-test");
    if (!database.open()
        || !database.upsertAccount(accountId, "google", "upload-test", "Upload Test", "upload@example.com", "connected")
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { "id", "primary@example.com" }, { "summary", "Primary" },
            { "timeZone", "America/Phoenix" }, { "accessRole", "owner" }, { "selected", true },
            { "conferenceProperties", QJsonObject { { "allowedConferenceSolutionTypes",
                QJsonArray { QStringLiteral("hangoutsMeet") } } } }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject().value("id").toString();
    const qint64 start = QDateTime(QDate(2026, 9, 21), QTime(9, 0), QTimeZone("America/Phoenix")).toMSecsSinceEpoch();
    const QString localEventId = database.createPendingEvent(QJsonObject {
            { "calendarId", calendarId }, { "title", "Upload contract" },
            { "startMs", double(start) }, { "endMs", double(start + 3600000) },
            { "timeZone", "America/Phoenix" },
            { "attendees", QJsonArray { QJsonObject { { "email", "guest@example.com" } } } },
            { "reminders", QJsonObject { { "useDefault", false }, { "overrides", QJsonArray {
                QJsonObject { { "method", "popup" }, { "minutes", 10 } },
                QJsonObject { { "method", "popup" }, { "minutes", 60 } }
            } } } },
            { "visibility", "private" }, { "transparency", "transparent" },
            { "guestsCanInviteOthers", false }, { "guestsCanModify", true },
            { "guestsCanSeeOtherGuests", true },
            { "createConference", true },
            { "recurrence", QJsonArray { QStringLiteral("RRULE:FREQ=WEEKLY;BYDAY=MO") } }
        });
    if (localEventId.isEmpty() || !database.updatePendingEvent(QJsonObject {
            { "id", localEventId }, { "calendarId", calendarId }, { "title", "Upload contract" },
            { "location", "Before upload" }, { "startMs", double(start) },
            { "endMs", double(start + 3600000) }, { "timeZone", "America/Phoenix" }
        })) return 4;

    GoogleMutations mutations(database);
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &app, [&](bool changed) {
        completed = changed;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("test-write-token"))) return 5;
    loop.exec();
    const QJsonArray events = database.eventsForRange("2026-09-21", "2026-09-21").array();
    if (!completed || !validCreateRequest || !database.nextPendingMutation(accountId).object().isEmpty()
        || events.size() != 1 || events.at(0).toObject().value("id").toString() != QStringLiteral("google-created-id")
        || !events.at(0).toObject().value("isRecurring").toBool()
        || !events.at(0).toObject().value("isSeriesMaster").toBool()
        || events.at(0).toObject().value("seriesId").toString() != QStringLiteral("google-created-id")
        || events.at(0).toObject().value("attendees").toArray().size() != 1
        || events.at(0).toObject().value("organizer").toObject().value("self") != true
        || events.at(0).toObject().value("visibility") != QStringLiteral("private")
        || events.at(0).toObject().value("transparency") != QStringLiteral("transparent")
        || events.at(0).toObject().value("hangoutLink") != QStringLiteral("https://meet.google.com/abc-defg-hij")
        || events.at(0).toObject().value("meetingLinks").toArray().size() != 4)
        return 6;

    if (!database.updatePendingEvent(QJsonObject {
            { "id", "google-created-id" }, { "calendarId", calendarId },
            { "title", "Updated contract" }, { "location", "Phoenix" },
            { "description", "Zoom https://example.zoom.us/j/123 and Teams https://teams.microsoft.com/l/meetup-join/abc" },
            { "startMs", double(start + 1800000) }, { "endMs", double(start + 5400000) },
            { "timeZone", "America/Phoenix" }
        })) return 7;
    const QJsonObject queuedUpdate = database.nextPendingMutation(accountId).object();
    if (queuedUpdate.value("operation").toString() != QStringLiteral("update")
        || queuedUpdate.value("baseEtag").toString() != QStringLiteral("etag-1")) return 8;
    completed = false;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("test-write-token"))) return 9;
    loop.exec();
    const QJsonArray updatedEvents = database.eventsForRange("2026-09-21", "2026-09-21").array();
    if (!completed || !validUpdateRequest || !validConflictLookup || updateAttempts != 2
        || !database.nextPendingMutation(accountId).object().isEmpty()
        || updatedEvents.size() != 1
        || updatedEvents.at(0).toObject().value("title").toString() != QStringLiteral("Updated contract")
        || updatedEvents.at(0).toObject().value("description").toString() != QStringLiteral("Changed elsewhere")
        || updatedEvents.at(0).toObject().value("location").toString() != QStringLiteral("Phoenix")
        || updatedEvents.at(0).toObject().value("etag").toString() != QStringLiteral("etag-2")) {
        std::fprintf(stderr, "update result completed=%d request=%d lookup=%d attempts=%d pending=%d events=%s error=%s\n",
                     completed, validUpdateRequest, validConflictLookup, updateAttempts,
                     database.nextPendingMutation(accountId).object().isEmpty(),
                     qPrintable(QJsonDocument(updatedEvents).toJson(QJsonDocument::Compact)),
                     qPrintable(database.lastError()));
        return 10;
    }

    if (!database.updatePendingEvent(QJsonObject {
            { "id", "google-created-id" }, { "calendarId", calendarId },
            { "title", "Local collision" }, { "location", "Phoenix" },
            { "description", "Changed elsewhere" }, { "startMs", double(start + 1800000) },
            { "endMs", double(start + 5400000) }, { "timeZone", "America/Phoenix" }
        })) return 11;
    const QString conflictingMutationId = database.nextPendingMutation(accountId).object()
                                              .value("id").toString();
    if (database.rebaseUpdateMutation(conflictingMutationId, QJsonObject {
            { "id", "google-created-id" }, { "summary", "Remote collision" },
            { "description", "Changed elsewhere" }, { "location", "Phoenix" },
            { "start", QJsonObject { { "dateTime", "2026-09-21T09:30:00.000-07:00" },
                                      { "timeZone", "America/Phoenix" } } },
            { "end", QJsonObject { { "dateTime", "2026-09-21T10:30:00.000-07:00" },
                                    { "timeZone", "America/Phoenix" } } },
            { "etag", "etag-3" }, { "updated", "2026-09-21T04:30:00Z" }
        }) || !database.lastError().contains(QStringLiteral("title"))) return 12;

    const int requestsBeforeStorageFailure = requestCount;
    {
        const QString connectionName = QStringLiteral("mutation-upload-storage-failure");
        {
            QSqlDatabase sql = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            sql.setDatabaseName(database.path());
            if (!sql.open()) return 13;
            QSqlQuery trigger(sql);
            if (!trigger.exec(QStringLiteral(
                    "CREATE TRIGGER reject_upload_state BEFORE UPDATE OF state ON pending_mutations "
                    "WHEN NEW.state='uploading' BEGIN SELECT RAISE(ABORT, 'simulated storage failure'); END")))
                return 14;
            sql.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
    bool storageFailureFinished = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &app, [&](bool changed) {
        if (!changed) storageFailureFinished = true;
    });
    if (!mutations.start(accountId, QStringLiteral("test-write-token"))) return 15;
    QCoreApplication::processEvents();
    const QJsonObject stoppedMutation = database.nextPendingMutation(accountId).object();
    const QJsonObject mutationStatus = mutations.status().object();
    if (!storageFailureFinished || requestCount != requestsBeforeStorageFailure
        || stoppedMutation.value("id") != conflictingMutationId
        || stoppedMutation.value("state") != QStringLiteral("queued")
        || stoppedMutation.value("attemptCount").toInt() != 0
        || mutationStatus.value("state") != QStringLiteral("idle")
        || !mutationStatus.value("lastError").toString().contains(QStringLiteral("simulated storage failure")))
        return 16;
    return 0;
}
