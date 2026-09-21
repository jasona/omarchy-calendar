#include "googlemutations.h"

#include "database.h"

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>

namespace {
QString responseError(const QByteArray &body, const QString &fallback)
{
    const QString message = QJsonDocument::fromJson(body).object().value(QStringLiteral("error"))
                                .toObject().value(QStringLiteral("message")).toString();
    return message.isEmpty() ? fallback : message;
}

QJsonObject googleEventBody(const QJsonObject &payload, bool includeId)
{
    const qint64 startMs = qint64(payload.value(QStringLiteral("startMs")).toDouble());
    const qint64 endMs = qint64(payload.value(QStringLiteral("endMs")).toDouble());
    QTimeZone zone(payload.value(QStringLiteral("timeZone")).toString().toUtf8());
    if (!zone.isValid()) zone = QTimeZone::systemTimeZone();
    QJsonObject start;
    QJsonObject end;
    if (payload.value(QStringLiteral("allDay")).toBool()) {
        start.insert(QStringLiteral("date"), QDateTime::fromMSecsSinceEpoch(startMs, zone).date().toString(Qt::ISODate));
        end.insert(QStringLiteral("date"), QDateTime::fromMSecsSinceEpoch(endMs, zone).date().toString(Qt::ISODate));
    } else {
        start.insert(QStringLiteral("dateTime"), QDateTime::fromMSecsSinceEpoch(startMs, zone).toString(Qt::ISODateWithMs));
        start.insert(QStringLiteral("timeZone"), QString::fromUtf8(zone.id()));
        end.insert(QStringLiteral("dateTime"), QDateTime::fromMSecsSinceEpoch(endMs, zone).toString(Qt::ISODateWithMs));
        end.insert(QStringLiteral("timeZone"), QString::fromUtf8(zone.id()));
    }
    QJsonObject body {
        { QStringLiteral("summary"), payload.value(QStringLiteral("title")) },
        { QStringLiteral("description"), payload.value(QStringLiteral("description")) },
        { QStringLiteral("location"), payload.value(QStringLiteral("location")) },
        { QStringLiteral("start"), start },
        { QStringLiteral("end"), end }
    };
    if (includeId)
        body.insert(QStringLiteral("id"), payload.value(QStringLiteral("googleEventId")));
    return body;
}
}

GoogleMutations::GoogleMutations(Database &database, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_network(new QNetworkAccessManager(this))
    , m_retryTimer(new QTimer(this))
    , m_apiBaseUrl(qEnvironmentVariable("OMARCHY_CALENDAR_GOOGLE_API_BASE_URL",
                                        QStringLiteral("https://www.googleapis.com")))
{
    while (m_apiBaseUrl.endsWith('/')) m_apiBaseUrl.chop(1);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this] {
        m_retryPending = false;
        processNext();
    });
}

bool GoogleMutations::start(const QString &accountId, const QString &accessToken)
{
    if (m_busy || m_retryPending || accountId.isEmpty() || accessToken.isEmpty()) return false;
    m_accountId = accountId;
    m_accessToken = accessToken;
    m_changed = false;
    processNext();
    return true;
}

void GoogleMutations::cancel()
{
    m_retryTimer->stop();
    for (auto *reply : m_network->findChildren<QNetworkReply *>()) reply->abort();
    m_busy = false;
    m_retryPending = false;
    m_currentMutationId.clear();
    m_currentOperation.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    emit stateChanged();
}

void GoogleMutations::processNext()
{
    const QJsonObject mutation = m_database.nextPendingMutation(m_accountId).object();
    if (mutation.isEmpty()) { complete(); return; }
    m_currentMutationId = mutation.value(QStringLiteral("id")).toString();
    m_currentOperation = mutation.value(QStringLiteral("operation")).toString();
    m_currentGoogleEventId = mutation.value(QStringLiteral("providerEventId")).toString();
    m_currentBaseEtag = mutation.value(QStringLiteral("baseEtag")).toString();
    if (m_currentOperation == QStringLiteral("create"))
        m_currentGoogleEventId = mutation.value(QStringLiteral("payload")).toObject()
                                     .value(QStringLiteral("googleEventId")).toString();
    m_busy = true;
    m_lastError.clear();
    m_retryAt.clear();
    m_database.setMutationState(m_currentMutationId, QStringLiteral("uploading"), {}, true);
    emit stateChanged();
    const QByteArray calendarId = QUrl::toPercentEncoding(
        mutation.value(QStringLiteral("providerCalendarId")).toString());
    QByteArray endpoint = m_apiBaseUrl.toUtf8() + QByteArrayLiteral("/calendar/v3/calendars/")
        + calendarId + QByteArrayLiteral("/events");
    if (m_currentOperation == QStringLiteral("update"))
        endpoint += QByteArrayLiteral("/") + QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(endpoint));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (m_currentOperation == QStringLiteral("update") && !m_currentBaseEtag.isEmpty())
        request.setRawHeader("If-Match", m_currentBaseEtag.toUtf8());
    const QByteArray body = QJsonDocument(googleEventBody(
        mutation.value(QStringLiteral("payload")).toObject(),
        m_currentOperation == QStringLiteral("create"))).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_currentOperation == QStringLiteral("update")
        ? m_network->sendCustomRequest(request, QByteArrayLiteral("PATCH"), body)
        : m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleReply(reply); });
}

