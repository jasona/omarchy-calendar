#include "database.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QTimeZone>

namespace {
QJsonObject eventFromQuery(const QSqlQuery &query)
{
    return {
        { QStringLiteral("id"), query.value(QStringLiteral("provider_event_id")).toString() },
        { QStringLiteral("calendarId"), query.value(QStringLiteral("calendar_id")).toString() },
        { QStringLiteral("calendarName"), query.value(QStringLiteral("calendar_name")).toString() },
        { QStringLiteral("color"), query.value(QStringLiteral("color")).toString() },
        { QStringLiteral("dateKey"), query.value(QStringLiteral("date_key")).toString() },
        { QStringLiteral("startMs"), query.value(QStringLiteral("start_ms")).toDouble() },
        { QStringLiteral("endMs"), query.value(QStringLiteral("end_ms")).toDouble() },
        { QStringLiteral("allDay"), query.value(QStringLiteral("all_day")).toBool() },
        { QStringLiteral("title"), query.value(QStringLiteral("title")).toString() },
        { QStringLiteral("description"), query.value(QStringLiteral("description")).toString() },
        { QStringLiteral("location"), query.value(QStringLiteral("location")).toString() },
        { QStringLiteral("eventUrl"), query.value(QStringLiteral("event_url")).toString() },
        { QStringLiteral("timeZone"), query.value(QStringLiteral("time_zone")).toString() },
        { QStringLiteral("etag"), query.value(QStringLiteral("etag")).toString() },
        { QStringLiteral("source"), query.value(QStringLiteral("source")).toString() },
        { QStringLiteral("multiDay"), query.value(QStringLiteral("day_count")).toInt() > 1 }
    };
}

QString googleCalendarId(const QString &accountId, const QString &providerCalendarId)
{
    const QByteArray identity = accountId.toUtf8() + '\0' + providerCalendarId.toUtf8();
    return QStringLiteral("google:")
        + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(32));
}

QDateTime googleDateTime(const QJsonObject &value, const QString &fallbackTimeZone)
{
    const QString dateTime = value.value(QStringLiteral("dateTime")).toString();
    if (!dateTime.isEmpty())
        return QDateTime::fromString(dateTime, Qt::ISODate);

    const QDate date = QDate::fromString(value.value(QStringLiteral("date")).toString(), Qt::ISODate);
    const QByteArray zoneName = value.value(QStringLiteral("timeZone")).toString(fallbackTimeZone).toUtf8();
    const QTimeZone zone(zoneName);
    return QDateTime(date, QTime(0, 0), zone.isValid() ? zone : QTimeZone::systemTimeZone());
}
}

