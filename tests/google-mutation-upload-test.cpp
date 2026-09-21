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
    QTemporaryDir temporary;
    QTcpServer server;
    if (!temporary.isValid() || !server.listen(QHostAddress::LocalHost, 0)) return 2;
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());

    bool validRequest = false;
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
                const QJsonObject body = QJsonDocument::fromJson(
                    request.mid(headerEnd + 4, contentLength)).object();
                validRequest = request.startsWith("POST /calendar/v3/calendars/primary%40example.com/events")
                    && request.contains("Authorization: Bearer test-write-token")
                    && body.value("id").toString().size() == 32
                    && body.value("summary").toString() == QStringLiteral("Upload contract")
                    && body.value("start").toObject().value("timeZone").toString() == QStringLiteral("America/Phoenix");
                respond(socket, R"({"id":"google-created-id","iCalUID":"created@example.com","htmlLink":"https://calendar.google.com/created","etag":"etag-1","updated":"2026-09-21T03:00:00Z"})");
            });
        }
    });

    Database database(temporary.filePath("calendar.db"));
    const QString accountId = QStringLiteral("google:upload-test");
    if (!database.open()
        || !database.upsertAccount(accountId, "google", "upload-test", "Upload Test", "upload@example.com", "connected")
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { "id", "primary@example.com" }, { "summary", "Primary" },
            { "timeZone", "America/Phoenix" }, { "accessRole", "owner" }, { "selected", true }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject().value("id").toString();
    const qint64 start = QDateTime(QDate(2026, 9, 21), QTime(9, 0), QTimeZone("America/Phoenix")).toMSecsSinceEpoch();
    if (database.createPendingEvent(QJsonObject {
            { "calendarId", calendarId }, { "title", "Upload contract" },
            { "startMs", double(start) }, { "endMs", double(start + 3600000) },
            { "timeZone", "America/Phoenix" }
        }).isEmpty()) return 4;

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
    if (!completed || !validRequest || !database.nextPendingMutation(accountId).object().isEmpty()
        || events.size() != 1 || events.at(0).toObject().value("id").toString() != QStringLiteral("google-created-id"))
        return 6;
    return 0;
}