void GoogleMutations::handleReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    if (m_currentOperation == QStringLiteral("create") && status == 409 && !m_currentGoogleEventId.isEmpty()) {
        const QJsonObject mutation = m_database.nextPendingMutation(m_accountId).object();
        const QByteArray calendarId = QUrl::toPercentEncoding(
            mutation.value(QStringLiteral("providerCalendarId")).toString());
        const QByteArray eventId = QUrl::toPercentEncoding(m_currentGoogleEventId);
        QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
            + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
            + QByteArrayLiteral("/events/") + eventId));
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
        auto *existing = m_network->get(request);
        connect(existing, &QNetworkReply::finished, this, [this, existing] { handleReply(existing); });
        return;
    }
    if (status < 200 || status >= 300) {
        fail(responseError(body, status == 0 ? networkError
             : QStringLiteral("Google event %1 failed (%2)")
                   .arg(m_currentOperation == QStringLiteral("update") ? QStringLiteral("update")
                                                                        : QStringLiteral("creation"))
                   .arg(status)), status);
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const bool reconciled = document.isObject()
        && (m_currentOperation == QStringLiteral("update")
            ? m_database.completeUpdateMutation(m_currentMutationId, document.object())
            : m_database.completeCreateMutation(m_currentMutationId, document.object()));
    if (!reconciled) {
        fail(document.isObject() ? m_database.lastError() : QStringLiteral("Google returned an invalid event"), 400);
        return;
    }
    m_lastError.clear();
    m_changed = true;
    m_currentMutationId.clear();
    m_currentOperation.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    processNext();
}

void GoogleMutations::fail(const QString &message, int httpStatus)
{
    m_lastError = message;
    m_busy = false;
    const bool retryable = httpStatus == 0 || httpStatus == 408 || httpStatus == 429 || httpStatus >= 500;
    const QString state = retryable ? QStringLiteral("retrying")
                        : (httpStatus == 401 || httpStatus == 403) ? QStringLiteral("blocked")
                                                                 : QStringLiteral("failed");
    m_database.setMutationState(m_currentMutationId, state, message);
    if (retryable) {
        const int attempts = m_database.nextPendingMutation(m_accountId).object()
                                 .value(QStringLiteral("attemptCount")).toInt();
        const int seconds = std::min(5 * (1 << std::min(attempts, 5)), 300);
        m_retryPending = true;
        m_retryAt = QDateTime::currentDateTimeUtc().addSecs(seconds).toString(Qt::ISODateWithMs);
        m_retryTimer->start(seconds * 1000);
    }
    emit stateChanged();
    emit finished(false);
}

void GoogleMutations::complete()
{
    m_busy = false;
    m_retryPending = false;
    m_currentMutationId.clear();
    m_currentOperation.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    m_retryAt.clear();
    emit stateChanged();
    emit finished(m_changed);
}

QJsonDocument GoogleMutations::status() const
{
    const int pendingCount = m_database.status().object()
                                 .value(QStringLiteral("pendingMutationCount")).toInt();
    return QJsonDocument(QJsonObject {
        { QStringLiteral("state"), m_busy ? QStringLiteral("uploading")
            : m_retryPending ? QStringLiteral("retrying") : QStringLiteral("idle") },
        { QStringLiteral("lastError"), m_lastError },
        { QStringLiteral("retryAt"), m_retryAt },
        { QStringLiteral("mutationId"), m_currentMutationId },
        { QStringLiteral("pendingCount"), pendingCount }
    });
}
