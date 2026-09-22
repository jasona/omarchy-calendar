#include "googlesync.h"

#include "database.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

namespace {
QString responseError(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QString message = document.object().value(QStringLiteral("error"))
                                .toObject().value(QStringLiteral("message")).toString();
    return message.isEmpty() ? fallback : message;
}

bool retryableStatus(int status)
{
    return status == 0 || status == 408 || status == 429 || status >= 500;
}
}

GoogleSync::GoogleSync(Database &database, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_network(new QNetworkAccessManager(this))
    , m_retryTimer(new QTimer(this))
    , m_apiBaseUrl(qEnvironmentVariable("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
                                        QStringLiteral("https://www.googleapis.com")))
{
    while (m_apiBaseUrl.endsWith('/'))
        m_apiBaseUrl.chop(1);
    bool retryBaseValid = false;
    const int retryBase = qEnvironmentVariableIntValue(
        "OMARCHY_CALENDAR_SYNC_RETRY_BASE_SECONDS", &retryBaseValid);
    if (retryBaseValid && retryBase > 0)
        m_retryBaseSeconds = retryBase;
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this] {
        if (!m_retryPending || !m_online)
            return;
        m_retryPending = false;
        beginAttempt();
    });

    if (!QNetworkInformation::instance())
        QNetworkInformation::loadDefaultBackend();
    if (auto *information = QNetworkInformation::instance()) {
        m_online = information->reachability() != QNetworkInformation::Reachability::Disconnected;
        connect(information, &QNetworkInformation::reachabilityChanged, this,
                [this](QNetworkInformation::Reachability reachability) {
            const bool online = reachability != QNetworkInformation::Reachability::Disconnected;
            if (m_online == online)
                return;
            m_online = online;
            emit stateChanged();
            if (m_online && m_retryPending && !m_busy) {
                m_retryTimer->stop();
                m_retryPending = false;
                beginAttempt();
            }
        });
    }
}

bool GoogleSync::start(const QString &accountId, const QString &accessToken)
{
    if (m_busy || accountId.isEmpty() || accessToken.isEmpty())
        return false;
    m_accountId = accountId;
    m_accessToken = accessToken;
    m_retryTimer->stop();
    m_retryPending = false;
    m_failureCount = 0;
    beginAttempt();
    return true;
}

void GoogleSync::cancel()
{
    m_retryTimer->stop();
    for (auto *reply : m_network->findChildren<QNetworkReply *>())
        reply->abort();
    m_retryPending = false;
    m_busy = false;
    m_accountId.clear();
    m_accessToken.clear();
    m_currentStage.clear();
    m_lastError.clear();
    m_retryAt.clear();
    clearWorkingSet();
    emit stateChanged();
}

void GoogleSync::clearWorkingSet()
{
    m_calendarItems = QJsonArray {};
    m_eventItems = QJsonArray {};
    m_calendarQueue.clear();
    m_currentCalendar.clear();
}

void GoogleSync::beginAttempt()
{
    m_calendarItems = {};
    m_eventItems = {};
    m_calendarQueue.clear();
    m_currentCalendar.clear();
    m_changed = false;
    m_calendarsCompleted = 0;
    m_calendarsTotal = 0;
    m_currentStage = QStringLiteral("Loading calendar list");
    m_lastError.clear();
    m_retryAt.clear();
    m_lastAttemptAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    m_busy = true;
    m_database.updateAccountSyncState(m_accountId, QStringLiteral("syncing"));
    emit stateChanged();
    if (!m_online) {
        fail(QStringLiteral("Waiting for a network connection"));
        return;
    }
    requestCalendarPage();
}

QNetworkReply *GoogleSync::get(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Omarchy Calendar/0.1"));
    return m_network->get(request);
}

