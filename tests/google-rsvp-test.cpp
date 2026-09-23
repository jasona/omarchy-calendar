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
    // Invitations must still be upcoming when this contract runs.
    const QString eventDate = QDateTime::currentDateTimeUtc().addDays(2).date().toString(Qt::ISODate);
    QTemporaryDir temporary;
    QTcpServer server;
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) return 2;
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    bool validPatch = false;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                QByteArray request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                const int headerEnd = request.indexOf("\r\n\r\n");
                if (headerEnd < 0) return;
                int length = 0;
                for (const QByteArray &line : request.left(headerEnd).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (request.size() < headerEnd + 4 + length) return;
                const QJsonObject body = QJsonDocument::fromJson(
                    request.mid(headerEnd + 4, length)).object();
                const QJsonArray attendees = body.value("attendees").toArray();
                validPatch = request.startsWith(
                    "PATCH /calendar/v3/calendars/primary%40example.com/events/invite-1 ")
                    && !request.contains("sendUpdates=")
                    && request.toLower().contains("if-match: invite-etag-1")
                    && attendees.size() == 2
                    && attendees.at(0).toObject().value("responseStatus") == QStringLiteral("accepted")
                    && attendees.at(1).toObject().value("email") == QStringLiteral("other@example.com");
                respond(socket, QByteArray(R"({"id":"invite-1","summary":"Project kickoff","organizer":{"email":"owner@example.com"},"attendees":[{"email":"me@example.com","self":true,"responseStatus":"accepted"},{"email":"other@example.com","responseStatus":"tentative"}],"start":{"dateTime":"2026-09-22T09:00:00-07:00","timeZone":"America/Phoenix"},"end":{"dateTime":"2026-09-22T10:00:00-07:00","timeZone":"America/Phoenix"},"etag":"invite-etag-2","updated":"2026-09-21T22:00:00Z","status":"confirmed"})")
                                    .replace("2026-09-22", eventDate.toUtf8()));
            });
        }
    });

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:rsvp-test");
    if (!database.open()
        || !database.upsertAccount(accountId, "google", "rsvp-test", "RSVP Test",
                                   "me@example.com", "connected")
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { "id", "primary@example.com" }, { "summary", "Primary" },
            { "timeZone", "America/Phoenix" }, { "accessRole", "owner" }, { "selected", true }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject().value("id").toString();
    const QJsonObject invitation {
        { "id", "invite-1" }, { "summary", "Project kickoff" },
        { "organizer", QJsonObject { { "email", "owner@example.com" } } },
        { "attendees", QJsonArray {
            QJsonObject { { "email", "me@example.com" }, { "self", true }, { "responseStatus", "needsAction" } },
            QJsonObject { { "email", "other@example.com" }, { "responseStatus", "tentative" } }
        } },
        { "start", QJsonObject { { "dateTime", eventDate + "T09:00:00-07:00" }, { "timeZone", "America/Phoenix" } } },
        { "end", QJsonObject { { "dateTime", eventDate + "T10:00:00-07:00" }, { "timeZone", "America/Phoenix" } } },
        { "etag", "invite-etag-1" }, { "updated", "2026-09-21T21:00:00Z" }, { "status", "confirmed" }
    };
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { invitation }, "rsvp-sync", true)) return 4;
    const QJsonArray notifications = database.takeNewInvitations().array();
    if (notifications.size() != 1
        || notifications.at(0).toObject().value("title") != QStringLiteral("Project kickoff")
        || !database.takeNewInvitations().array().isEmpty()
        || !database.respondPendingEvent(calendarId, "invite-1", "accepted")) return 4;
    const QJsonObject optimistic = database.eventsForRange(eventDate, eventDate).array().at(0).toObject();
    if (!optimistic.value("canRespond").toBool()
        || optimistic.value("selfResponseStatus") != QStringLiteral("accepted")
        || database.nextPendingMutation(accountId).object().value("operation") != QStringLiteral("rsvp")) return 5;

    GoogleMutations mutations(database);
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &loop, [&](bool changed) {
        completed = changed;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, "rsvp-token")) return 6;
    loop.exec();
    const QJsonObject stored = database.eventsForRange(eventDate, eventDate).array().at(0).toObject();
    if (!completed || !validPatch || !database.pendingMutations().array().isEmpty()
        || stored.value("selfResponseStatus") != QStringLiteral("accepted")
        || stored.value("etag") != QStringLiteral("invite-etag-2")) return 7;

    if (!database.respondPendingEvent(calendarId, "invite-1", "tentative")) return 8;
    const QString mutationId = database.nextPendingMutation(accountId).object().value("id").toString();
    QJsonObject changed = invitation;
    changed.insert("etag", "invite-etag-3");
    changed.insert("attendees", QJsonArray {
        QJsonObject { { "email", "me@example.com" }, { "self", true }, { "responseStatus", "needsAction" } },
        QJsonObject { { "email", "new@example.com" }, { "responseStatus", "accepted" } }
    });
    if (!database.rebaseRsvpMutation(mutationId, changed)) return 9;
    const QJsonObject rebased = database.nextPendingMutation(accountId).object();
    const QJsonArray merged = rebased.value("payload").toObject().value("attendees").toArray();
    if (rebased.value("baseEtag") != QStringLiteral("invite-etag-3") || merged.size() != 2
        || merged.at(0).toObject().value("responseStatus") != QStringLiteral("tentative")
        || merged.at(1).toObject().value("email") != QStringLiteral("new@example.com")) return 10;
    return 0;
}
