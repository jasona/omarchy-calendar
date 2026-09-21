#include "eventstore.h"

#include <algorithm>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>

EventStore::EventStore(QObject *parent)
    : QObject(parent)
    , m_sourcePath(QDir::homePath() + QStringLiteral("/.local/state/omarchy/calendar-events.json"))
{
    m_service = new QDBusInterface(
        QStringLiteral("org.omarchy.Calendar"),
        QStringLiteral("/org/omarchy/Calendar"),
        QStringLiteral("org.omarchy.Calendar1"),
        QDBusConnection::sessionBus(),
        this);
    if (m_service->isValid()) {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.omarchy.Calendar"),
            QStringLiteral("/org/omarchy/Calendar"),
            QStringLiteral("org.omarchy.Calendar1"),
            QStringLiteral("EventsChanged"),
            this,
            SLOT(reload()));
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.omarchy.Calendar"),
            QStringLiteral("/org/omarchy/Calendar"),
            QStringLiteral("org.omarchy.Calendar1"),
            QStringLiteral("AccountsChanged"),
            this,
            SLOT(forwardAccountsChanged()));
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.omarchy.Calendar"),
            QStringLiteral("/org/omarchy/Calendar"),
            QStringLiteral("org.omarchy.Calendar1"),
            QStringLiteral("ProviderStatusChanged"),
            this,
            SLOT(forwardProviderStatusChanged()));
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.omarchy.Calendar"),
            QStringLiteral("/org/omarchy/Calendar"),
            QStringLiteral("org.omarchy.Calendar1"),
            QStringLiteral("AuthorizationRequired"),
            this,
            SLOT(forwardAuthorizationUrl(QString)));
    } else {
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &EventStore::reload);
    }
    reload();
}

void EventStore::forwardAuthorizationUrl(const QString &url)
{
    emit authorizationRequired(url);
}

void EventStore::forwardAccountsChanged()
{
    emit accountsChanged();
}

void EventStore::forwardProviderStatusChanged()
{
    emit providerStatusChanged();
}

bool EventStore::serviceBacked() const
{
    return m_service && m_service->isValid();
}

void EventStore::ensureWatching()
{
    if (QFile::exists(m_sourcePath) && !m_watcher.files().contains(m_sourcePath))
        m_watcher.addPath(m_sourcePath);
}

void EventStore::reload()
{
    if (serviceBacked()) {
        const auto status = serviceObject(QStringLiteral("GetStatus"));
        m_eventCount = status.value(QStringLiteral("eventCount")).toInt();
        m_lastError = status.value(QStringLiteral("lastError")).toString();
        emit eventsChanged();
        return;
    }

    QFile file(m_sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = tr("Calendar feed is not available yet");
        m_events.clear();
        m_eventCount = 0;
        emit eventsChanged();
        return;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_lastError = tr("Calendar feed could not be read");
        m_events.clear();
        m_eventCount = 0;
        ensureWatching();
        emit eventsChanged();
        return;
    }

    m_events.clear();
    const auto events = document.object().value(QStringLiteral("events")).toArray();
    m_events.reserve(events.size());
    for (const auto &value : events) {
        if (value.isObject())
            m_events.append(value.toObject());
    }
    m_eventCount = m_events.size();

    std::sort(m_events.begin(), m_events.end(), [](const QJsonObject &left, const QJsonObject &right) {
        return left.value(QStringLiteral("start")).toString()
             < right.value(QStringLiteral("start")).toString();
    });

    m_lastError.clear();
    ensureWatching();
    emit eventsChanged();
}

QVariantList EventStore::serviceArray(const QString &method, const QVariantList &arguments) const
{
    if (!serviceBacked())
        return {};
    const QDBusReply<QString> reply = m_service->callWithArgumentList(QDBus::Block, method, arguments);
    if (!reply.isValid())
        return {};
    const auto document = QJsonDocument::fromJson(reply.value().toUtf8());
    return document.isArray() ? document.array().toVariantList() : QVariantList {};
}