Database::Database(QString path)
    : m_path(std::move(path))
    , m_connectionName(QStringLiteral("omarchy-calendar-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    if (m_database.isValid())
        m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

void Database::setError(const QString &context, const QString &detail)
{
    m_lastError = context + QStringLiteral(": ") + detail;
}

bool Database::execute(const QString &statement)
{
    QSqlQuery query(m_database);
    if (query.exec(statement))
        return true;
    setError(QStringLiteral("Database statement failed"), query.lastError().text());
    return false;
}

bool Database::open()
{
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        setError(QStringLiteral("Database directory could not be created"), QFileInfo(m_path).absolutePath());
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_path);
    if (!m_database.open()) {
        setError(QStringLiteral("Database could not be opened"), m_database.lastError().text());
        return false;
    }

    return execute(QStringLiteral("PRAGMA journal_mode=WAL"))
        && execute(QStringLiteral("PRAGMA foreign_keys=ON"))
        && execute(QStringLiteral("PRAGMA synchronous=NORMAL"))
        && migrate();
}

bool Database::migrate()
{
    if (!m_database.transaction()) {
        setError(QStringLiteral("Migration transaction could not start"), m_database.lastError().text());
        return false;
    }

    const QStringList statements {
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS calendars ("
            "id TEXT PRIMARY KEY, name TEXT NOT NULL, color TEXT NOT NULL, source TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS events ("
            "provider_event_id TEXT NOT NULL, date_key TEXT NOT NULL, calendar_id TEXT NOT NULL, "
            "start_ms INTEGER NOT NULL, end_ms INTEGER NOT NULL, all_day INTEGER NOT NULL DEFAULT 0, "
            "title TEXT NOT NULL DEFAULT '', location TEXT NOT NULL DEFAULT '', event_url TEXT NOT NULL DEFAULT '', "
            "source TEXT NOT NULL, PRIMARY KEY(provider_event_id, date_key), "
            "FOREIGN KEY(calendar_id) REFERENCES calendars(id) ON DELETE CASCADE)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS events_date_key_idx ON events(date_key, start_ms)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS events_end_ms_idx ON events(end_ms)"),
        QStringLiteral(
            "INSERT OR IGNORE INTO schema_migrations(version, applied_at) "
            "VALUES(1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))")
    };

    for (const auto &statement : statements) {
        if (!execute(statement)) {
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery versionTwo(m_database);
    if (!versionTwo.exec(QStringLiteral("SELECT 1 FROM schema_migrations WHERE version=2"))) {
        setError(QStringLiteral("Migration version could not be checked"), versionTwo.lastError().text());
        m_database.rollback();
        return false;
    }

    if (!versionTwo.next()) {
        const QStringList versionTwoStatements {
            QStringLiteral(
                "CREATE TABLE accounts ("
                "id TEXT PRIMARY KEY, provider TEXT NOT NULL, provider_account_id TEXT NOT NULL, "
                "display_name TEXT NOT NULL DEFAULT '', email TEXT NOT NULL DEFAULT '', "
                "enabled INTEGER NOT NULL DEFAULT 1, sync_state TEXT NOT NULL DEFAULT 'disconnected', "
                "last_sync_at TEXT NOT NULL DEFAULT '', last_error TEXT NOT NULL DEFAULT '', "
                "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
                "UNIQUE(provider, provider_account_id))"),
            QStringLiteral("ALTER TABLE calendars ADD COLUMN account_id TEXT REFERENCES accounts(id) ON DELETE CASCADE"),
            QStringLiteral("ALTER TABLE calendars ADD COLUMN provider_calendar_id TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE calendars ADD COLUMN time_zone TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE calendars ADD COLUMN access_role TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE calendars ADD COLUMN selected INTEGER NOT NULL DEFAULT 1"),
            QStringLiteral(
                "CREATE UNIQUE INDEX calendars_provider_identity_idx "
                "ON calendars(account_id, provider_calendar_id) WHERE account_id IS NOT NULL"),
            QStringLiteral("DROP INDEX events_date_key_idx"),
            QStringLiteral("DROP INDEX events_end_ms_idx"),
            QStringLiteral("ALTER TABLE events RENAME TO events_v1"),
            QStringLiteral(
                "CREATE TABLE events ("
                "provider_event_id TEXT NOT NULL, date_key TEXT NOT NULL, calendar_id TEXT NOT NULL, "
                "start_ms INTEGER NOT NULL, end_ms INTEGER NOT NULL, all_day INTEGER NOT NULL DEFAULT 0, "
                "title TEXT NOT NULL DEFAULT '', description TEXT NOT NULL DEFAULT '', "
                "location TEXT NOT NULL DEFAULT '', event_url TEXT NOT NULL DEFAULT '', "
                "provider_uid TEXT NOT NULL DEFAULT '', time_zone TEXT NOT NULL DEFAULT '', "
                "status TEXT NOT NULL DEFAULT '', transparency TEXT NOT NULL DEFAULT '', "
                "etag TEXT NOT NULL DEFAULT '', provider_updated_at TEXT NOT NULL DEFAULT '', "
                "raw_json TEXT NOT NULL DEFAULT '', source TEXT NOT NULL, "
                "PRIMARY KEY(calendar_id, provider_event_id, date_key), "
                "FOREIGN KEY(calendar_id) REFERENCES calendars(id) ON DELETE CASCADE)"),
            QStringLiteral(
                "INSERT INTO events(provider_event_id, date_key, calendar_id, start_ms, end_ms, all_day, "
                "title, location, event_url, source) "
                "SELECT provider_event_id, date_key, calendar_id, start_ms, end_ms, all_day, "
                "title, location, event_url, source FROM events_v1"),
            QStringLiteral("DROP TABLE events_v1"),
            QStringLiteral("CREATE INDEX events_date_key_idx ON events(date_key, start_ms)"),
            QStringLiteral("CREATE INDEX events_end_ms_idx ON events(end_ms)"),
            QStringLiteral(
                "CREATE TABLE sync_cursors ("
                "account_id TEXT NOT NULL, calendar_id TEXT NOT NULL, cursor TEXT NOT NULL DEFAULT '', "
                "updated_at TEXT NOT NULL, PRIMARY KEY(account_id, calendar_id), "
                "FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE, "
                "FOREIGN KEY(calendar_id) REFERENCES calendars(id) ON DELETE CASCADE)"),
            QStringLiteral(
                "INSERT INTO schema_migrations(version, applied_at) "
                "VALUES(2, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))")
        };

        for (const auto &statement : versionTwoStatements) {
            if (!execute(statement)) {
                m_database.rollback();
                return false;
            }
        }
    }

    QSqlQuery versionThree(m_database);
    if (!versionThree.exec(QStringLiteral("SELECT 1 FROM schema_migrations WHERE version=3"))) {
        setError(QStringLiteral("Migration version could not be checked"), versionThree.lastError().text());
        m_database.rollback();
        return false;
    }
    if (!versionThree.next()) {
        const QStringList versionThreeStatements {
            QStringLiteral(
                "CREATE TABLE pending_mutations ("
                "id TEXT PRIMARY KEY, account_id TEXT NOT NULL, calendar_id TEXT NOT NULL, "
                "provider_event_id TEXT NOT NULL, operation TEXT NOT NULL, payload_json TEXT NOT NULL, "
                "base_etag TEXT NOT NULL DEFAULT '', state TEXT NOT NULL DEFAULT 'queued', "
                "attempt_count INTEGER NOT NULL DEFAULT 0, last_error TEXT NOT NULL DEFAULT '', "
                "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
                "FOREIGN KEY(account_id) REFERENCES accounts(id) ON DELETE CASCADE, "
                "FOREIGN KEY(calendar_id) REFERENCES calendars(id) ON DELETE CASCADE)"),
            QStringLiteral("CREATE INDEX pending_mutations_state_idx ON pending_mutations(state, created_at)"),
            QStringLiteral(
                "INSERT INTO schema_migrations(version, applied_at) "
                "VALUES(3, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))")
        };
        for (const auto &statement : versionThreeStatements) {
            if (!execute(statement)) {
                m_database.rollback();
                return false;
            }
        }
    }

    QSqlQuery versionFour(m_database);
    if (!versionFour.exec(QStringLiteral("SELECT 1 FROM schema_migrations WHERE version=4"))) {
        setError(QStringLiteral("Migration version could not be checked"), versionFour.lastError().text());
        m_database.rollback();
        return false;
    }
    if (!versionFour.next()) {
        const QStringList versionFourStatements {
            QStringLiteral("ALTER TABLE accounts ADD COLUMN granted_scopes TEXT NOT NULL DEFAULT ''"),
            QStringLiteral(
                "INSERT INTO schema_migrations(version, applied_at) "
                "VALUES(4, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))")
        };
        for (const auto &statement : versionFourStatements) {
            if (!execute(statement)) {
                m_database.rollback();
                return false;
            }
        }
    }

    if (!m_database.commit()) {
        setError(QStringLiteral("Migration transaction could not commit"), m_database.lastError().text());
        return false;
    }
    return true;
}

bool Database::importCompatibilityFeed(const QString &feedPath)
{
    QFile file(feedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Compatibility feed could not be opened"), file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(QStringLiteral("Compatibility feed is invalid"), parseError.errorString());
        return false;
    }

    const auto root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("events")).isArray()) {
        setError(QStringLiteral("Compatibility feed is unsupported"), QStringLiteral("expected version 1"));
        return false;
    }

    if (root.value(QStringLiteral("source")).toString() == QStringLiteral("Omarchy Calendar")) {
        QSqlQuery account(QStringLiteral("SELECT 1 FROM accounts WHERE provider='google' LIMIT 1"), m_database);
        if (account.next()) {
            if (!m_database.transaction()) {
                setError(QStringLiteral("Native calendar repair transaction could not start"), m_database.lastError().text());
                return false;
            }
            if (!execute(QStringLiteral(
                    "UPDATE calendars SET source='google' "
                    "WHERE source='compat-json' AND account_id IS NOT NULL"))
                || !execute(QStringLiteral(
                    "UPDATE events SET source='google' WHERE source='compat-json' AND calendar_id IN ("
                    "SELECT id FROM calendars WHERE source='google' AND account_id IS NOT NULL)"))) {
                m_database.rollback();
                return false;
            }
            if (!m_database.commit()) {
                setError(QStringLiteral("Native calendar repair transaction could not commit"), m_database.lastError().text());
                return false;
            }
            m_lastError.clear();
            return true;
        }
    }

    if (!m_database.transaction()) {
        setError(QStringLiteral("Import transaction could not start"), m_database.lastError().text());
        return false;
    }

    QSqlQuery removeEvents(m_database);
    removeEvents.prepare(QStringLiteral("DELETE FROM events WHERE source = ?"));
    removeEvents.addBindValue(QStringLiteral("compat-json"));
    if (!removeEvents.exec()) {
        setError(QStringLiteral("Old compatibility events could not be replaced"), removeEvents.lastError().text());
        m_database.rollback();
        return false;
    }

    QSqlQuery removeCalendars(m_database);
    removeCalendars.prepare(QStringLiteral("DELETE FROM calendars WHERE source = ?"));
    removeCalendars.addBindValue(QStringLiteral("compat-json"));
    if (!removeCalendars.exec()) {
        setError(QStringLiteral("Old compatibility calendars could not be replaced"), removeCalendars.lastError().text());
        m_database.rollback();
        return false;
    }

    QSqlQuery calendar(m_database);
    calendar.prepare(QStringLiteral(
        "INSERT INTO calendars(id, name, color, source) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name=excluded.name, color=excluded.color, source=excluded.source"));

    QSqlQuery event(m_database);
    event.prepare(QStringLiteral(
        "INSERT INTO events(provider_event_id, date_key, calendar_id, start_ms, end_ms, all_day, "
        "title, location, event_url, source) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(calendar_id, provider_event_id, date_key) DO UPDATE SET "
        "calendar_id=excluded.calendar_id, start_ms=excluded.start_ms, end_ms=excluded.end_ms, "
        "all_day=excluded.all_day, title=excluded.title, location=excluded.location, "
        "event_url=excluded.event_url, source=excluded.source"));

    for (const auto &value : root.value(QStringLiteral("events")).toArray()) {
        if (!value.isObject())
            continue;
        const auto item = value.toObject();
        const QString calendarId = item.value(QStringLiteral("calendarId")).toString();

        calendar.bindValue(0, calendarId);
        calendar.bindValue(1, item.value(QStringLiteral("calendarName")).toString());
        calendar.bindValue(2, item.value(QStringLiteral("color")).toString());
        calendar.bindValue(3, QStringLiteral("compat-json"));
        if (!calendar.exec()) {
            setError(QStringLiteral("Calendar import failed"), calendar.lastError().text());
            m_database.rollback();
            return false;
        }

        const auto start = QDateTime::fromString(item.value(QStringLiteral("start")).toString(), Qt::ISODate);
        const auto end = QDateTime::fromString(item.value(QStringLiteral("end")).toString(), Qt::ISODate);
        event.bindValue(0, item.value(QStringLiteral("id")).toString());
        event.bindValue(1, item.value(QStringLiteral("dateKey")).toString());
        event.bindValue(2, calendarId);
        event.bindValue(3, start.isValid() ? start.toMSecsSinceEpoch() : 0);
        event.bindValue(4, end.isValid() ? end.toMSecsSinceEpoch() : 0);
        event.bindValue(5, item.value(QStringLiteral("allDay")).toBool() ? 1 : 0);
        event.bindValue(6, item.value(QStringLiteral("title")).toString());
        event.bindValue(7, item.value(QStringLiteral("location")).toString());
        event.bindValue(8, item.value(QStringLiteral("eventUrl")).toString());
        event.bindValue(9, QStringLiteral("compat-json"));
        if (!event.exec()) {
            setError(QStringLiteral("Event import failed"), event.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery metadata(m_database);
    metadata.prepare(QStringLiteral(
        "INSERT INTO metadata(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    const QList<QPair<QString, QString>> values {
        { QStringLiteral("compat_feed_synced_at"), root.value(QStringLiteral("syncedAt")).toString() },
        { QStringLiteral("compat_feed_imported_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) }
    };
    for (const auto &[key, value] : values) {
        metadata.bindValue(0, key);
        metadata.bindValue(1, value);
        if (!metadata.exec()) {
            setError(QStringLiteral("Import metadata failed"), metadata.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setError(QStringLiteral("Import transaction could not commit"), m_database.lastError().text());
        return false;
    }

    m_lastError.clear();
    return true;
}

bool Database::exportCompatibilityFeed(const QString &feedPath)
{
    QJsonArray events;
    QSqlQuery query(QStringLiteral(
        "SELECT e.provider_event_id, e.calendar_id, c.name, c.color, e.date_key, "
        "e.start_ms, e.end_ms, e.all_day, e.title, e.location, e.event_url "
        "FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE e.source IN ('google','local-pending') AND c.selected=1 "
        "ORDER BY e.date_key, e.start_ms, e.title"), m_database);
    if (query.lastError().isValid()) {
        setError(QStringLiteral("Compatibility export query failed"), query.lastError().text());
        return false;
    }

    while (query.next()) {
        events.append(QJsonObject {
            { QStringLiteral("id"), query.value(0).toString() },
            { QStringLiteral("calendarId"), query.value(1).toString() },
            { QStringLiteral("calendarName"), query.value(2).toString() },
            { QStringLiteral("color"), query.value(3).toString() },
            { QStringLiteral("dateKey"), query.value(4).toString() },
            { QStringLiteral("start"), QDateTime::fromMSecsSinceEpoch(query.value(5).toLongLong(), QTimeZone::UTC)
                                                  .toString(Qt::ISODateWithMs) },
            { QStringLiteral("end"), QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong(), QTimeZone::UTC)
                                                .toString(Qt::ISODateWithMs) },
            { QStringLiteral("allDay"), query.value(7).toBool() },
            { QStringLiteral("title"), query.value(8).toString() },
            { QStringLiteral("location"), query.value(9).toString() },
            { QStringLiteral("eventUrl"), query.value(10).toString() }
        });
    }

    const QFileInfo outputInfo(feedPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        setError(QStringLiteral("Compatibility export directory could not be created"), outputInfo.absolutePath());
        return false;
    }
    QSaveFile output(feedPath);
    if (!output.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Compatibility export could not be opened"), output.errorString());
        return false;
    }
    const QJsonDocument document(QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("syncedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) },
        { QStringLiteral("source"), QStringLiteral("Omarchy Calendar") },
        { QStringLiteral("events"), events }
    });
    if (output.write(document.toJson(QJsonDocument::Indented)) < 0 || !output.commit()) {
        setError(QStringLiteral("Compatibility export could not be committed"), output.errorString());
        return false;
    }
    QFile::setPermissions(feedPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    m_lastError.clear();
    return true;
}

QJsonDocument Database::eventsForRange(const QString &firstDate, const QString &lastDate) const
{
    QJsonArray events;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT e.provider_event_id, e.calendar_id, c.name AS calendar_name, c.color, e.date_key, "
        "e.start_ms, e.end_ms, e.all_day, e.title, e.description, e.location, e.event_url, "
        "e.time_zone, e.etag, e.source, "
        "(SELECT COUNT(*) FROM events span WHERE span.calendar_id=e.calendar_id "
        "AND span.provider_event_id=e.provider_event_id) AS day_count "
        "FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE e.date_key BETWEEN ? AND ? AND c.selected=1 "
        "AND (c.source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!='')) "
        "ORDER BY e.start_ms, e.title"));
    query.addBindValue(firstDate);
    query.addBindValue(lastDate);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return QJsonDocument(events);
    }
    while (query.next())
        events.append(eventFromQuery(query));
    return QJsonDocument(events);
}

QJsonDocument Database::calendars() const
{
    QJsonArray calendars;
    QSqlQuery query(QStringLiteral(
        "SELECT id, name, color, source, account_id, provider_calendar_id, "
        "time_zone, access_role, selected FROM calendars "
        "WHERE source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!='') "
        "ORDER BY name"), m_database);
    while (query.next()) {
        calendars.append(QJsonObject {
            { QStringLiteral("id"), query.value(0).toString() },
            { QStringLiteral("name"), query.value(1).toString() },
            { QStringLiteral("color"), query.value(2).toString() },
            { QStringLiteral("source"), query.value(3).toString() },
            { QStringLiteral("accountId"), query.value(4).toString() },
            { QStringLiteral("providerCalendarId"), query.value(5).toString() },
            { QStringLiteral("timeZone"), query.value(6).toString() },
            { QStringLiteral("accessRole"), query.value(7).toString() },
            { QStringLiteral("selected"), query.value(8).toBool() }
        });
    }
    return QJsonDocument(calendars);
}

QJsonDocument Database::accounts() const
{
    QJsonArray accounts;
    QSqlQuery query(QStringLiteral(
        "SELECT id, provider, provider_account_id, display_name, email, enabled, "
        "sync_state, last_sync_at, last_error FROM accounts ORDER BY display_name, email"), m_database);
    while (query.next()) {
        accounts.append(QJsonObject {
            { QStringLiteral("id"), query.value(0).toString() },
            { QStringLiteral("provider"), query.value(1).toString() },
            { QStringLiteral("providerAccountId"), query.value(2).toString() },
            { QStringLiteral("displayName"), query.value(3).toString() },
            { QStringLiteral("email"), query.value(4).toString() },
            { QStringLiteral("enabled"), query.value(5).toBool() },
            { QStringLiteral("syncState"), query.value(6).toString() },
            { QStringLiteral("lastSyncAt"), query.value(7).toString() },
            { QStringLiteral("lastError"), query.value(8).toString() }
        });
    }
    return QJsonDocument(accounts);
}

bool Database::upsertAccount(const QString &id, const QString &provider,
                             const QString &providerAccountId, const QString &displayName,
                             const QString &email, const QString &syncState)
{
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO accounts(id, provider, provider_account_id, display_name, email, sync_state, created_at, updated_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
        "provider_account_id=excluded.provider_account_id, display_name=excluded.display_name, "
        "email=excluded.email, sync_state=excluded.sync_state, updated_at=excluded.updated_at"));
    query.addBindValue(id);
    query.addBindValue(provider);
    query.addBindValue(providerAccountId);
    query.addBindValue(displayName);
    query.addBindValue(email);
    query.addBindValue(syncState);
    query.addBindValue(now);
    query.addBindValue(now);
    if (query.exec())
        return true;
    setError(QStringLiteral("Account could not be saved"), query.lastError().text());
    return false;
}

bool Database::updateAccountSyncState(const QString &id, const QString &syncState,
                                      const QString &lastError)
{
    QSqlQuery query(m_database);
    const bool synced = syncState == QStringLiteral("idle");
    query.prepare(synced
        ? QStringLiteral("UPDATE accounts SET sync_state=?, last_error=?, last_sync_at=?, updated_at=? WHERE id=?")
        : QStringLiteral("UPDATE accounts SET sync_state=?, last_error=?, updated_at=? WHERE id=?"));
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    query.addBindValue(syncState);
    query.addBindValue(lastError.isNull() ? QStringLiteral("") : lastError);
    if (synced)
        query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(id);
    if (query.exec())
        return true;
    setError(QStringLiteral("Account state could not be updated"), query.lastError().text());
    return false;
}

bool Database::setAccountGrantedScopes(const QString &id, const QString &scopes)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE accounts SET granted_scopes=?, updated_at=? WHERE id=?"));
    query.addBindValue(scopes);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(id);
    if (query.exec() && query.numRowsAffected() == 1)
        return true;
    setError(QStringLiteral("Google scopes could not be stored"), query.lastError().text());
    return false;
}

QString Database::accountGrantedScopes(const QString &id) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT granted_scopes FROM accounts WHERE id=?"));
    query.addBindValue(id);
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}

bool Database::requeueBlockedMutations(const QString &accountId)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE pending_mutations SET state='queued',last_error='',updated_at=? "
        "WHERE account_id=? AND state='blocked'"));
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(accountId);
    if (query.exec()) return true;
    setError(QStringLiteral("Blocked mutations could not be requeued"), query.lastError().text());
    return false;
}

