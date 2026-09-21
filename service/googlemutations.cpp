#include "googlemutations.h"

#include "database.h"
#include "recurrence.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
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
        const QString startDate = payload.value(QStringLiteral("allDayStartDate")).toString(
            QDateTime::fromMSecsSinceEpoch(startMs, zone).date().toString(Qt::ISODate));
        const QString endDate = payload.value(QStringLiteral("allDayEndDate")).toString(
            QDateTime::fromMSecsSinceEpoch(endMs, zone).date().toString(Qt::ISODate));
        start.insert(QStringLiteral("date"), startDate);
        end.insert(QStringLiteral("date"), endDate);
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
    const QJsonArray recurrence = payload.value(QStringLiteral("recurrence")).toArray();
    if (!recurrence.isEmpty())
        body.insert(QStringLiteral("recurrence"), recurrence);
    return body;
}

QJsonObject googleSeriesEventBody(const QJsonObject &payload, const QJsonObject &parent)
{
    QJsonObject adjusted = payload;
    const QJsonObject parentStart = parent.value(QStringLiteral("start")).toObject();
    if (payload.value(QStringLiteral("allDay")).toBool()) {
        const QDate baseDate = QDate::fromString(
            payload.value(QStringLiteral("scopeBaseAllDayStartDate")).toString(), Qt::ISODate);
        const QDate desiredDate = QDate::fromString(
            payload.value(QStringLiteral("allDayStartDate")).toString(), Qt::ISODate);
        const QDate desiredEnd = QDate::fromString(
            payload.value(QStringLiteral("allDayEndDate")).toString(), Qt::ISODate);
        const QDate parentDate = QDate::fromString(
            parentStart.value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (baseDate.isValid() && desiredDate.isValid() && desiredEnd.isValid() && parentDate.isValid()) {
            const QDate shiftedStart = parentDate.addDays(baseDate.daysTo(desiredDate));
            adjusted.insert(QStringLiteral("allDayStartDate"), shiftedStart.toString(Qt::ISODate));
            adjusted.insert(QStringLiteral("allDayEndDate"),
                            shiftedStart.addDays(desiredDate.daysTo(desiredEnd)).toString(Qt::ISODate));
        }
    } else {
        const qint64 baseStartMs = qint64(payload.value(QStringLiteral("scopeBaseStartMs")).toDouble());
        const qint64 desiredStartMs = qint64(payload.value(QStringLiteral("startMs")).toDouble());
        const qint64 desiredEndMs = qint64(payload.value(QStringLiteral("endMs")).toDouble());
        const QDateTime parentDateTime = QDateTime::fromString(
            parentStart.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
        if (baseStartMs > 0 && desiredEndMs > desiredStartMs && parentDateTime.isValid()) {
            const qint64 shiftedStartMs = parentDateTime.toMSecsSinceEpoch() + desiredStartMs - baseStartMs;
            adjusted.insert(QStringLiteral("startMs"), double(shiftedStartMs));
            adjusted.insert(QStringLiteral("endMs"), double(shiftedStartMs + desiredEndMs - desiredStartMs));
            adjusted.insert(QStringLiteral("timeZone"),
                            parentStart.value(QStringLiteral("timeZone")).toString(
                                payload.value(QStringLiteral("timeZone")).toString()));
        }
    }
    return googleEventBody(adjusted, false);
}

QString rulePart(const QString &rule, const QString &name)
{
    const QRegularExpression expression(QStringLiteral("(?:^|;)") + name
                                        + QStringLiteral("=([^;]+)"),
                                        QRegularExpression::CaseInsensitiveOption);
    return expression.match(rule).captured(1);
}

QString withRulePart(QString rule, const QString &name, const QString &value)
{
    const QRegularExpression expression(QStringLiteral(";?") + name
                                        + QStringLiteral("=[^;]+"),
                                        QRegularExpression::CaseInsensitiveOption);
    rule.remove(expression);
    return rule + QStringLiteral(";") + name + QStringLiteral("=") + value;
}

bool splitRecurrence(const QJsonObject &parent, const QJsonObject &payload,
                     QJsonArray *past, QJsonArray *future)
{
    const QJsonArray recurrence = parent.value(QStringLiteral("recurrence")).toArray();
    const QJsonObject start = parent.value(QStringLiteral("start")).toObject();
    const QJsonObject end = parent.value(QStringLiteral("end")).toObject();
    const bool allDay = start.contains(QStringLiteral("date"));
    const QString zoneName = start.value(QStringLiteral("timeZone")).toString(
        payload.value(QStringLiteral("timeZone")).toString());
    QTimeZone zone(zoneName.toUtf8());
    if (!zone.isValid()) zone = QTimeZone::systemTimeZone();
    const QDateTime parentStart = allDay
        ? QDateTime(QDate::fromString(start.value(QStringLiteral("date")).toString(), Qt::ISODate), QTime(0, 0), zone)
        : QDateTime::fromString(start.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
    const QDateTime parentEnd = allDay
        ? QDateTime(QDate::fromString(end.value(QStringLiteral("date")).toString(), Qt::ISODate), QTime(0, 0), zone)
        : QDateTime::fromString(end.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
    const QDate splitDate = allDay
        ? QDate::fromString(payload.value(QStringLiteral("scopeOriginalStartDate")).toString(), Qt::ISODate)
        : QDateTime::fromMSecsSinceEpoch(
              qint64(payload.value(QStringLiteral("scopeOriginalStartMs")).toDouble()), zone).date();
    if (!parentStart.isValid() || !parentEnd.isValid() || !splitDate.isValid()
        || splitDate <= parentStart.date()) return false;

    for (const QJsonValue &value : recurrence) {
        const QString line = value.toString();
        if (!line.startsWith(QStringLiteral("RRULE:"), Qt::CaseInsensitive)) return false;
        const QString countText = rulePart(line, QStringLiteral("COUNT"));
        QString pastRule = line;
        QString futureRule = line;
        if (!countText.isEmpty()) {
            const int total = countText.toInt();
            const auto occurrences = Recurrence::expand(
                line, parentStart.toMSecsSinceEpoch(), parentEnd.toMSecsSinceEpoch(),
                QString::fromUtf8(zone.id()), parentStart.date(), splitDate.addDays(-1), total);
            const int before = occurrences.size();
            if (before < 1 || before >= total) return false;
            pastRule = withRulePart(pastRule, QStringLiteral("COUNT"), QString::number(before));
            futureRule = withRulePart(futureRule, QStringLiteral("COUNT"), QString::number(total - before));
        } else {
            const QString until = allDay
                ? splitDate.addDays(-1).toString(QStringLiteral("yyyyMMdd"))
                : QDateTime::fromMSecsSinceEpoch(
                      qint64(payload.value(QStringLiteral("scopeOriginalStartMs")).toDouble()) - 1000,
                      QTimeZone::UTC).toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
            pastRule = withRulePart(pastRule, QStringLiteral("UNTIL"), until);
        }
        past->append(pastRule);
        future->append(futureRule);
    }
    return !past->isEmpty() && !future->isEmpty();
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
    m_conflict = false;
    m_currentMutationId.clear();
    m_currentOperation.clear();
    m_currentProviderCalendarId.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    m_currentPayload = {};
    m_currentSeriesParent = {};
    m_truncatedRecurrence = {};
    emit stateChanged();
}

void GoogleMutations::processNext()
{
    const QJsonObject mutation = m_database.nextPendingMutation(m_accountId).object();
    if (mutation.isEmpty()) { complete(); return; }
    m_currentMutationId = mutation.value(QStringLiteral("id")).toString();
    m_currentOperation = mutation.value(QStringLiteral("operation")).toString();
    m_currentProviderCalendarId = mutation.value(QStringLiteral("providerCalendarId")).toString();
    m_currentGoogleEventId = mutation.value(QStringLiteral("providerEventId")).toString();
    m_currentBaseEtag = mutation.value(QStringLiteral("baseEtag")).toString();
    m_currentPayload = mutation.value(QStringLiteral("payload")).toObject();
    if (m_currentOperation == QStringLiteral("create"))
        m_currentGoogleEventId = m_currentPayload.value(QStringLiteral("googleEventId")).toString();
    else if (m_currentOperation == QStringLiteral("update-series")
             || m_currentOperation == QStringLiteral("update-future")
             || m_currentOperation == QStringLiteral("delete-series")
             || m_currentOperation == QStringLiteral("delete-future"))
        m_currentGoogleEventId = m_currentPayload.value(QStringLiteral("seriesId")).toString();
    m_busy = true;
    m_conflict = false;
    m_lastError.clear();
    m_retryAt.clear();
    m_database.setMutationState(m_currentMutationId, QStringLiteral("uploading"), {}, true);
    emit stateChanged();
    if (m_currentOperation == QStringLiteral("update-series")
        || m_currentOperation == QStringLiteral("update-future")
        || m_currentOperation == QStringLiteral("delete-future")) {
        fetchSeriesMaster();
        return;
    }
    const QByteArray calendarId = QUrl::toPercentEncoding(
        mutation.value(QStringLiteral("providerCalendarId")).toString());
    QByteArray endpoint = m_apiBaseUrl.toUtf8() + QByteArrayLiteral("/calendar/v3/calendars/")
        + calendarId + QByteArrayLiteral("/events");
    if (m_currentOperation == QStringLiteral("update")
        || m_currentOperation == QStringLiteral("delete")
        || m_currentOperation == QStringLiteral("delete-series"))
        endpoint += QByteArrayLiteral("/") + QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(endpoint));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if ((m_currentOperation == QStringLiteral("update")
         || m_currentOperation == QStringLiteral("delete")) && !m_currentBaseEtag.isEmpty())
        request.setRawHeader("If-Match", m_currentBaseEtag.toUtf8());
    QNetworkReply *reply = nullptr;
    if (m_currentOperation == QStringLiteral("delete")
        || m_currentOperation == QStringLiteral("delete-series")) {
        reply = m_network->deleteResource(request);
    } else {
        const QByteArray body = QJsonDocument(googleEventBody(
            mutation.value(QStringLiteral("payload")).toObject(),
            m_currentOperation == QStringLiteral("create"))).toJson(QJsonDocument::Compact);
        reply = m_currentOperation == QStringLiteral("update")
            ? m_network->sendCustomRequest(request, QByteArrayLiteral("PATCH"), body)
            : m_network->post(request, body);
    }
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
    if (m_currentOperation == QStringLiteral("update") && status == 412) {
        fetchCurrentEvent();
        return;
    }
    if (m_currentOperation == QStringLiteral("update-series") && status == 412) {
        fail(QStringLiteral("The recurring series changed in Google while it was being saved"), 412);
        return;
    }
    const bool deletion = m_currentOperation == QStringLiteral("delete")
        || m_currentOperation == QStringLiteral("delete-series");
    if (deletion && status == 404) {
        if (!m_database.completeDeleteMutation(m_currentMutationId)) {
            fail(m_database.lastError(), 400);
            return;
        }
        m_lastError.clear();
        m_changed = true;
        m_currentMutationId.clear();
        m_currentOperation.clear();
        m_currentProviderCalendarId.clear();
        m_currentGoogleEventId.clear();
        m_currentBaseEtag.clear();
        m_currentPayload = {};
        processNext();
        return;
    }
    if (status < 200 || status >= 300) {
        const QString message = responseError(body, status == 0 ? networkError
             : QStringLiteral("Google event %1 failed (%2)")
                   .arg(m_currentOperation == QStringLiteral("update")
                            || m_currentOperation == QStringLiteral("update-series") ? QStringLiteral("update")
                        : deletion ? QStringLiteral("deletion")
                                                                         : QStringLiteral("creation"))
                   .arg(status));
        const bool permanentDeleteFailure = deletion
            && status >= 400 && status < 500 && status != 401 && status != 403
            && status != 408 && status != 429;
        if (permanentDeleteFailure) {
            m_database.setMutationState(m_currentMutationId, QStringLiteral("undoable"), message);
            if (m_database.undoPendingDelete(m_currentMutationId)) {
                m_lastError = message;
                m_conflict = status == 412;
                m_changed = true;
                complete();
                return;
            }
        }
        fail(message, status);
        return;
    }
    if (deletion) {
        if (!m_database.completeDeleteMutation(m_currentMutationId)) {
            fail(m_database.lastError(), 400);
            return;
        }
        m_lastError.clear();
        m_changed = true;
        m_currentMutationId.clear();
        m_currentOperation.clear();
        m_currentProviderCalendarId.clear();
        m_currentGoogleEventId.clear();
        m_currentBaseEtag.clear();
        m_currentPayload = {};
        processNext();
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const bool reconciled = document.isObject()
        && (m_currentOperation == QStringLiteral("update")
            || m_currentOperation == QStringLiteral("update-series")
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
    m_currentProviderCalendarId.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    m_currentPayload = {};
    processNext();
}

void GoogleMutations::fetchSeriesMaster()
{
    if (m_currentGoogleEventId.isEmpty()) {
        fail(QStringLiteral("Recurring series identity is missing"), 400);
        return;
    }
    const QByteArray calendarId = QUrl::toPercentEncoding(m_currentProviderCalendarId);
    const QByteArray eventId = QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
        + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
        + QByteArrayLiteral("/events/") + eventId));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleSeriesMasterReply(reply); });
}

void GoogleMutations::handleSeriesMasterReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    const QJsonDocument document = QJsonDocument::fromJson(responseBody);
    if (status < 200 || status >= 300 || !document.isObject()) {
        fail(responseError(responseBody, status == 0 ? networkError
             : QStringLiteral("Google recurring series lookup failed (%1)").arg(status)), status);
        return;
    }
    const QJsonObject parent = document.object();
    if (m_currentOperation == QStringLiteral("update-future")
        || m_currentOperation == QStringLiteral("delete-future")) {
        QJsonArray futureRecurrence;
        m_truncatedRecurrence = {};
        if (!splitRecurrence(parent, m_currentPayload, &m_truncatedRecurrence, &futureRecurrence)) {
            fail(QStringLiteral("This recurrence pattern cannot be split at the selected occurrence"), 400);
            return;
        }
        m_currentSeriesParent = parent;
        if (m_currentOperation == QStringLiteral("delete-future")) {
            truncateOriginalSeries();
            return;
        }
        QJsonObject futurePayload = m_currentPayload;
        futurePayload.insert(QStringLiteral("recurrence"), futureRecurrence);
        futurePayload.insert(QStringLiteral("googleEventId"), QString::fromLatin1(
            QCryptographicHash::hash(m_currentMutationId.toUtf8(), QCryptographicHash::Sha256)
                .toHex().left(32)));
        const QByteArray calendarId = QUrl::toPercentEncoding(m_currentProviderCalendarId);
        QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
            + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
            + QByteArrayLiteral("/events")));
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QByteArray body = QJsonDocument(googleEventBody(futurePayload, true))
                                    .toJson(QJsonDocument::Compact);
        auto *create = m_network->post(request, body);
        connect(create, &QNetworkReply::finished, this, [this, create] { handleFutureCreateReply(create); });
        return;
    }
    const QByteArray calendarId = QUrl::toPercentEncoding(m_currentProviderCalendarId);
    const QByteArray eventId = QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
        + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
        + QByteArrayLiteral("/events/") + eventId));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QString etag = parent.value(QStringLiteral("etag")).toString();
    if (!etag.isEmpty()) request.setRawHeader("If-Match", etag.toUtf8());
    const QByteArray body = QJsonDocument(googleSeriesEventBody(m_currentPayload, parent))
                                .toJson(QJsonDocument::Compact);
    auto *update = m_network->sendCustomRequest(request, QByteArrayLiteral("PATCH"), body);
    connect(update, &QNetworkReply::finished, this, [this, update] { handleReply(update); });
}