QVariantMap EventStore::serviceObject(const QString &method) const
{
    if (!serviceBacked())
        return {};
    const QDBusReply<QString> reply = m_service->call(method);
    if (!reply.isValid())
        return {};
    const auto document = QJsonDocument::fromJson(reply.value().toUtf8());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap {};
}

QVariantMap EventStore::toVariant(const QJsonObject &event)
{
    const auto start = QDateTime::fromString(event.value(QStringLiteral("start")).toString(), Qt::ISODate);
    const auto end = QDateTime::fromString(event.value(QStringLiteral("end")).toString(), Qt::ISODate);

    QVariantMap result;
    result.insert(QStringLiteral("id"), event.value(QStringLiteral("id")).toString());
    result.insert(QStringLiteral("calendarId"), event.value(QStringLiteral("calendarId")).toString());
    result.insert(QStringLiteral("title"), event.value(QStringLiteral("title")).toString());
    result.insert(QStringLiteral("calendarName"), event.value(QStringLiteral("calendarName")).toString());
    result.insert(QStringLiteral("color"), event.value(QStringLiteral("color")).toString());
    result.insert(QStringLiteral("dateKey"), event.value(QStringLiteral("dateKey")).toString());
    result.insert(QStringLiteral("allDay"), event.value(QStringLiteral("allDay")).toBool());
    result.insert(QStringLiteral("location"), event.value(QStringLiteral("location")).toString());
    result.insert(QStringLiteral("eventUrl"), event.value(QStringLiteral("eventUrl")).toString());
    result.insert(QStringLiteral("startMs"), start.isValid() ? start.toMSecsSinceEpoch() : 0);
    result.insert(QStringLiteral("endMs"), end.isValid() ? end.toMSecsSinceEpoch() : 0);
    return result;
}

QVariantList EventStore::withCollisionLayout(QVariantList events)
{
    QMap<QString, QList<int>> days;
    for (int index = 0; index < events.size(); ++index) {
        const auto event = events.at(index).toMap();
        if (!event.value(QStringLiteral("allDay")).toBool())
            days[event.value(QStringLiteral("dateKey")).toString()].append(index);
    }

    for (const auto &indices : days) {
        int clusterStart = 0;
        while (clusterStart < indices.size()) {
            int clusterEnd = clusterStart + 1;
            qint64 latestEnd = events.at(indices.at(clusterStart)).toMap().value(QStringLiteral("endMs")).toLongLong();
            while (clusterEnd < indices.size()) {
                const auto candidate = events.at(indices.at(clusterEnd)).toMap();
                if (candidate.value(QStringLiteral("startMs")).toLongLong() >= latestEnd)
                    break;
                latestEnd = qMax(latestEnd, candidate.value(QStringLiteral("endMs")).toLongLong());
                ++clusterEnd;
            }

            QList<qint64> laneEnds;
            for (int position = clusterStart; position < clusterEnd; ++position) {
                const int eventIndex = indices.at(position);
                auto event = events.at(eventIndex).toMap();
                const qint64 start = event.value(QStringLiteral("startMs")).toLongLong();
                const qint64 end = event.value(QStringLiteral("endMs")).toLongLong();
                int lane = 0;
                while (lane < laneEnds.size() && laneEnds.at(lane) > start)
                    ++lane;
                if (lane == laneEnds.size())
                    laneEnds.append(end);
                else
                    laneEnds[lane] = end;
                event.insert(QStringLiteral("layoutColumn"), lane);
                events[eventIndex] = event;
            }

            for (int position = clusterStart; position < clusterEnd; ++position) {
                const int eventIndex = indices.at(position);
                auto event = events.at(eventIndex).toMap();
                event.insert(QStringLiteral("layoutColumnCount"), laneEnds.size());
                events[eventIndex] = event;
            }
            clusterStart = clusterEnd;
        }
    }
    return events;
}