bool Database::removeAccount(const QString &id)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM accounts WHERE id=?"));
    query.addBindValue(id);
    if (query.exec())
        return true;
    setError(QStringLiteral("Account could not be removed"), query.lastError().text());
    return false;
}

bool Database::setCalendarSelected(const QString &calendarId, bool selected)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE calendars SET selected=? WHERE id=? AND source='google'"));
    query.addBindValue(selected ? 1 : 0);
    query.addBindValue(calendarId);
    if (!query.exec()) {
        setError(QStringLiteral("Calendar selection could not be updated"), query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        setError(QStringLiteral("Calendar selection could not be updated"), QStringLiteral("calendar not found"));
        return false;
    }
    m_lastError.clear();
    return true;
}

QString Database::createPendingEvent(const QJsonObject &event)
{
    const QString calendarId = event.value("calendarId").toString();
    const QString title = event.value("title").toString().trimmed();
    const qint64 startMs = qint64(event.value("startMs").toDouble());
    const qint64 endMs = qint64(event.value("endMs").toDouble());
    if (calendarId.isEmpty() || title.isEmpty() || startMs <= 0 || endMs <= startMs) {
        setError("Event could not be created", "title, calendar, and valid times are required");
        return {};
    }
    QSqlQuery calendar(m_database);
    calendar.prepare("SELECT account_id,time_zone,access_role FROM calendars WHERE id=? AND source='google'");
    calendar.addBindValue(calendarId);
    if (!calendar.exec() || !calendar.next()
        || (calendar.value(2).toString() != "owner" && calendar.value(2).toString() != "writer")) {
        setError("Event could not be created", "calendar is unavailable or read-only");
        return {};
    }
    QTimeZone zone(event.value("timeZone").toString(calendar.value(1).toString()).toUtf8());
    if (!zone.isValid()) zone = QTimeZone::systemTimeZone();
    const QString eventId = "local:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString mutationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonObject payload = event;
    payload.insert("id", eventId);
    payload.insert("googleEventId", QString::fromLatin1(
        QCryptographicHash::hash(eventId.toUtf8(), QCryptographicHash::Sha256).toHex().left(32)));
    payload.insert("timeZone", QString::fromUtf8(zone.id()));
    const QString json = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    if (!m_database.transaction()) return {};
    QSqlQuery row(m_database);
    row.prepare("INSERT INTO events(provider_event_id,date_key,calendar_id,start_ms,end_ms,all_day,title,description,location,time_zone,status,transparency,raw_json,source) VALUES(?,?,?,?,?,?,?,?,?,?,'confirmed','opaque',?,'local-pending')");
    row.addBindValue(eventId);
    row.addBindValue(QDateTime::fromMSecsSinceEpoch(startMs, zone).date().toString(Qt::ISODate));
    row.addBindValue(calendarId); row.addBindValue(startMs); row.addBindValue(endMs);
    row.addBindValue(event.value("allDay").toBool() ? 1 : 0); row.addBindValue(title);
    row.addBindValue(event.value("description").toString(QStringLiteral("")));
    row.addBindValue(event.value("location").toString(QStringLiteral("")));
    row.addBindValue(QString::fromUtf8(zone.id())); row.addBindValue(json);
    QSqlQuery mutation(m_database);
    mutation.prepare("INSERT INTO pending_mutations(id,account_id,calendar_id,provider_event_id,operation,payload_json,created_at,updated_at) VALUES(?,?,?,?,'create',?,?,?)");
    mutation.addBindValue(mutationId); mutation.addBindValue(calendar.value(0).toString());
    mutation.addBindValue(calendarId); mutation.addBindValue(eventId); mutation.addBindValue(json);
    mutation.addBindValue(now); mutation.addBindValue(now);
    if (!row.exec() || !mutation.exec() || !m_database.commit()) {
        setError("Event could not be queued", row.lastError().text() + mutation.lastError().text());
        m_database.rollback();
        return {};
    }
    m_lastError.clear();
    return eventId;
}

