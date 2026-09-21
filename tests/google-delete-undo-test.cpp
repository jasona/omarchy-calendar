#include "database.h"
#include "googlemutations.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    QTcpServer server;
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) return 2;
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    bool validDelete = false;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray request = socket->readAll();
                if (!request.contains("\r\n\r\n")) return;
                validDelete = request.startsWith(
                    "DELETE /calendar/v3/calendars/primary%40example.com/events/google-delete-id")
                    && request.contains("Authorization: Bearer delete-token")
                    && request.toLower().contains("if-match: delete-etag");
                socket->write("HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                socket->disconnectFromHost();
            });
        }
    });

    Database database(temporary.filePath("calendar.db"));
    const QString accountId = QStringLiteral("google:delete-test");
    if (!database.open()
        || !database.upsertAccount(accountId, "google", "delete-test", "Delete Test",
                                   "delete@example.com", "connected")
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { "id", "primary@example.com" }, { "summary", "Primary" },
            { "timeZone", "America/Phoenix" }, { "accessRole", "owner" }, { "selected", true }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value("id").toString();
    const QJsonObject remote {
        { "id", "google-delete-id" }, { "summary", "Delete contract" },
        { "description", "Restorable" }, { "location", "Phoenix" },
        { "start", QJsonObject { { "dateTime", "2026-09-22T09:00:00-07:00" },
                                  { "timeZone", "America/Phoenix" } } },
        { "end", QJsonObject { { "dateTime", "2026-09-22T10:00:00-07:00" },
                                { "timeZone", "America/Phoenix" } } },
        { "etag", "delete-etag" }, { "status", "confirmed" },
        { "updated", "2026-09-22T01:00:00Z" }
    };
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { remote }, "delete-sync", true))
        return 4;

    QString mutationId = database.deletePendingEvent(calendarId, QStringLiteral("google-delete-id"));
    if (mutationId.isEmpty()
        || !database.eventsForRange("2026-09-22", "2026-09-22").array().isEmpty()
        || !database.nextPendingMutation(accountId).object().isEmpty()) return 5;
    if (!database.applyGoogleEvents(accountId, calendarId, QJsonArray { remote }, "delete-sync-2", false)
        || !database.eventsForRange("2026-09-22", "2026-09-22").array().isEmpty()
        || !database.undoPendingDelete(mutationId)) return 5;
    const QJsonArray restored = database.eventsForRange("2026-09-22", "2026-09-22").array();
    if (restored.size() != 1 || restored.at(0).toObject().value("etag") != QStringLiteral("delete-etag"))
        return 6;

    mutationId = database.deletePendingEvent(calendarId, QStringLiteral("google-delete-id"));
    if (mutationId.isEmpty() || !database.finalizePendingDelete(mutationId)) return 7;
    const QJsonObject queued = database.nextPendingMutation(accountId).object();
    if (queued.value("operation") != QStringLiteral("delete")
        || queued.value("baseEtag") != QStringLiteral("delete-etag")) return 8;

    GoogleMutations mutations(database);
    QEventLoop loop;
    bool completed = false;
    QObject::connect(&mutations, &GoogleMutations::finished, &app, [&](bool changed) {
        completed = changed;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("delete-token"))) return 9;
    loop.exec();
    if (!completed || !validDelete || !database.nextPendingMutation(accountId).object().isEmpty())
        return 10;

    const qint64 startMs = QDateTime::fromString("2026-09-23T09:00:00-07:00", Qt::ISODate).toMSecsSinceEpoch();
    const QString localId = database.createPendingEvent(QJsonObject {
        { "calendarId", calendarId }, { "title", "Local draft" },
        { "startMs", double(startMs) }, { "endMs", double(startMs + 3600000) },
        { "timeZone", "America/Phoenix" }
    });
    const QString cancelId = database.deletePendingEvent(calendarId, localId);
    if (localId.isEmpty() || cancelId.isEmpty()
        || !database.nextPendingMutation(accountId).object().isEmpty()
        || !database.undoPendingDelete(cancelId)) return 11;
    const QJsonObject create = database.nextPendingMutation(accountId).object();
    if (create.value("operation") != QStringLiteral("create")
        || create.value("providerEventId") != localId) return 12;
    return 0;
}