void GoogleMutations::handleFutureCreateReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    if ((status < 200 || status >= 300) && status != 409) {
        fail(responseError(body, status == 0 ? networkError
             : QStringLiteral("Google future-series creation failed (%1)").arg(status)), status);
        return;
    }
    truncateOriginalSeries();
}

void GoogleMutations::truncateOriginalSeries()
{
    const QByteArray calendarId = QUrl::toPercentEncoding(m_currentProviderCalendarId);
    const QByteArray eventId = QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
        + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
        + QByteArrayLiteral("/events/") + eventId));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QString etag = m_currentSeriesParent.value(QStringLiteral("etag")).toString();
    if (!etag.isEmpty()) request.setRawHeader("If-Match", etag.toUtf8());
    const QByteArray body = QJsonDocument(QJsonObject {
        { QStringLiteral("recurrence"), m_truncatedRecurrence }
    }).toJson(QJsonDocument::Compact);
    auto *update = m_network->sendCustomRequest(request, QByteArrayLiteral("PATCH"), body);
    connect(update, &QNetworkReply::finished, this, [this, update] { handleFutureTruncateReply(update); });
}

void GoogleMutations::handleFutureTruncateReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (status < 200 || status >= 300 || !document.isObject()) {
        fail(responseError(body, status == 0 ? networkError
             : QStringLiteral("Google original-series truncation failed (%1)").arg(status)), status);
        return;
    }
    const bool completed = m_currentOperation == QStringLiteral("delete-future")
        ? m_database.completeDeleteMutation(m_currentMutationId)
        : m_database.completeUpdateMutation(m_currentMutationId, document.object());
    if (!completed) {
        fail(m_database.lastError(), 400);
        return;
    }
    m_lastError.clear();
    m_changed = true;
    m_currentMutationId.clear();
    m_currentOperation.clear();
    m_currentProviderCalendarId.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    m_currentPayload = {};
    m_currentSeriesParent = {};
    m_truncatedRecurrence = {};
    processNext();
}

