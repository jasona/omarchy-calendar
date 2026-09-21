#include "database.h"
#include "googlemutations.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QTimeZone>

namespace {
void respond(QTcpSocket *socket, const QByteArray &body)
{
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                  + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    socket->disconnectFromHost();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    QTcpServer server;
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) return 2;
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    bool fetchedParent = false;
    bool patchedParent = false;
    bool createdFuture = false;
    bool truncatedParent = false;
    bool canceledFuture = false;
    int parentFetches = 0;
    int parentPatches = 0;
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
                if (request.startsWith("GET /calendar/v3/calendars/primary%40example.com/events/series-1")) {
                    fetchedParent = request.contains("Authorization: Bearer series-token");
                    ++parentFetches;
                    respond(socket, R"({"id":"series-1","summary":"Weekly review","start":{"dateTime":"2026-09-07T09:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-07T10:00:00-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO;COUNT=5"],"etag":"parent-etag-1"})");
                    return;
                }
                const QJsonObject body = QJsonDocument::fromJson(
                    request.mid(headerEnd + 4, contentLength)).object();
                if (request.startsWith("POST /calendar/v3/calendars/primary%40example.com/events")) {
                    createdFuture = body.value(QStringLiteral("summary")) == QStringLiteral("Future updated")
                        && body.value(QStringLiteral("id")).toString().size() == 32
                        && body.value(QStringLiteral("start")).toObject().value(QStringLiteral("dateTime"))
                            .toString().startsWith(QStringLiteral("2026-09-21T11:00:00"))
                        && body.value(QStringLiteral("recurrence")).toArray().at(0).toString()
                            .contains(QStringLiteral("COUNT=3"));
                    respond(socket, R"({"id":"future-series","summary":"Future updated","start":{"dateTime":"2026-09-21T11:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-21T12:00:00-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO;COUNT=3"],"etag":"future-etag"})");
                    return;
                }
                ++parentPatches;
                if (parentPatches > 2) {
                    canceledFuture = request.startsWith(
                        "PATCH /calendar/v3/calendars/primary%40example.com/events/series-1")
                        && request.toLower().contains("if-match: parent-etag-1")
                        && body.size() == 1
                        && body.value(QStringLiteral("recurrence")).toArray().at(0).toString()
                            .contains(QStringLiteral("COUNT=2"));
                    respond(socket, R"({"id":"series-1","summary":"Weekly review","start":{"dateTime":"2026-09-07T09:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-07T10:00:00-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO;COUNT=2"],"etag":"parent-etag-4"})");
                    return;
                }
                if (parentPatches > 1) {
                    truncatedParent = body.value(QStringLiteral("recurrence")).toArray().at(0).toString()
                        .contains(QStringLiteral("COUNT=2"));
                    respond(socket, R"({"id":"series-1","summary":"Series updated","start":{"dateTime":"2026-09-07T10:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-07T11:00:00-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO;COUNT=2"],"etag":"parent-etag-3"})");
                    return;
                }
                patchedParent = request.startsWith("PATCH /calendar/v3/calendars/primary%40example.com/events/series-1")
                    && request.toLower().contains("if-match: parent-etag-1")
                    && body.value(QStringLiteral("summary")) == QStringLiteral("Series updated")
                    && body.value(QStringLiteral("start")).toObject().value(QStringLiteral("dateTime"))
                        .toString().startsWith(QStringLiteral("2026-09-07T10:00:00"))
                    && body.value(QStringLiteral("end")).toObject().value(QStringLiteral("dateTime"))
                        .toString().startsWith(QStringLiteral("2026-09-07T11:00:00"))
                    && !body.contains(QStringLiteral("recurrence"));
                respond(socket, R"({"id":"series-1","summary":"Series updated","start":{"dateTime":"2026-09-07T10:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-07T11:00:00-07:00","timeZone":"America/Phoenix"},"recurrence":["RRULE:FREQ=WEEKLY;BYDAY=MO"],"etag":"parent-etag-2","updated":"2026-09-21T20:00:00Z"})");
            });
        }
    });

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:series-update");
    if (!database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("series-update"),
                                   QStringLiteral("Series Update"), QStringLiteral("series@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") },
            { QStringLiteral("accessRole"), QStringLiteral("owner") },
            { QStringLiteral("selected"), true }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value(QStringLiteral("id")).toString();
    const QJsonObject instance {
        { QStringLiteral("id"), QStringLiteral("instance-1") },
        { QStringLiteral("recurringEventId"), QStringLiteral("series-1") },
        { QStringLiteral("summary"), QStringLiteral("Weekly review") },
        { QStringLiteral("originalStartTime"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-09-21T09:00:00-07:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") }
        } },
        { QStringLiteral("start"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-09-21T09:00:00-07:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") }
        } },
        { QStringLiteral("end"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-09-21T10:00:00-07:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") }
        } },
        { QStringLiteral("etag"), QStringLiteral("instance-etag") },
        { QStringLiteral("status"), QStringLiteral("confirmed") }
    };
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { instance },
                                    QStringLiteral("series-sync"), true)) return 4;
    const QJsonObject stored = database.eventsForRange(QStringLiteral("2026-09-21"),
                                                       QStringLiteral("2026-09-21")).array().at(0).toObject();
    const qint64 startMs = qint64(stored.value(QStringLiteral("startMs")).toDouble());
    if (!database.updatePendingEvent(QJsonObject {
            { QStringLiteral("id"), QStringLiteral("instance-1") },
            { QStringLiteral("calendarId"), calendarId },
            { QStringLiteral("title"), QStringLiteral("Series updated") },
            { QStringLiteral("startMs"), double(startMs + 3600000) },
            { QStringLiteral("endMs"), double(startMs + 7200000) },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") },
            { QStringLiteral("scope"), QStringLiteral("series") },
            { QStringLiteral("seriesId"), QStringLiteral("series-1") },
            { QStringLiteral("scopeBaseStartMs"), double(startMs) }
        })) return 5;

    GoogleMutations mutations(database);
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &loop, [&](bool changed) {
        completed = changed;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("series-token"))) return 6;
    loop.exec();
    if (!completed || !fetchedParent || !patchedParent
        || !database.nextPendingMutation(accountId).object().isEmpty()) return 7;

    if (!database.updatePendingEvent(QJsonObject {
            { QStringLiteral("id"), QStringLiteral("instance-1") },
            { QStringLiteral("calendarId"), calendarId },
            { QStringLiteral("title"), QStringLiteral("Future updated") },
            { QStringLiteral("startMs"), double(startMs + 7200000) },
            { QStringLiteral("endMs"), double(startMs + 10800000) },
            { QStringLiteral("timeZone"), QStringLiteral("America/Phoenix") },
            { QStringLiteral("scope"), QStringLiteral("future") },
            { QStringLiteral("seriesId"), QStringLiteral("series-1") },
            { QStringLiteral("scopeBaseStartMs"), double(startMs + 3600000) },
            { QStringLiteral("scopeOriginalStartMs"), double(startMs) }
        })) return 8;
    completed = false;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("series-token"))) return 9;
    loop.exec();
    if (!completed || parentFetches != 2 || !createdFuture || !truncatedParent
        || !database.nextPendingMutation(accountId).object().isEmpty()) return 10;

    QString deletion = database.deletePendingEvent(QJsonObject {
        { QStringLiteral("calendarId"), calendarId },
        { QStringLiteral("id"), QStringLiteral("instance-1") },
        { QStringLiteral("scope"), QStringLiteral("future") },
        { QStringLiteral("seriesId"), QStringLiteral("series-1") },
        { QStringLiteral("originalStartMs"), double(startMs) }
    });
    if (deletion.isEmpty()) return 11;
    if (!database.undoPendingDelete(deletion)) return 15;
    if (database.eventsForRange(QStringLiteral("2026-09-21"),
                                QStringLiteral("2026-09-21")).array().size() != 1) return 16;
    deletion = database.deletePendingEvent(QJsonObject {
        { QStringLiteral("calendarId"), calendarId },
        { QStringLiteral("id"), QStringLiteral("instance-1") },
        { QStringLiteral("scope"), QStringLiteral("future") },
        { QStringLiteral("seriesId"), QStringLiteral("series-1") },
        { QStringLiteral("originalStartMs"), double(startMs) }
    });
    if (deletion.isEmpty() || !database.finalizePendingDelete(deletion)) return 12;
    completed = false;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("series-token"))) return 13;
    loop.exec();
    if (!completed || parentFetches != 3 || !canceledFuture
        || !database.nextPendingMutation(accountId).object().isEmpty()) return 14;
    return 0;
}
