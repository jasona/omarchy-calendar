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
#include <cstdio>

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
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) {
        std::fprintf(stderr, "setup failed temporary=%d server=%s\n", temporary.isValid(),
                     qPrintable(server.errorString()));
        return 2;
    }
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    bool validMove = false;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray request = socket->readAll();
                validMove = request.startsWith(
                    "POST /calendar/v3/calendars/source%40example.com/events/event-1/move?destination=destination%40example.com&sendUpdates=all ")
                    && request.contains("Authorization: Bearer move-token");
                respond(socket, R"({"id":"event-1","summary":"Move me","start":{"dateTime":"2026-09-21T09:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-21T10:00:00-07:00","timeZone":"America/Phoenix"},"status":"confirmed","transparency":"opaque","iCalUID":"move@example.com","htmlLink":"https://calendar.google.com/moved","etag":"moved-etag","updated":"2026-09-21T20:00:00Z"})");
            });
        }
    });

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:move-test");
    if (!database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("move-test"),
                                   QStringLiteral("Move Test"), QStringLiteral("move@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray {
            QJsonObject { { "id", "source@example.com" }, { "summary", "Source" },
                          { "timeZone", "America/Phoenix" }, { "accessRole", "owner" },
                          { "selected", true } },
            QJsonObject { { "id", "destination@example.com" }, { "summary", "Destination" },
                          { "timeZone", "America/Phoenix" }, { "accessRole", "writer" },
                          { "selected", true } }
        })) return 3;
    const QJsonArray calendars = database.calendarsForAccount(accountId).array();
    QString sourceId;
    QString destinationId;
    for (const QJsonValue &value : calendars) {
        const QJsonObject calendar = value.toObject();
        if (calendar.value("providerCalendarId") == QStringLiteral("source@example.com"))
            sourceId = calendar.value("id").toString();
        if (calendar.value("providerCalendarId") == QStringLiteral("destination@example.com"))
            destinationId = calendar.value("id").toString();
    }
    if (sourceId.isEmpty() || destinationId.isEmpty()) return 4;

    const QJsonObject remote {
        { "id", "event-1" }, { "summary", "Move me" }, { "status", "confirmed" },
        { "start", QJsonObject { { "dateTime", "2026-09-21T09:00:00-07:00" },
                                   { "timeZone", "America/Phoenix" } } },
        { "end", QJsonObject { { "dateTime", "2026-09-21T10:00:00-07:00" },
                                 { "timeZone", "America/Phoenix" } } },
        { "etag", "source-etag" }, { "updated", "2026-09-21T19:00:00Z" }
    };
    if (!database.applyGoogleEvents(accountId, sourceId, QJsonArray { remote },
                                    QStringLiteral("source-sync"), true)) return 5;
    const QJsonObject stored = database.eventsForRange(QStringLiteral("2026-09-21"),
                                                       QStringLiteral("2026-09-21")).array().at(0).toObject();
    if (!database.updatePendingEvent(QJsonObject {
            { "id", "event-1" }, { "calendarId", sourceId },
            { "targetCalendarId", destinationId }, { "title", "Move me" },
            { "startMs", stored.value("startMs") }, { "endMs", stored.value("endMs") },
            { "timeZone", "America/Phoenix" }
        })) return 6;
    const QJsonObject queued = database.nextPendingMutation(accountId).object();
    const QJsonArray optimistic = database.eventsForRange(QStringLiteral("2026-09-21"),
                                                          QStringLiteral("2026-09-21")).array();
    if (queued.value("operation") != QStringLiteral("move")
        || queued.value("providerCalendarId") != QStringLiteral("source@example.com")
        || queued.value("payload").toObject().value("targetProviderCalendarId")
               != QStringLiteral("destination@example.com")
        || optimistic.size() != 1
        || optimistic.at(0).toObject().value("calendarId") != destinationId) return 7;

    // A source refresh while the move is queued must not resurrect a duplicate.
    if (!database.applyGoogleEvents(accountId, sourceId, QJsonArray { remote },
                                    QStringLiteral("source-sync-2"), false)
        || database.eventsForRange(QStringLiteral("2026-09-21"),
                                   QStringLiteral("2026-09-21")).array().size() != 1) return 8;

    GoogleMutations mutations(database);
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &loop, [&](bool changed) {
        completed = changed;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("move-token"))) return 9;
    loop.exec();
    const QJsonArray moved = database.eventsForRange(QStringLiteral("2026-09-21"),
                                                     QStringLiteral("2026-09-21")).array();
    if (!completed || !validMove || !database.nextPendingMutation(accountId).object().isEmpty()
        || moved.size() != 1 || moved.at(0).toObject().value("calendarId") != destinationId
        || moved.at(0).toObject().value("etag") != QStringLiteral("moved-etag")) return 10;

    QJsonObject recurring = remote;
    recurring.insert("id", "recurring-1");
    recurring.insert("recurrence", QJsonArray { QStringLiteral("RRULE:FREQ=WEEKLY") });
    if (!database.applyGoogleEvents(accountId, sourceId, QJsonArray { recurring },
                                    QStringLiteral("source-sync-3"), false)) return 11;
    const QJsonArray allEvents = database.eventsForRange(QStringLiteral("2026-09-21"),
                                                         QStringLiteral("2026-09-21")).array();
    QJsonObject recurringStored;
    for (const QJsonValue &value : allEvents)
        if (value.toObject().value("id") == QStringLiteral("recurring-1")) recurringStored = value.toObject();
    if (recurringStored.isEmpty() || database.updatePendingEvent(QJsonObject {
            { "id", "recurring-1" }, { "calendarId", sourceId },
            { "targetCalendarId", destinationId }, { "title", "Recurring" },
            { "startMs", recurringStored.value("startMs") }, { "endMs", recurringStored.value("endMs") },
            { "timeZone", "America/Phoenix" }
        }) || !database.lastError().contains(QStringLiteral("recurring"))) return 12;
    return 0;
}