bool Database::updatePendingEvent(const QJsonObject &event)
{
    const QString eventId = event.value(QStringLiteral("id")).toString();
    const QString calendarId = event.value(QStringLiteral("calendarId")).toString();
    const QString title = event.value(QStringLiteral("title")).toString().trimmed();
    const qint64 startMs = qint64(event.value(QStringLiteral("startMs")).toDouble());
    const qint64 endMs = qint64(event.value(QStringLiteral("endMs")).toDouble());
    if (eventId.isEmpty() || calendarId.isEmpty() || title.isEmpty()
        || startMs <= 0 || endMs <= startMs) {
        setError(QStringLiteral("Event could not be updated"),
                 QStringLiteral("event, title, calendar, and valid times are required"));
        return false;
    }

    QSqlQuery existing(m_database);
    existing.prepare(QStringLiteral(
        "SELECT c.account_id,c.time_zone,c.access_role,e.etag,e.source,COUNT(*) "
        "FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE e.calendar_id=? AND e.provider_event_id=? GROUP BY e.calendar_id,e.provider_event_id"));
    existing.addBindValue(calendarId);
    existing.addBindValue(eventId);
    if (!existing.exec() || !existing.next()) {
        setError(QStringLiteral("Event could not be updated"), QStringLiteral("event not found"));
        return false;
    }
    const QString accessRole = existing.value(2).toString();
    if ((accessRole != QStringLiteral("owner") && accessRole != QStringLiteral("writer"))
        || existing.value(5).toInt() != 1) {
        setError(QStringLiteral("Event could not be updated"),
                 existing.value(5).toInt() != 1 ? QStringLiteral("multi-day editing is not available yet")
                                                : QStringLiteral("calendar is read-only"));
        return false;
    }

    QTimeZone zone(event.value(QStringLiteral("timeZone")).toString(existing.value(1).toString()).toUtf8());
    if (!zone.isValid()) zone = QTimeZone::systemTimeZone();
    QJsonObject payload = event;
    payload.insert(QStringLiteral("timeZone"), QString::fromUtf8(zone.id()));
    if (existing.value(4).toString() == QStringLiteral("local-pending"))
        payload.insert(QStringLiteral("googleEventId"), QString::fromLatin1(
            QCryptographicHash::hash(eventId.toUtf8(), QCryptographicHash::Sha256).toHex().left(32)));
    const QString json = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!m_database.transaction()) return false;
    QSqlQuery row(m_database);
    row.prepare(QStringLiteral(
        "UPDATE events SET date_key=?,start_ms=?,end_ms=?,all_day=?,title=?,description=?,"
        "location=?,time_zone=? WHERE calendar_id=? AND provider_event_id=?"));
    row.addBindValue(QDateTime::fromMSecsSinceEpoch(startMs, zone).date().toString(Qt::ISODate));
    row.addBindValue(startMs);
    row.addBindValue(endMs);
    row.addBindValue(event.value(QStringLiteral("allDay")).toBool() ? 1 : 0);
    row.addBindValue(title);
    row.addBindValue(event.value(QStringLiteral("description")).toString(QStringLiteral("")));
    row.addBindValue(event.value(QStringLiteral("location")).toString(QStringLiteral("")));
    row.addBindValue(QString::fromUtf8(zone.id()));
    row.addBindValue(calendarId);
    row.addBindValue(eventId);

    bool queued = false;
    QSqlQuery mutation(m_database);
    if (existing.value(4).toString() == QStringLiteral("local-pending")) {
        mutation.prepare(QStringLiteral(
            "UPDATE pending_mutations SET payload_json=?,updated_at=? "
            "WHERE calendar_id=? AND provider_event_id=? AND operation='create'"));
        mutation.addBindValue(json);
        mutation.addBindValue(now);
        mutation.addBindValue(calendarId);
        mutation.addBindValue(eventId);
        queued = mutation.exec() && mutation.numRowsAffected() == 1;
    } else {
        mutation.prepare(QStringLiteral(
            "UPDATE pending_mutations SET payload_json=?,base_etag=?,state='queued',last_error='',updated_at=? "
            "WHERE id=(SELECT id FROM pending_mutations WHERE calendar_id=? AND provider_event_id=? "
            "AND operation='update' AND state IN ('queued','retrying','failed') ORDER BY created_at DESC LIMIT 1)"));
        mutation.addBindValue(json);
        mutation.addBindValue(existing.value(3).toString());
        mutation.addBindValue(now);
        mutation.addBindValue(calendarId);
        mutation.addBindValue(eventId);
        queued = mutation.exec();
        if (queued && mutation.numRowsAffected() == 0) {
            mutation.prepare(QStringLiteral(
                "INSERT INTO pending_mutations(id,account_id,calendar_id,provider_event_id,operation,payload_json,"
                "base_etag,created_at,updated_at) VALUES(?,?,?,?, 'update',?,?,?,?)"));
            mutation.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
            mutation.addBindValue(existing.value(0).toString());
            mutation.addBindValue(calendarId);
            mutation.addBindValue(eventId);
            mutation.addBindValue(json);
            mutation.addBindValue(existing.value(3).toString());
            mutation.addBindValue(now);
            mutation.addBindValue(now);
            queued = mutation.exec();
        }
    }
    if (!row.exec() || row.numRowsAffected() != 1 || !queued || !m_database.commit()) {
        setError(QStringLiteral("Event update could not be queued"),
                 row.lastError().text() + mutation.lastError().text());
        m_database.rollback();
        return false;
    }
    m_lastError.clear();
    return true;
}