void GoogleSync::requestCalendarPage(const QString &pageToken)
{
    QUrl url(m_apiBaseUrl + QStringLiteral("/calendar/v3/users/me/calendarList"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("maxResults"), QStringLiteral("250"));
    query.addQueryItem(QStringLiteral("showHidden"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("colorRgbFormat"), QStringLiteral("true"));
    if (!pageToken.isEmpty())
        query.addQueryItem(QStringLiteral("pageToken"), pageToken);
    url.setQuery(query);
    auto *reply = get(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleCalendarPage(reply); });
}

void GoogleSync::handleCalendarPage(QNetworkReply *reply)
{
    if (!m_busy) {
        reply->deleteLater();
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkError = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (status < 200 || status >= 300) {
        const QString fallback = status == 0
            ? networkError
            : QStringLiteral("Google Calendar List request failed (%1)").arg(status);
        fail(responseError(body, fallback), retryableStatus(status));
        return;
    }
    const auto document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        fail(QStringLiteral("Google Calendar List returned invalid JSON"));
        return;
    }
    const QJsonObject root = document.object();
    for (const auto &item : root.value(QStringLiteral("items")).toArray())
        m_calendarItems.append(item);
    const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString();
    if (!nextPageToken.isEmpty()) {
        requestCalendarPage(nextPageToken);
        return;
    }
    if (!m_database.replaceGoogleCalendars(m_accountId, m_calendarItems)) {
        fail(m_database.lastError(), false);
        return;
    }
    for (const auto &value : m_database.calendarsForAccount(m_accountId).array()) {
        const QVariantMap calendar = value.toObject().toVariantMap();
        if (calendar.value(QStringLiteral("selected")).toBool())
            m_calendarQueue.enqueue(calendar);
    }
    m_calendarsTotal = m_calendarQueue.size();
    m_changed = true;
    syncNextCalendar();
}

void GoogleSync::syncNextCalendar()
{
    if (m_calendarQueue.isEmpty()) {
        complete();
        return;
    }
    m_currentCalendar = m_calendarQueue.dequeue();
    m_currentStage = m_currentCalendar.value(QStringLiteral("name")).toString();
    m_eventItems = {};
    m_fullSync = m_database.syncCursor(m_accountId, m_currentCalendar.value(QStringLiteral("id")).toString()).isEmpty();
    emit stateChanged();
    requestEventPage();
}

void GoogleSync::requestEventPage(const QString &pageToken)
{
    const QByteArray calendarId = QUrl::toPercentEncoding(
        m_currentCalendar.value(QStringLiteral("providerCalendarId")).toString());
    QUrl url = QUrl::fromEncoded(
        m_apiBaseUrl.toUtf8() + QByteArrayLiteral("/calendar/v3/calendars/")
        + calendarId + QByteArrayLiteral("/events"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("maxResults"), QStringLiteral("2500"));
    query.addQueryItem(QStringLiteral("singleEvents"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("showDeleted"), QStringLiteral("true"));
    if (!pageToken.isEmpty())
        query.addQueryItem(QStringLiteral("pageToken"), pageToken);
    if (m_fullSync) {
        query.addQueryItem(QStringLiteral("timeMin"), QDateTime::currentDateTimeUtc().addYears(-1).toString(Qt::ISODate));
        query.addQueryItem(QStringLiteral("timeMax"), QDateTime::currentDateTimeUtc().addYears(3).toString(Qt::ISODate));
    } else {
        query.addQueryItem(QStringLiteral("syncToken"), m_database.syncCursor(
                               m_accountId, m_currentCalendar.value(QStringLiteral("id")).toString()));
    }
    url.setQuery(query);
    auto *reply = get(url);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleEventPage(reply); });
}

void GoogleSync::handleEventPage(QNetworkReply *reply)
{
    if (!m_busy) {
        reply->deleteLater();
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkError = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (status == 410 && !m_fullSync) {
        m_fullSync = true;
        m_eventItems = {};
        requestEventPage();
        return;
    }
    if (status < 200 || status >= 300) {
        const QString fallback = status == 0
            ? networkError
            : QStringLiteral("Google Events request failed for %1 (%2)")
                  .arg(m_currentCalendar.value(QStringLiteral("name")).toString()).arg(status);
        fail(responseError(body, fallback), retryableStatus(status));
        return;
    }
    const auto document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        fail(QStringLiteral("Google Events returned invalid JSON"));
        return;
    }
    const QJsonObject root = document.object();
    for (const auto &item : root.value(QStringLiteral("items")).toArray())
        m_eventItems.append(item);
    const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString();
    if (!nextPageToken.isEmpty()) {
        requestEventPage(nextPageToken);
        return;
    }
    const QString nextSyncToken = root.value(QStringLiteral("nextSyncToken")).toString();
    if (!m_database.applyGoogleEvents(
            m_accountId, m_currentCalendar.value(QStringLiteral("id")).toString(),
            m_eventItems, nextSyncToken, m_fullSync)) {
        fail(m_database.lastError(), false);
        return;
    }
    m_changed = true;
    ++m_calendarsCompleted;
    syncNextCalendar();
}

void GoogleSync::fail(const QString &message, bool retryable)
{
    m_lastError = message;
    m_busy = false;
    if (retryable)
        scheduleRetry();
    m_database.updateAccountSyncState(m_accountId,
                                      m_retryPending ? QStringLiteral("retrying")
                                                     : QStringLiteral("error"), message);
    clearWorkingSet();
    emit stateChanged();
    emit finished(false);
}

void GoogleSync::scheduleRetry()
{
    const int delaySeconds = std::min(m_retryBaseSeconds * (1 << std::min(m_failureCount, 4)), 300);
    ++m_failureCount;
    m_retryPending = true;
    if (m_online) {
        m_retryAt = QDateTime::currentDateTimeUtc().addSecs(delaySeconds).toString(Qt::ISODateWithMs);
        m_retryTimer->start(delaySeconds * 1000);
    } else {
        m_retryAt.clear();
    }
}

void GoogleSync::complete()
{
    m_database.updateAccountSyncState(m_accountId, QStringLiteral("idle"));
    m_busy = false;
    m_retryPending = false;
    m_failureCount = 0;
    m_lastError.clear();
    m_retryAt.clear();
    m_currentStage.clear();
    m_lastSuccessAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    clearWorkingSet();
    emit stateChanged();
    emit finished(m_changed);
}

QJsonDocument GoogleSync::status() const
{
    const QString state = m_busy ? QStringLiteral("syncing")
                                 : m_retryPending ? QStringLiteral("retrying")
                                                  : QStringLiteral("idle");
    int retryInSeconds = 0;
    if (!m_retryAt.isEmpty())
        retryInSeconds = std::max(0, static_cast<int>(QDateTime::currentDateTimeUtc().secsTo(
            QDateTime::fromString(m_retryAt, Qt::ISODateWithMs))));
    return QJsonDocument(QJsonObject {
        { QStringLiteral("state"), state },
        { QStringLiteral("online"), m_online },
        { QStringLiteral("stage"), m_currentStage },
        { QStringLiteral("calendarsCompleted"), m_calendarsCompleted },
        { QStringLiteral("calendarsTotal"), m_calendarsTotal },
        { QStringLiteral("lastAttemptAt"), m_lastAttemptAt },
        { QStringLiteral("lastSuccessAt"), m_lastSuccessAt },
        { QStringLiteral("lastError"), m_lastError },
        { QStringLiteral("retryAt"), m_retryAt },
        { QStringLiteral("retryInSeconds"), retryInSeconds }
    });
}
