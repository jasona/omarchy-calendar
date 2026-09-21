#include "database.h"
#include "googlesync.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

namespace {
void respond(QTcpSocket *socket, int status, const QByteArray &reason, const QByteArray &body)
{
    const QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason
        + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
        + QByteArray::number(body.size()) + "\r\n\r\n" + body;
    socket->write(response);
    socket->disconnectFromHost();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid())
        return 2;

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0))
        return 3;
    qputenv("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
            QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
    qputenv("OMARCHY_CALENDAR_SYNC_RETRY_BASE_SECONDS", "1");

    int calendarRequests = 0;
    int eventRequests = 0;
    bool sawIncrementalRequest = false;
    bool sawFullRequest = false;
    bool rejectAuthorization = false;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&] {
        while (auto *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray request = socket->property("requestBuffer").toByteArray()
                    + socket->readAll();
                if (!request.contains("\r\n\r\n"))
                {
                    socket->setProperty("requestBuffer", request);
                    return;
                }
                if (request.startsWith("GET /calendar/v3/users/me/calendarList")) {
                    ++calendarRequests;
                    if (rejectAuthorization) {
                        respond(socket, 401, "Unauthorized",
                                R"({"error":{"message":"Google authorization was revoked"}})");
                    } else if (calendarRequests == 1) {
                        respond(socket, 503, "Service Unavailable",
                                R"({"error":{"message":"Temporary Google outage"}})");
                    } else if (calendarRequests == 2) {
                        respond(socket, 429, "Too Many Requests",
                                R"({"error":{"message":"Google rate limit reached"}})");
                    } else {
                        respond(socket, 200, "OK",
                                R"({"items":[{"id":"primary@example.com","summary":"Primary","backgroundColor":"#6c8cdb","timeZone":"America/Phoenix","accessRole":"owner","selected":true}]})");
                    }
                } else if (request.startsWith("GET /calendar/v3/calendars/")) {
                    ++eventRequests;
                    if (eventRequests == 1) {
                        sawIncrementalRequest = request.contains("syncToken=stale-token");
                        respond(socket, 410, "Gone",
                                R"({"error":{"message":"Sync token is no longer valid"}})");
                    } else {
                        sawFullRequest = request.contains("timeMin=")
                            && !request.contains("syncToken=");
                        respond(socket, 200, "OK",
                                R"({"items":[],"nextSyncToken":"retry-test-token"})");
                    }
                } else {
                    respond(socket, 404, "Not Found", R"({"error":{"message":"Unexpected test request"}})");
                }
            });
        }
    });

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    if (!database.open())
        return 4;
    const QString accountId = QStringLiteral("google:retry-test");
    if (!database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("retry-test"),
                                QStringLiteral("Retry Test"), QStringLiteral("retry@example.com"),
                                QStringLiteral("connected")))
        return 5;
    const QJsonArray seededCalendars {
        QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("selected"), true }
        }
    };
    if (!database.replaceGoogleCalendars(accountId, seededCalendars))
        return 6;
    const auto seeded = database.calendarsForAccount(accountId).array();
    if (seeded.size() != 1)
        return 7;
    const QString seededCalendarId = seeded.at(0).toObject().value(QStringLiteral("id")).toString();
    if (!database.setSyncCursor(accountId, seededCalendarId, QStringLiteral("stale-token")))
        return 8;

    GoogleSync sync(database);
    bool sawServerRetry = false;
    bool sawRateLimitRetry = false;
    bool completed = false;
    QEventLoop loop;
    QObject::connect(&sync, &GoogleSync::stateChanged, &app, [&] {
        const auto status = sync.status().object();
        if (status.value(QStringLiteral("state")).toString() == QStringLiteral("retrying")) {
            const QString error = status.value(QStringLiteral("lastError")).toString();
            sawServerRetry = sawServerRetry || error == QStringLiteral("Temporary Google outage");
            sawRateLimitRetry = sawRateLimitRetry || error == QStringLiteral("Google rate limit reached");
        }
    });
    QObject::connect(&sync, &GoogleSync::finished, &app, [&](bool changed) {
        if (changed) {
            completed = true;
            loop.quit();
        }
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(10000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();

    if (!sync.start(accountId, QStringLiteral("test-access-token")))
        return 9;
    loop.exec();

    const auto accounts = database.accounts().array();
    const auto calendars = database.calendarsForAccount(accountId).array();
    if (!completed || !sawServerRetry || !sawRateLimitRetry
        || !sawIncrementalRequest || !sawFullRequest
        || calendarRequests != 3 || eventRequests != 2
        || accounts.size() != 1 || calendars.size() != 1)
        return 10;
    const QString calendarId = calendars.at(0).toObject().value(QStringLiteral("id")).toString();
    if (database.syncCursor(accountId, calendarId) != QStringLiteral("retry-test-token")
        || accounts.at(0).toObject().value(QStringLiteral("syncState")).toString() != QStringLiteral("idle")
        || sync.status().object().value(QStringLiteral("lastSuccessAt")).toString().isEmpty())
        return 11;

    rejectAuthorization = true;
    GoogleSync revokedSync(database);
    bool revokedFinished = false;
    QEventLoop revokedLoop;
    QObject::connect(&revokedSync, &GoogleSync::finished, &app, [&](bool changed) {
        revokedFinished = !changed;
        revokedLoop.quit();
    });
    QTimer revokedTimeout;
    revokedTimeout.setSingleShot(true);
    revokedTimeout.setInterval(3000);
    QObject::connect(&revokedTimeout, &QTimer::timeout, &revokedLoop, &QEventLoop::quit);
    revokedTimeout.start();
    if (!revokedSync.start(accountId, QStringLiteral("revoked-access-token")))
        return 12;
    revokedLoop.exec();

    const auto revokedAccounts = database.accounts().array();
    if (!revokedFinished || calendarRequests != 4 || eventRequests != 2
        || revokedAccounts.size() != 1
        || revokedAccounts.at(0).toObject().value(QStringLiteral("syncState")).toString()
            != QStringLiteral("error")
        || revokedAccounts.at(0).toObject().value(QStringLiteral("lastError")).toString()
            != QStringLiteral("Google authorization was revoked")
        || database.calendarsForAccount(accountId).array().size() != 1
        || database.syncCursor(accountId, calendarId) != QStringLiteral("retry-test-token"))
        return 13;
    return 0;
}