QJsonDocument Database::nextPendingMutation(const QString &accountId) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT m.id,m.calendar_id,c.provider_calendar_id,m.provider_event_id,m.operation,"
        "m.payload_json,m.attempt_count,m.state,m.base_etag FROM pending_mutations m "
        "JOIN calendars c ON c.id=m.calendar_id "
        "WHERE m.account_id=? AND m.state IN ('queued','retrying','uploading') "
        "ORDER BY m.created_at LIMIT 1"));
    query.addBindValue(accountId);
    if (!query.exec() || !query.next())
        return QJsonDocument(QJsonObject {});
    const QJsonDocument payload = QJsonDocument::fromJson(query.value(5).toByteArray());
    return QJsonDocument(QJsonObject {
        { QStringLiteral("id"), query.value(0).toString() },
        { QStringLiteral("calendarId"), query.value(1).toString() },
        { QStringLiteral("providerCalendarId"), query.value(2).toString() },
        { QStringLiteral("providerEventId"), query.value(3).toString() },
        { QStringLiteral("operation"), query.value(4).toString() },
        { QStringLiteral("payload"), payload.object() },
        { QStringLiteral("attemptCount"), query.value(6).toInt() },
        { QStringLiteral("state"), query.value(7).toString() },
        { QStringLiteral("baseEtag"), query.value(8).toString() }
    });
}