QVariantList EventStore::eventsForRange(const QString &firstDate, const QString &lastDate) const
{
    if (serviceBacked())
        return withCollisionLayout(serviceArray(QStringLiteral("GetEvents"), { firstDate, lastDate }));

    QVariantList result;
    for (const auto &event : m_events) {
        const QString dateKey = event.value(QStringLiteral("dateKey")).toString();
        if (dateKey >= firstDate && dateKey <= lastDate)
            result.append(toVariant(event));
    }
    return withCollisionLayout(result);
}

QVariantList EventStore::searchEvents(const QString &queryText, int limit) const
{
    if (serviceBacked())
        return serviceArray(QStringLiteral("SearchEvents"), { queryText, limit });

    const QString needle = queryText.trimmed();
    if (needle.isEmpty())
        return {};
    QVariantList result;
    for (const auto &event : m_events) {
        const auto converted = toVariant(event);
        if (converted.value(QStringLiteral("title")).toString().contains(needle, Qt::CaseInsensitive)
            || converted.value(QStringLiteral("location")).toString().contains(needle, Qt::CaseInsensitive)
            || converted.value(QStringLiteral("calendarName")).toString().contains(needle, Qt::CaseInsensitive)) {
            result.append(converted);
            if (result.size() >= qBound(1, limit, 200))
                break;
        }
    }
    return result;
}

QVariantList EventStore::calendars() const
{
    if (serviceBacked())
        return serviceArray(QStringLiteral("GetCalendars"));

    QVariantList result;
    QSet<QString> seen;
    for (const auto &event : m_events) {
        const QString id = event.value(QStringLiteral("calendarId")).toString();
        if (seen.contains(id))
            continue;
        seen.insert(id);
        result.append(QVariantMap {
            { QStringLiteral("id"), id },
            { QStringLiteral("name"), event.value(QStringLiteral("calendarName")).toString() },
            { QStringLiteral("color"), event.value(QStringLiteral("color")).toString() }
        });
    }
    return result;
}

QVariantList EventStore::accounts() const
{
    return serviceArray(QStringLiteral("GetAccounts"));
}

QVariantMap EventStore::providerStatus() const
{
    return serviceObject(QStringLiteral("GetProviderStatus"));
}

bool EventStore::beginGoogleAuthorization() const
{
    if (!serviceBacked())
        return false;
    const QDBusReply<bool> reply = m_service->call(QStringLiteral("BeginGoogleAuthorization"));
    return reply.isValid() && reply.value();
}

bool EventStore::disconnectGoogle() const
{
    if (!serviceBacked())
        return false;
    const QDBusReply<bool> reply = m_service->call(QStringLiteral("DisconnectGoogle"));
    return reply.isValid() && reply.value();
}

bool EventStore::setCalendarSelected(const QString &calendarId, bool selected) const
{
    if (!serviceBacked())
        return false;
    const QDBusReply<bool> reply = m_service->call(
        QStringLiteral("SetCalendarSelected"), calendarId, selected);
    return reply.isValid() && reply.value();
}

QString EventStore::createEvent(const QVariantMap &event) const
{
    if (!serviceBacked())
        return {};
    const QString json = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(event)).toJson(QJsonDocument::Compact));
    const QDBusReply<QString> reply = m_service->call(QStringLiteral("CreateEvent"), json);
    return reply.isValid() ? reply.value() : QString();
}

bool EventStore::syncNow() const
{
    if (!serviceBacked())
        return false;
    const QDBusReply<bool> reply = m_service->call(QStringLiteral("SyncNow"));
    return reply.isValid() && reply.value();
}

QVariantMap EventStore::nextEvent() const
{
    if (serviceBacked())
        return serviceObject(QStringLiteral("GetNextEvent"));

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &event : m_events) {
        const auto converted = toVariant(event);
        if (!converted.value(QStringLiteral("allDay")).toBool()
            && converted.value(QStringLiteral("endMs")).toLongLong() > now) {
            return converted;
        }
    }
    return {};
}
