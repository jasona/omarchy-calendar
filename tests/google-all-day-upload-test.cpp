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

    int requestCount = 0;
    bool validCreate = false;
    bool validUpdate = false;
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
                const QJsonObject start = body.value(QStringLiteral("start")).toObject();
                const QJsonObject end = body.value(QStringLiteral("end")).toObject();
                ++requestCount;
                if (request.startsWith("POST /calendar/v3/calendars/primary%40example.com/events")) {
                    validCreate = request.contains("Authorization: Bearer all-day-token")
                        && start.value(QStringLiteral("date")) == QStringLiteral("2026-03-07")
                        && end.value(QStringLiteral("date")) == QStringLiteral("2026-03-10")
                        && !start.contains(QStringLiteral("dateTime"))
                        && !end.contains(QStringLiteral("dateTime"));
                    respond(socket, R"({"id":"google-all-day","summary":"Conference","description":"","location":"","start":{"date":"2026-03-07"},"end":{"date":"2026-03-10"},"etag":"all-day-1","updated":"2026-03-01T10:00:00Z"})");
                } else {
                    validUpdate = request.startsWith("PATCH /calendar/v3/calendars/primary%40example.com/events/google-all-day")
                        && request.toLower().contains("if-match: all-day-1")
                        && start.value(QStringLiteral("date")) == QStringLiteral("2026-03-08")
                        && end.value(QStringLiteral("date")) == QStringLiteral("2026-03-11")
                        && !start.contains(QStringLiteral("dateTime"))
                        && !end.contains(QStringLiteral("dateTime"));
                    respond(socket, R"({"id":"google-all-day","summary":"Conference","description":"","location":"","start":{"date":"2026-03-08"},"end":{"date":"2026-03-11"},"etag":"all-day-2","updated":"2026-03-01T11:00:00Z"})");
                }
            });
        }
    });

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:all-day-upload");
    if (!database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("all-day-upload"),
                                   QStringLiteral("All-day Upload"), QStringLiteral("dates@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") },
            { QStringLiteral("accessRole"), QStringLiteral("owner") },
            { QStringLiteral("selected"), true }
        } })) return 3;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value(QStringLiteral("id")).toString();
    const QTimeZone zone("America/New_York");
    const qint64 startMs = QDateTime(QDate(2026, 3, 7), QTime(0, 0), zone).toMSecsSinceEpoch();
    const qint64 endMs = QDateTime(QDate(2026, 3, 10), QTime(0, 0), zone).toMSecsSinceEpoch();
    if (database.createPendingEvent(QJsonObject {
            { QStringLiteral("calendarId"), calendarId }, { QStringLiteral("title"), QStringLiteral("Conference") },
            { QStringLiteral("startMs"), double(startMs) }, { QStringLiteral("endMs"), double(endMs) },
            { QStringLiteral("allDay"), true }, { QStringLiteral("allDayStartDate"), QStringLiteral("2026-03-07") },
            { QStringLiteral("allDayEndDate"), QStringLiteral("2026-03-10") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        }).isEmpty()) return 4;

    GoogleMutations mutations(database);
    QEventLoop loop;
    QObject::connect(&mutations, &GoogleMutations::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("all-day-token"))) return 5;
    loop.exec();
    if (!validCreate || requestCount != 1 || !database.nextPendingMutation(accountId).object().isEmpty()) return 6;

    const qint64 movedStartMs = QDateTime(QDate(2026, 3, 8), QTime(0, 0), zone).toMSecsSinceEpoch();
    const qint64 movedEndMs = QDateTime(QDate(2026, 3, 11), QTime(0, 0), zone).toMSecsSinceEpoch();
    if (!database.updatePendingEvent(QJsonObject {
            { QStringLiteral("id"), QStringLiteral("google-all-day") }, { QStringLiteral("calendarId"), calendarId },
            { QStringLiteral("title"), QStringLiteral("Conference") }, { QStringLiteral("startMs"), double(movedStartMs) },
            { QStringLiteral("endMs"), double(movedEndMs) }, { QStringLiteral("allDay"), true },
            { QStringLiteral("allDayStartDate"), QStringLiteral("2026-03-08") },
            { QStringLiteral("allDayEndDate"), QStringLiteral("2026-03-11") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        })) return 7;
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    if (!mutations.start(accountId, QStringLiteral("all-day-token"))) return 8;
    loop.exec();
    const QJsonArray rows = database.eventsForRange(QStringLiteral("2026-03-08"),
                                                     QStringLiteral("2026-03-11")).array();
    if (!validUpdate || requestCount != 2 || rows.size() != 3
        || rows.at(0).toObject().value(QStringLiteral("allDayStartDate")) != QStringLiteral("2026-03-08")
        || rows.at(0).toObject().value(QStringLiteral("allDayEndDate")) != QStringLiteral("2026-03-11")) return 9;
    return 0;
}