bool Database::setMutationState(const QString &mutationId, const QString &state,
                                const QString &error, bool incrementAttempt)
{
    QSqlQuery query(m_database);
    query.prepare(incrementAttempt
        ? QStringLiteral("UPDATE pending_mutations SET state=?,last_error=?,attempt_count=attempt_count+1,updated_at=? WHERE id=?")
        : QStringLiteral("UPDATE pending_mutations SET state=?,last_error=?,updated_at=? WHERE id=?"));
    query.addBindValue(state);
    query.addBindValue(error);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.addBindValue(mutationId);
    if (query.exec() && query.numRowsAffected() == 1)
        return true;
    setError(QStringLiteral("Mutation state could not be updated"), query.lastError().text());
    return false;
}

bool Database::completeCreateMutation(const QString &mutationId, const QJsonObject &remoteEvent)
{
    const QString remoteId = remoteEvent.value(QStringLiteral("id")).toString();
    if (remoteId.isEmpty()) {
        setError(QStringLiteral("Google create response was incomplete"), QStringLiteral("missing event id"));
        return false;
    }
    if (!m_database.transaction()) {
        setError(QStringLiteral("Mutation completion transaction could not start"), m_database.lastError().text());
        return false;
    }
    QSqlQuery mutation(m_database);
    mutation.prepare(QStringLiteral("SELECT calendar_id,provider_event_id FROM pending_mutations WHERE id=? AND operation='create'"));
    mutation.addBindValue(mutationId);
    if (!mutation.exec() || !mutation.next()) {
        setError(QStringLiteral("Queued create could not be found"), mutation.lastError().text());
        m_database.rollback();
        return false;
    }
    const QString calendarId = mutation.value(0).toString();
    const QString localId = mutation.value(1).toString();
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral(
        "UPDATE events SET provider_event_id=?,provider_uid=?,event_url=?,etag=?,"
        "provider_updated_at=?,raw_json=?,source='google' "
        "WHERE calendar_id=? AND provider_event_id=? AND source='local-pending'"));
    update.addBindValue(remoteId);
    update.addBindValue(remoteEvent.value(QStringLiteral("iCalUID")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("htmlLink")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("etag")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("updated")).toString(QStringLiteral("")));
    update.addBindValue(QString::fromUtf8(QJsonDocument(remoteEvent).toJson(QJsonDocument::Compact)));
    update.addBindValue(calendarId);
    update.addBindValue(localId);
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM pending_mutations WHERE id=?"));
    remove.addBindValue(mutationId);
    if (!update.exec() || update.numRowsAffected() < 1 || !remove.exec() || !m_database.commit()) {
        setError(QStringLiteral("Google event could not replace its local draft"),
                 update.lastError().text() + remove.lastError().text());
        m_database.rollback();
        return false;
    }
    m_lastError.clear();
    return true;
}

bool Database::completeUpdateMutation(const QString &mutationId, const QJsonObject &remoteEvent)
{
    const QString remoteId = remoteEvent.value(QStringLiteral("id")).toString();
    if (remoteId.isEmpty()) {
        setError(QStringLiteral("Google update response was incomplete"), QStringLiteral("missing event id"));
        return false;
    }
    if (!m_database.transaction()) return false;
    QSqlQuery mutation(m_database);
    mutation.prepare(QStringLiteral(
        "SELECT calendar_id,provider_event_id FROM pending_mutations WHERE id=? AND operation='update'"));
    mutation.addBindValue(mutationId);
    if (!mutation.exec() || !mutation.next()) {
        setError(QStringLiteral("Queued update could not be found"), mutation.lastError().text());
        m_database.rollback();
        return false;
    }
    const QString calendarId = mutation.value(0).toString();
    const QString providerEventId = mutation.value(1).toString();
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral(
        "UPDATE events SET event_url=?,provider_uid=?,etag=?,provider_updated_at=?,raw_json=?,source='google' "
        "WHERE calendar_id=? AND provider_event_id=?"));
    update.addBindValue(remoteEvent.value(QStringLiteral("htmlLink")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("iCalUID")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("etag")).toString(QStringLiteral("")));
    update.addBindValue(remoteEvent.value(QStringLiteral("updated")).toString(QStringLiteral("")));
    update.addBindValue(QString::fromUtf8(QJsonDocument(remoteEvent).toJson(QJsonDocument::Compact)));
    update.addBindValue(calendarId);
    update.addBindValue(providerEventId);
    QSqlQuery advance(m_database);
    advance.prepare(QStringLiteral(
        "UPDATE pending_mutations SET base_etag=?,updated_at=? WHERE calendar_id=? "
        "AND provider_event_id=? AND operation='update' AND id!=?"));
    advance.addBindValue(remoteEvent.value(QStringLiteral("etag")).toString(QStringLiteral("")));
    advance.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    advance.addBindValue(calendarId);
    advance.addBindValue(providerEventId);
    advance.addBindValue(mutationId);
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM pending_mutations WHERE id=?"));
    remove.addBindValue(mutationId);
    if (!update.exec() || update.numRowsAffected() < 1 || !advance.exec()
        || !remove.exec() || !m_database.commit()) {
        setError(QStringLiteral("Google event update could not be reconciled"),
                 update.lastError().text() + remove.lastError().text());
        m_database.rollback();
        return false;
    }
    m_lastError.clear();
    return true;
}