void GoogleMutations::fetchCurrentEvent()
{
    const QByteArray calendarId = QUrl::toPercentEncoding(m_currentProviderCalendarId);
    const QByteArray eventId = QUrl::toPercentEncoding(m_currentGoogleEventId);
    QNetworkRequest request(QUrl::fromEncoded(m_apiBaseUrl.toUtf8()
        + QByteArrayLiteral("/calendar/v3/calendars/") + calendarId
        + QByteArrayLiteral("/events/") + eventId));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_accessToken.toUtf8());
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { handleConflictReply(reply); });
}

void GoogleMutations::handleConflictReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    if (status < 200 || status >= 300) {
        fail(responseError(body, status == 0 ? networkError
             : QStringLiteral("Google conflict lookup failed (%1)").arg(status)), status);
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject() || !m_database.rebaseUpdateMutation(m_currentMutationId, document.object())) {
        fail(document.isObject() ? m_database.lastError()
                                 : QStringLiteral("Google returned an invalid conflict response"), 412);
        return;
    }
    m_busy = false;
    emit stateChanged();
    processNext();
}

void GoogleMutations::fail(const QString &message, int httpStatus)
{
    m_lastError = message;
    m_busy = false;
    m_conflict = httpStatus == 412;
    const bool retryable = httpStatus == 0 || httpStatus == 408 || httpStatus == 429 || httpStatus >= 500;
    const QString state = retryable ? QStringLiteral("retrying")
                        : httpStatus == 412 ? QStringLiteral("conflict")
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
    m_currentProviderCalendarId.clear();
    m_currentGoogleEventId.clear();
    m_currentBaseEtag.clear();
    m_currentPayload = {};
    m_currentSeriesParent = {};
    m_truncatedRecurrence = {};
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
            : m_retryPending ? QStringLiteral("retrying")
            : m_conflict ? QStringLiteral("conflict") : QStringLiteral("idle") },
        { QStringLiteral("lastError"), m_lastError },
        { QStringLiteral("retryAt"), m_retryAt },
        { QStringLiteral("mutationId"), m_currentMutationId },
        { QStringLiteral("pendingCount"), pendingCount }
    });
}
