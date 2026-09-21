#include "timezonehelper.h"

#include <QDateTime>
#include <QLocale>
#include <QSet>
#include <QTimeZone>
#include <algorithm>

namespace {
QString offsetLabel(int seconds)
{
    const QChar sign = seconds < 0 ? QChar(0x2212) : QChar('+');
    const int totalMinutes = qAbs(seconds) / 60;
    return QStringLiteral("UTC%1%2:%3").arg(sign)
        .arg(totalMinutes / 60, 2, 10, QChar('0'))
        .arg(totalMinutes % 60, 2, 10, QChar('0'));
}

QTimeZone validZone(const QString &zoneId)
{
    const QTimeZone requested(zoneId.toUtf8());
    return requested.isValid() ? requested : QTimeZone::systemTimeZone();
}

QVariantMap describe(const QDateTime &value, const QTimeZone &eventZone)
{
    const QLocale locale;
    const QDateTime local = value.toTimeZone(QTimeZone::systemTimeZone());
    return {
        { QStringLiteral("epochMs"), value.toMSecsSinceEpoch() },
        { QStringLiteral("date"), value.date().toString(Qt::ISODate) },
        { QStringLiteral("time"), value.time().toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("dateLabel"), locale.toString(value.date(), QStringLiteral("ddd, MMM d")) },
        { QStringLiteral("timeLabel"), locale.toString(value.time(), QStringLiteral("h:mm AP")) },
        { QStringLiteral("abbreviation"), eventZone.abbreviation(value) },
        { QStringLiteral("offsetLabel"), offsetLabel(eventZone.offsetFromUtc(value)) },
        { QStringLiteral("localDate"), local.date().toString(Qt::ISODate) },
        { QStringLiteral("localTime"), local.time().toString(QStringLiteral("HH:mm")) },
        { QStringLiteral("localDateLabel"), locale.toString(local.date(), QStringLiteral("ddd, MMM d")) },
        { QStringLiteral("localTimeLabel"), locale.toString(local.time(), QStringLiteral("h:mm AP")) },
        { QStringLiteral("localAbbreviation"), QTimeZone::systemTimeZone().abbreviation(local) },
        { QStringLiteral("isLocal"), eventZone.id() == QTimeZone::systemTimeZoneId() }
    };
}
}

QString TimeZoneHelper::systemTimeZoneId() const
{
    return QString::fromUtf8(QTimeZone::systemTimeZoneId());
}

QVariantList TimeZoneHelper::options(qint64 atMs, const QStringList &preferred) const
{
    const QDateTime instant = QDateTime::fromMSecsSinceEpoch(
        atMs > 0 ? atMs : QDateTime::currentMSecsSinceEpoch(), QTimeZone::UTC);
    QList<QByteArray> ids = QTimeZone::availableTimeZoneIds();
    std::sort(ids.begin(), ids.end(), [&](const QByteArray &left, const QByteArray &right) {
        const int leftOffset = QTimeZone(left).offsetFromUtc(instant);
        const int rightOffset = QTimeZone(right).offsetFromUtc(instant);
        return leftOffset == rightOffset ? left < right : leftOffset < rightOffset;
    });

    QStringList ordered { systemTimeZoneId() };
    ordered.append(preferred);
    for (const auto &id : ids)
        ordered.append(QString::fromUtf8(id));

    QVariantList result;
    QSet<QString> seen;
    for (const QString &id : std::as_const(ordered)) {
        const QTimeZone zone(id.toUtf8());
        if (!zone.isValid() || seen.contains(id)) continue;
        seen.insert(id);
        const QString city = id.section('/', -1).replace('_', ' ');
        result.append(QVariantMap {
            { QStringLiteral("id"), id },
            { QStringLiteral("label"), QStringLiteral("(%1) %2 · %3")
                .arg(offsetLabel(zone.offsetFromUtc(instant)), city, id) },
            { QStringLiteral("offsetSeconds"), zone.offsetFromUtc(instant) }
        });
    }
    return result;
}

QVariantMap TimeZoneHelper::resolveWallTime(const QString &dateText, const QString &timeText,
                                            const QString &zoneId, bool preferLater) const
{
    const QDate date = QDate::fromString(dateText, Qt::ISODate);
    const QTime time = QTime::fromString(timeText, QStringLiteral("HH:mm"));
    const QTimeZone zone(zoneId.toUtf8());
    if (!date.isValid() || date.toString(Qt::ISODate) != dateText)
        return { { QStringLiteral("valid"), false },
                 { QStringLiteral("error"), QStringLiteral("Enter a valid date as YYYY-MM-DD.") } };
    if (!time.isValid() || time.toString(QStringLiteral("HH:mm")) != timeText)
        return { { QStringLiteral("valid"), false },
                 { QStringLiteral("error"), QStringLiteral("Enter a valid time as HH:MM.") } };
    if (!zone.isValid())
        return { { QStringLiteral("valid"), false },
                 { QStringLiteral("error"), QStringLiteral("Choose a valid IANA timezone.") } };

    const QDateTime before(date, time, zone, QDateTime::TransitionResolution::PreferBefore);
    const QDateTime after(date, time, zone, QDateTime::TransitionResolution::PreferAfter);
    const bool ambiguous = before.isValid() && after.isValid()
        && before.toMSecsSinceEpoch() != after.toMSecsSinceEpoch()
        && before.date() == date && before.time() == time
        && after.date() == date && after.time() == time;
    const QDateTime rejected(date, time, zone, QDateTime::TransitionResolution::Reject);
    if (!rejected.isValid() && !ambiguous) {
        return {
            { QStringLiteral("valid"), false },
            { QStringLiteral("gap"), true },
            { QStringLiteral("error"), QStringLiteral("This local time does not exist because the clock moves forward. Choose another time.") }
        };
    }

    QDateTime selected = rejected;
    if (ambiguous) {
        const qint64 early = qMin(before.toMSecsSinceEpoch(), after.toMSecsSinceEpoch());
        const qint64 late = qMax(before.toMSecsSinceEpoch(), after.toMSecsSinceEpoch());
        selected = QDateTime::fromMSecsSinceEpoch(preferLater ? late : early, zone);
    }

    QVariantMap result = describe(selected, zone);
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("ambiguous"), ambiguous);
    result.insert(QStringLiteral("occurrence"), ambiguous && preferLater ? 2 : 1);
    return result;
}

QVariantMap TimeZoneHelper::wallTime(qint64 epochMs, const QString &zoneId) const
{
    const QTimeZone zone = validZone(zoneId);
    const QDateTime value = QDateTime::fromMSecsSinceEpoch(epochMs, zone);
    const QDateTime before(value.date(), value.time(), zone, QDateTime::TransitionResolution::PreferBefore);
    const QDateTime after(value.date(), value.time(), zone, QDateTime::TransitionResolution::PreferAfter);
    const bool ambiguous = before.isValid() && after.isValid()
        && before.toMSecsSinceEpoch() != after.toMSecsSinceEpoch()
        && before.date() == value.date() && before.time() == value.time()
        && after.date() == value.date() && after.time() == value.time();
    QVariantMap result = describe(value, zone);
    result.insert(QStringLiteral("valid"), epochMs > 0);
    result.insert(QStringLiteral("zoneId"), QString::fromUtf8(zone.id()));
    result.insert(QStringLiteral("ambiguous"), ambiguous);
    result.insert(QStringLiteral("occurrence"), ambiguous
        && epochMs == qMax(before.toMSecsSinceEpoch(), after.toMSecsSinceEpoch()) ? 2 : 1);
    return result;
}