bool Database::setSyncCursor(const QString &accountId, const QString &calendarId,
                             const QString &cursor)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO sync_cursors(account_id, calendar_id, cursor, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(account_id, calendar_id) DO UPDATE SET cursor=excluded.cursor, updated_at=excluded.updated_at"));
    query.addBindValue(accountId);
    query.addBindValue(calendarId);
    query.addBindValue(cursor);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (query.exec())
        return true;
    setError(QStringLiteral("Sync cursor could not be saved"), query.lastError().text());
    return false;
}

bool Database::replaceGoogleCalendars(const QString &accountId, const QJsonArray &items)
{
    if (!m_database.transaction()) {
        setError(QStringLiteral("Calendar sync transaction could not start"), m_database.lastError().text());
        return false;
    }

    QSet<QString> retained;
    QSqlQuery upsert(m_database);
    upsert.prepare(QStringLiteral(
        "INSERT INTO calendars(id, name, color, source, account_id, provider_calendar_id, time_zone, access_role, selected) "
        "VALUES(?, ?, ?, 'google', ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
        "name=excluded.name, color=excluded.color, account_id=excluded.account_id, "
        "provider_calendar_id=excluded.provider_calendar_id, time_zone=excluded.time_zone, "
        "access_role=excluded.access_role, selected=calendars.selected, source='google'"));

    for (const auto &value : items) {
        if (!value.isObject())
            continue;
        const QJsonObject item = value.toObject();
        const QString providerId = item.value(QStringLiteral("id")).toString();
        if (providerId.isEmpty())
            continue;
        const QString internalId = googleCalendarId(accountId, providerId);
        retained.insert(internalId);
        upsert.bindValue(0, internalId);
        upsert.bindValue(1, item.value(QStringLiteral("summaryOverride")).toString(
                                   item.value(QStringLiteral("summary")).toString(providerId)));
        upsert.bindValue(2, item.value(QStringLiteral("backgroundColor")).toString(QStringLiteral("#6c8cdb")));
        upsert.bindValue(3, accountId);
        upsert.bindValue(4, providerId);
        upsert.bindValue(5, item.value(QStringLiteral("timeZone")).toString(QStringLiteral("")));
        upsert.bindValue(6, item.value(QStringLiteral("accessRole")).toString(QStringLiteral("")));
        upsert.bindValue(7, item.value(QStringLiteral("selected")).toBool(true) ? 1 : 0);
        if (!upsert.exec()) {
            setError(QStringLiteral("Google calendar could not be stored"), upsert.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery existing(m_database);
    existing.prepare(QStringLiteral("SELECT id FROM calendars WHERE account_id=? AND source='google'"));
    existing.addBindValue(accountId);
    if (!existing.exec()) {
        setError(QStringLiteral("Existing Google calendars could not be read"), existing.lastError().text());
        m_database.rollback();
        return false;
    }
    QStringList removed;
    while (existing.next()) {
        const QString id = existing.value(0).toString();
        if (!retained.contains(id))
            removed.append(id);
    }
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM calendars WHERE id=?"));
    for (const auto &id : removed) {
        remove.bindValue(0, id);
        if (!remove.exec()) {
            setError(QStringLiteral("Removed Google calendar could not be deleted"), remove.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit()) {
        setError(QStringLiteral("Calendar sync transaction could not commit"), m_database.lastError().text());
        return false;
    }
    return true;
}

QJsonDocument Database::calendarsForAccount(const QString &accountId) const
{
    QJsonArray result;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, provider_calendar_id, name, color, time_zone, access_role, selected "
        "FROM calendars WHERE account_id=? ORDER BY name"));
    query.addBindValue(accountId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return QJsonDocument(result);
    }
    while (query.next()) {
        result.append(QJsonObject {
            { QStringLiteral("id"), query.value(0).toString() },
            { QStringLiteral("providerCalendarId"), query.value(1).toString() },
            { QStringLiteral("name"), query.value(2).toString() },
            { QStringLiteral("color"), query.value(3).toString() },
            { QStringLiteral("timeZone"), query.value(4).toString() },
            { QStringLiteral("accessRole"), query.value(5).toString() },
            { QStringLiteral("selected"), query.value(6).toBool() }
        });
    }
    return QJsonDocument(result);
}

QString Database::syncCursor(const QString &accountId, const QString &calendarId) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT cursor FROM sync_cursors WHERE account_id=? AND calendar_id=?"));
    query.addBindValue(accountId);
    query.addBindValue(calendarId);
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}

bool Database::applyGoogleEvents(const QString &accountId, const QString &calendarId,
                                 const QJsonArray &items, const QString &nextSyncToken,
                                 bool fullSync)
{
    if (nextSyncToken.isEmpty()) {
        setError(QStringLiteral("Google event page is incomplete"), QStringLiteral("missing nextSyncToken"));
        return false;
    }
    if (!m_database.transaction()) {
        setError(QStringLiteral("Event sync transaction could not start"), m_database.lastError().text());
        return false;
    }

    if (fullSync) {
        QSqlQuery clear(m_database);
        clear.prepare(QStringLiteral("DELETE FROM events WHERE calendar_id=? AND source='google'"));
        clear.addBindValue(calendarId);
        if (!clear.exec()) {
            setError(QStringLiteral("Old Google events could not be cleared"), clear.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM events WHERE calendar_id=? AND provider_event_id=?"));
    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
        "INSERT INTO events(provider_event_id, date_key, calendar_id, start_ms, end_ms, all_day, "
        "title, description, location, event_url, provider_uid, time_zone, status, transparency, "
        "etag, provider_updated_at, raw_json, source) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'google')"));

    for (const auto &value : items) {
        if (!value.isObject())
            continue;
        const QJsonObject item = value.toObject();
        const QString providerEventId = item.value(QStringLiteral("id")).toString();
        if (providerEventId.isEmpty())
            continue;

        remove.bindValue(0, calendarId);
        remove.bindValue(1, providerEventId);
        if (!remove.exec()) {
            setError(QStringLiteral("Existing Google event could not be replaced"), remove.lastError().text());
            m_database.rollback();
            return false;
        }
        if (item.value(QStringLiteral("status")).toString() == QStringLiteral("cancelled"))
            continue;

        const QJsonObject startValue = item.value(QStringLiteral("start")).toObject();
        const QJsonObject endValue = item.value(QStringLiteral("end")).toObject();
        const bool allDay = startValue.contains(QStringLiteral("date"));
        const QString timeZone = startValue.value(QStringLiteral("timeZone")).toString();
        const QDateTime start = googleDateTime(startValue, timeZone);
        const QDateTime end = googleDateTime(endValue, timeZone);
        if (!start.isValid() || !end.isValid() || end <= start)
            continue;

        QDate firstDate = allDay
            ? QDate::fromString(startValue.value(QStringLiteral("date")).toString(), Qt::ISODate)
            : start.date();
        QDate lastDate = allDay
            ? QDate::fromString(endValue.value(QStringLiteral("date")).toString(), Qt::ISODate).addDays(-1)
            : end.addMSecs(-1).date();
        for (QDate date = firstDate; date.isValid() && date <= lastDate; date = date.addDays(1)) {
            int column = 0;
            insert.bindValue(column++, providerEventId);
            insert.bindValue(column++, date.toString(Qt::ISODate));
            insert.bindValue(column++, calendarId);
            insert.bindValue(column++, start.toMSecsSinceEpoch());
            insert.bindValue(column++, end.toMSecsSinceEpoch());
            insert.bindValue(column++, allDay ? 1 : 0);
            insert.bindValue(column++, item.value(QStringLiteral("summary")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("description")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("location")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("htmlLink")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("iCalUID")).toString(QStringLiteral("")));
            insert.bindValue(column++, timeZone.isNull() ? QStringLiteral("") : timeZone);
            insert.bindValue(column++, item.value(QStringLiteral("status")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("transparency")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("etag")).toString(QStringLiteral("")));
            insert.bindValue(column++, item.value(QStringLiteral("updated")).toString(QStringLiteral("")));
            insert.bindValue(column++, QString::fromUtf8(QJsonDocument(item).toJson(QJsonDocument::Compact)));
            if (!insert.exec()) {
                setError(QStringLiteral("Google event could not be stored"), insert.lastError().text());
                m_database.rollback();
                return false;
            }
        }
    }

    QSqlQuery cursor(m_database);
    cursor.prepare(QStringLiteral(
        "INSERT INTO sync_cursors(account_id, calendar_id, cursor, updated_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(account_id, calendar_id) DO UPDATE SET cursor=excluded.cursor, updated_at=excluded.updated_at"));
    cursor.addBindValue(accountId);
    cursor.addBindValue(calendarId);
    cursor.addBindValue(nextSyncToken);
    cursor.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!cursor.exec()) {
        setError(QStringLiteral("Google sync cursor could not be stored"), cursor.lastError().text());
        m_database.rollback();
        return false;
    }

    if (!m_database.commit()) {
        setError(QStringLiteral("Event sync transaction could not commit"), m_database.lastError().text());
        return false;
    }
    return true;
}

QJsonDocument Database::searchEvents(const QString &queryText, int limit) const
{
    QJsonArray events;
    const QString term = queryText.trimmed();
    if (term.isEmpty())
        return QJsonDocument(events);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT e.provider_event_id, e.calendar_id, c.name AS calendar_name, c.color, e.date_key, "
        "e.start_ms, e.end_ms, e.all_day, e.title, e.description, e.location, e.event_url, "
        "e.time_zone, e.etag, e.source, "
        "(SELECT COUNT(*) FROM events span WHERE span.calendar_id=e.calendar_id "
        "AND span.provider_event_id=e.provider_event_id) AS day_count "
        "FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE e.rowid IN (SELECT MIN(rowid) FROM events GROUP BY provider_event_id) "
        "AND c.selected=1 "
        "AND (c.source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!='')) "
        "AND (instr(lower(e.title), lower(?))>0 OR instr(lower(e.location), lower(?))>0 "
        "OR instr(lower(c.name), lower(?))>0) "
        "ORDER BY CASE WHEN e.end_ms>=? THEN 0 ELSE 1 END, "
        "CASE WHEN e.end_ms>=? THEN e.start_ms END ASC, "
        "CASE WHEN e.end_ms<? THEN e.start_ms END DESC LIMIT ?"));
    query.addBindValue(term);
    query.addBindValue(term);
    query.addBindValue(term);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(qBound(1, limit, 200));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return QJsonDocument(events);
    }
    while (query.next())
        events.append(eventFromQuery(query));
    return QJsonDocument(events);
}

