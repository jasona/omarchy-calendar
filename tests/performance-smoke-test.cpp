#include "database.h"

#include <QCoreApplication>
#include <QDate>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    QJsonArray events;
    const QDate first(2026, 1, 1);
    constexpr int eventCount = 20000;
    for (int index = 0; index < eventCount; ++index) {
        const QDate date = first.addDays(index % 365);
        const QDateTime start(date, QTime(8 + index % 10, (index % 4) * 15));
        events.append(QJsonObject {
            { QStringLiteral("id"), QStringLiteral("event-%1").arg(index) },
            { QStringLiteral("calendarId"), QStringLiteral("performance") },
            { QStringLiteral("calendarName"), QStringLiteral("Performance") },
            { QStringLiteral("color"), QStringLiteral("#6c8cdb") },
            { QStringLiteral("dateKey"), date.toString(Qt::ISODate) },
            { QStringLiteral("start"), start.toString(Qt::ISODate) },
            { QStringLiteral("end"), start.addSecs(1800).toString(Qt::ISODate) },
            { QStringLiteral("title"), index % 1000 == 0
                ? QStringLiteral("Needle performance event") : QStringLiteral("Routine event %1").arg(index) },
            { QStringLiteral("location"), QStringLiteral("Local") },
            { QStringLiteral("eventUrl"), QStringLiteral("https://example.invalid/event") },
            { QStringLiteral("allDay"), false }
        });
    }
    QFile feed(temporary.filePath(QStringLiteral("feed.json")));
    if (!feed.open(QIODevice::WriteOnly)
        || feed.write(QJsonDocument(QJsonObject {
            { QStringLiteral("version"), 1 }, { QStringLiteral("source"), QStringLiteral("performance-test") },
            { QStringLiteral("syncedAt"), QStringLiteral("2026-01-01T00:00:00Z") },
            { QStringLiteral("events"), events }
        }).toJson(QJsonDocument::Compact)) < 1) return 3;
    feed.close();

    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    QElapsedTimer timer;
    timer.start();
    if (!database.open() || !database.importCompatibilityFeed(feed.fileName())) {
        std::fprintf(stderr, "%s\n", qPrintable(database.lastError()));
        return 4;
    }
    const qint64 importMs = timer.restart();
    for (int index = 0; index < 250; ++index) {
        const QDate date = first.addDays(index % 358);
        if (database.eventsForRange(date.toString(Qt::ISODate), date.addDays(6).toString(Qt::ISODate))
                .array().isEmpty()) return 5;
    }
    const qint64 rangesMs = timer.restart();
    for (int index = 0; index < 25; ++index) {
        if (database.searchEvents(QStringLiteral("Needle"), 100).array().size() != 20) return 6;
    }
    const qint64 searchesMs = timer.elapsed();
    std::printf("events=%d import_ms=%lld range_250_ms=%lld search_25_ms=%lld\n", eventCount,
                static_cast<long long>(importMs), static_cast<long long>(rangesMs),
                static_cast<long long>(searchesMs));
    if (importMs > 15000 || rangesMs > 5000 || searchesMs > 5000) return 7;
    return 0;
}
