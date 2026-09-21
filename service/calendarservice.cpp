#include "calendarservice.h"

#include "database.h"
#include "googleauth.h"
#include "googlesync.h"
#include "googlemutations.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

CalendarService::CalendarService(Database &database, GoogleAuth &googleAuth, GoogleSync &googleSync,
                                 GoogleMutations &googleMutations,
                                 QString feedPath, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_googleAuth(googleAuth)
    , m_googleSync(googleSync)
    , m_googleMutations(googleMutations)
    , m_feedPath(std::move(feedPath))
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &CalendarService::feedChanged);
    connect(&m_googleAuth, &GoogleAuth::authorizationRequired,
            this, &CalendarService::AuthorizationRequired);
    connect(&m_googleAuth, &GoogleAuth::accountConnected, this, [this] {
        emit AccountsChanged();
        emit ProviderStatusChanged();
    });
    connect(&m_googleAuth, &GoogleAuth::accountDisconnected, this, [this] {
        emit AccountsChanged();
        emit EventsChanged();
        emit ProviderStatusChanged();
    });
    connect(&m_googleAuth, &GoogleAuth::stateChanged,
            this, &CalendarService::ProviderStatusChanged);
    connect(&m_googleAuth, &GoogleAuth::accessTokenReady,
            this, [this](const QString &accountId, const QString &accessToken) {
        m_googleSync.start(accountId, accessToken);
        if (m_googleAuth.writeAccessAvailable())
            m_googleMutations.start(accountId, accessToken);
        emit ProviderStatusChanged();
    });
    connect(&m_googleSync, &GoogleSync::stateChanged,
            this, &CalendarService::ProviderStatusChanged);
    connect(&m_googleSync, &GoogleSync::finished, this, [this](bool changed) {
        emit AccountsChanged();
        emit ProviderStatusChanged();
        if (changed) {
            if (!m_database.exportCompatibilityFeed(m_feedPath))
                qWarning().noquote() << m_database.lastError();
            ensureWatching();
            emit EventsChanged();
        }
    });
    connect(&m_googleMutations, &GoogleMutations::stateChanged,
            this, &CalendarService::ProviderStatusChanged);
    connect(&m_googleMutations, &GoogleMutations::finished, this, [this](bool changed) {
        emit ProviderStatusChanged();
        if (changed) {
            if (!m_database.exportCompatibilityFeed(m_feedPath))
                qWarning().noquote() << m_database.lastError();
            ensureWatching();
            emit EventsChanged();
            QTimer::singleShot(1000, this, [this] { SyncNow(); });
        }
    });
    auto *syncTimer = new QTimer(this);
    syncTimer->setInterval(5 * 60 * 1000);
    connect(syncTimer, &QTimer::timeout, this, [this] { SyncNow(); });
    syncTimer->start();
    ensureWatching();
}

void CalendarService::ensureWatching()
{
    if (QFile::exists(m_feedPath) && !m_watcher.files().contains(m_feedPath))
        m_watcher.addPath(m_feedPath);
}

void CalendarService::feedChanged()
{
    Reload();
    ensureWatching();
}

QString CalendarService::GetEvents(const QString &firstDate, const QString &lastDate) const
{
    return QString::fromUtf8(m_database.eventsForRange(firstDate, lastDate).toJson(QJsonDocument::Compact));
}

QString CalendarService::GetCalendars() const
{
    return QString::fromUtf8(m_database.calendars().toJson(QJsonDocument::Compact));
}

QString CalendarService::GetAccounts() const
{
    return QString::fromUtf8(m_database.accounts().toJson(QJsonDocument::Compact));
}

QString CalendarService::SearchEvents(const QString &queryText, int limit) const
{
    return QString::fromUtf8(m_database.searchEvents(queryText, limit).toJson(QJsonDocument::Compact));
}

QString CalendarService::GetNextEvent() const
{
    return QString::fromUtf8(m_database.nextEvent().toJson(QJsonDocument::Compact));
}

QString CalendarService::GetStatus() const
{
    return QString::fromUtf8(m_database.status().toJson(QJsonDocument::Compact));
}

QString CalendarService::GetProviderStatus() const
{
    QJsonObject status = m_googleAuth.status().object();
    status.insert(QStringLiteral("sync"), m_googleSync.status().object());
    status.insert(QStringLiteral("mutations"), m_googleMutations.status().object());
    return QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact));
}

bool CalendarService::BeginGoogleAuthorization()
{
    return m_googleAuth.beginAuthorization();
}

bool CalendarService::DisconnectGoogle()
{
    if (!m_googleAuth.disconnectAccount())
        return false;
    m_googleSync.cancel();
    m_googleMutations.cancel();
    if (!m_database.importCompatibilityFeed(m_feedPath))
        qWarning().noquote() << m_database.lastError();
    emit EventsChanged();
    return true;
}

bool CalendarService::SetCalendarSelected(const QString &calendarId, bool selected)
{
    if (!m_database.setCalendarSelected(calendarId, selected))
        return false;
    if (!m_database.exportCompatibilityFeed(m_feedPath))
        qWarning().noquote() << m_database.lastError();
    ensureWatching();
    emit EventsChanged();
    if (selected)
        SyncNow();
    return true;
}

QString CalendarService::CreateEvent(const QString &eventJson)
{
    const QJsonDocument document = QJsonDocument::fromJson(eventJson.toUtf8());
    if (!document.isObject())
        return {};
    const QString eventId = m_database.createPendingEvent(document.object());
    if (eventId.isEmpty())
        return {};
    if (!m_database.exportCompatibilityFeed(m_feedPath))
        qWarning().noquote() << m_database.lastError();
    ensureWatching();
    emit EventsChanged();
    if (m_googleAuth.writeAccessAvailable())
        m_googleMutations.start(m_googleAuth.currentAccountId(), m_googleAuth.accessToken());
    return eventId;
}

bool CalendarService::SyncNow()
{
    return m_googleSync.start(m_googleAuth.currentAccountId(), m_googleAuth.accessToken());
}

bool CalendarService::Reload()
{
    if (!m_database.importCompatibilityFeed(m_feedPath))
        return false;
    emit EventsChanged();
    return true;
}