QJsonDocument Database::nextEvent() const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT e.provider_event_id, e.calendar_id, c.name AS calendar_name, c.color, e.date_key, "
        "e.start_ms, e.end_ms, e.all_day, e.title, e.description, e.location, e.event_url, "
        "e.time_zone, e.etag, e.source, "
        "(SELECT COUNT(*) FROM events span WHERE span.calendar_id=e.calendar_id "
        "AND span.provider_event_id=e.provider_event_id) AS day_count "
        "FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE e.all_day=0 AND e.end_ms>? AND c.selected=1 "
        "AND (c.source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!='')) "
        "ORDER BY e.start_ms LIMIT 1"));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (query.exec() && query.next())
        return QJsonDocument(eventFromQuery(query));
    return QJsonDocument(QJsonObject {});
}

QJsonDocument Database::status() const
{
    QSqlQuery counts(QStringLiteral(
        "SELECT (SELECT COUNT(*) FROM calendars c WHERE c.source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!='')), "
        "(SELECT COUNT(*) FROM events e JOIN calendars c ON c.id=e.calendar_id "
        "WHERE c.selected=1 AND (c.source!='compat-json' OR NOT EXISTS ("
        "SELECT 1 FROM accounts WHERE provider='google' AND last_sync_at!=''))), "
        "(SELECT COUNT(*) FROM accounts), "
        "(SELECT COUNT(*) FROM pending_mutations), "
        "(SELECT MAX(value) FROM metadata WHERE key='compat_feed_synced_at')"), m_database);
    QJsonObject result {
        { QStringLiteral("schemaVersion"), 4 },
        { QStringLiteral("databasePath"), m_path },
        { QStringLiteral("lastError"), m_lastError }
    };
    if (counts.next()) {
        result.insert(QStringLiteral("calendarCount"), counts.value(0).toInt());
        result.insert(QStringLiteral("eventCount"), counts.value(1).toInt());
        result.insert(QStringLiteral("accountCount"), counts.value(2).toInt());
        result.insert(QStringLiteral("pendingMutationCount"), counts.value(3).toInt());
        result.insert(QStringLiteral("feedSyncedAt"), counts.value(4).toString());
    }
    return QJsonDocument(result);
}
