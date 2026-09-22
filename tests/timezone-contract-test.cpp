#include "database.h"
#include "timezonehelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdlib>

int main(int argc, char **argv)
{
    qputenv("TZ", QByteArrayLiteral("America/Phoenix"));
    tzset();
    QCoreApplication app(argc, argv);
    TimeZoneHelper helper;
    if (helper.systemTimeZoneId() != QStringLiteral("America/Phoenix")) return 2;

    const QVariantMap gap = helper.resolveWallTime(QStringLiteral("2026-03-08"),
                                                   QStringLiteral("02:30"),
                                                   QStringLiteral("America/New_York"));
    if (gap.value(QStringLiteral("valid")).toBool()
        || !gap.value(QStringLiteral("gap")).toBool()) return 3;

    const QVariantMap first = helper.resolveWallTime(QStringLiteral("2026-11-01"),
                                                     QStringLiteral("01:30"),
                                                     QStringLiteral("America/New_York"), false);
    const QVariantMap second = helper.resolveWallTime(QStringLiteral("2026-11-01"),
                                                      QStringLiteral("01:30"),
                                                      QStringLiteral("America/New_York"), true);
    if (!first.value(QStringLiteral("valid")).toBool()
        || !first.value(QStringLiteral("ambiguous")).toBool()
        || !second.value(QStringLiteral("ambiguous")).toBool()
        || second.value(QStringLiteral("epochMs")).toLongLong()
             - first.value(QStringLiteral("epochMs")).toLongLong() != 3600000) return 4;
    const QVariantMap reopenedSecond = helper.wallTime(
        second.value(QStringLiteral("epochMs")).toLongLong(), QStringLiteral("America/New_York"));
    if (!reopenedSecond.value(QStringLiteral("ambiguous")).toBool()
        || reopenedSecond.value(QStringLiteral("occurrence")).toInt() != 2) return 16;

    const QVariantMap tokyo = helper.resolveWallTime(QStringLiteral("2026-01-02"),
                                                     QStringLiteral("09:00"),
                                                     QStringLiteral("Asia/Tokyo"));
    if (!tokyo.value(QStringLiteral("valid")).toBool()
        || tokyo.value(QStringLiteral("localDate")).toString() != QStringLiteral("2026-01-01")
        || tokyo.value(QStringLiteral("localTime")).toString() != QStringLiteral("17:00")) return 5;

    struct ZoneCase {
        const char *zone;
        const char *date;
        const char *time;
        const char *instant;
    };
    const ZoneCase matrix[] {
        { "America/New_York", "2026-07-01", "09:00", "2026-07-01T09:00:00-04:00" },
        { "Europe/London", "2026-01-15", "12:00", "2026-01-15T12:00:00+00:00" },
        { "Asia/Kolkata", "2026-07-01", "09:00", "2026-07-01T09:00:00+05:30" },
        { "Australia/Sydney", "2026-01-15", "09:00", "2026-01-15T09:00:00+11:00" }
    };
    for (const ZoneCase &item : matrix) {
        const QVariantMap resolved = helper.resolveWallTime(QString::fromLatin1(item.date),
                                                            QString::fromLatin1(item.time),
                                                            QString::fromLatin1(item.zone));
        const qint64 expected = QDateTime::fromString(QString::fromLatin1(item.instant),
                                                      Qt::ISODate).toMSecsSinceEpoch();
        if (!resolved.value(QStringLiteral("valid")).toBool()
            || resolved.value(QStringLiteral("epochMs")).toLongLong() != expected) return 15;
    }

    bool foundNewYork = false;
    const QVariantList options = helper.options(QDateTime::currentMSecsSinceEpoch(),
                                                { QStringLiteral("America/New_York") });
    if (options.size() < 2
        || options.at(1).toMap().value(QStringLiteral("id")) != QStringLiteral("America/New_York")) return 17;
    for (const QVariant &value : options) {
        if (value.toMap().value(QStringLiteral("id")) == QStringLiteral("America/New_York")) {
            foundNewYork = true;
            break;
        }
    }
    if (!foundNewYork) return 6;

    QTemporaryDir temporary;
    Database database(temporary.filePath(QStringLiteral("calendar.db")));
    const QString accountId = QStringLiteral("google:timezone-test");
    if (!temporary.isValid() || !database.open()
        || !database.upsertAccount(accountId, QStringLiteral("google"), QStringLiteral("timezone-test"),
                                   QStringLiteral("Timezone Test"), QStringLiteral("zone@example.com"),
                                   QStringLiteral("connected"))
        || !database.replaceGoogleCalendars(accountId, QJsonArray { QJsonObject {
            { QStringLiteral("id"), QStringLiteral("primary@example.com") },
            { QStringLiteral("summary"), QStringLiteral("Primary") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") },
            { QStringLiteral("accessRole"), QStringLiteral("owner") },
            { QStringLiteral("selected"), true }
        } })) return 7;
    const QString calendarId = database.calendarsForAccount(accountId).array().at(0).toObject()
                                   .value(QStringLiteral("id")).toString();
    const QJsonObject remote {
        { QStringLiteral("id"), QStringLiteral("new-year") },
        { QStringLiteral("summary"), QStringLiteral("New York midnight") },
        { QStringLiteral("start"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-01-01T00:30:00-05:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        } },
        { QStringLiteral("end"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-01-01T01:30:00-05:00") },
            { QStringLiteral("timeZone"), QStringLiteral("America/New_York") }
        } },
        { QStringLiteral("status"), QStringLiteral("confirmed") },
        { QStringLiteral("etag"), QStringLiteral("zone-etag") }
    };
    const QJsonObject calendarZoneFallback {
        { QStringLiteral("id"), QStringLiteral("calendar-zone-fallback") },
        { QStringLiteral("summary"), QStringLiteral("Calendar zone fallback") },
        { QStringLiteral("start"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-01-02T12:00:00-05:00") }
        } },
        { QStringLiteral("end"), QJsonObject {
            { QStringLiteral("dateTime"), QStringLiteral("2026-01-02T13:00:00-05:00") }
        } },
        { QStringLiteral("status"), QStringLiteral("confirmed") },
        { QStringLiteral("etag"), QStringLiteral("fallback-etag") }
    };
    if (!database.applyGoogleEvents(accountId, calendarId,
                                    QJsonArray { remote, calendarZoneFallback },
                                    QStringLiteral("zone-sync"), true)) return 8;
    const QJsonArray localDay = database.eventsForRange(QStringLiteral("2025-12-31"),
                                                        QStringLiteral("2025-12-31")).array();
    if (localDay.size() != 1
        || localDay.at(0).toObject().value(QStringLiteral("timeZone"))
             != QStringLiteral("America/New_York")
        || localDay.at(0).toObject().value(QStringLiteral("dateKey"))
             != QStringLiteral("2025-12-31")) return 9;
    const QJsonArray fallbackRows = database.eventsForRange(QStringLiteral("2026-01-02"),
                                                            QStringLiteral("2026-01-02")).array();
    if (fallbackRows.size() != 1
        || fallbackRows.at(0).toObject().value(QStringLiteral("timeZone"))
             != QStringLiteral("America/New_York")) return 18;

    const QString migrationPath = temporary.filePath(QStringLiteral("migration.db"));
    {
        Database seed(migrationPath);
        if (!seed.open()) return 10;
    }
    {
        const QString connectionName = QStringLiteral("timezone-migration-seed");
        QSqlDatabase sql = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        sql.setDatabaseName(migrationPath);
        if (!sql.open()) return 11;
        QSqlQuery query(sql);
        if (!query.exec(QStringLiteral("DELETE FROM schema_migrations WHERE version=6"))
            || !query.exec(QStringLiteral(
                "INSERT INTO calendars(id,account_id,provider_calendar_id,name,color,time_zone,access_role,source) "
                "VALUES('migration-calendar','','migration-calendar','Migration','#6c8cdb','America/New_York','owner','google')"))) return 12;
        const qint64 migrationStart = QDateTime::fromString(
            QStringLiteral("2026-01-01T00:30:00-05:00"), Qt::ISODate).toMSecsSinceEpoch();
        const qint64 migrationEnd = QDateTime::fromString(
            QStringLiteral("2026-01-02T01:30:00-05:00"), Qt::ISODate).toMSecsSinceEpoch();
        query.prepare(QStringLiteral(
            "INSERT INTO events(provider_event_id,date_key,calendar_id,start_ms,end_ms,all_day,title,time_zone,source) "
            "VALUES('migration-event',?,'migration-calendar',?,?,0,'Migration event','America/New_York','google')"));
        for (const QString &date : { QStringLiteral("2026-01-01"), QStringLiteral("2026-01-02") }) {
            query.bindValue(0, date);
            query.bindValue(1, migrationStart);
            query.bindValue(2, migrationEnd);
            if (!query.exec()) return 13;
        }
        sql.close();
        sql = {};
        QSqlDatabase::removeDatabase(connectionName);
    }
    {
        Database migrated(migrationPath);
        if (!migrated.open()
            || migrated.status().object().value(QStringLiteral("schemaVersion")).toInt() != 10
            || migrated.eventsForRange(QStringLiteral("2025-12-31"),
                                       QStringLiteral("2026-01-01")).array().size() != 2
            || !migrated.eventsForRange(QStringLiteral("2026-01-02"),
                                        QStringLiteral("2026-01-02")).array().isEmpty()) return 14;
    }
    return 0;
}
