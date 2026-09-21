#include "preferences.h"

#include <QSettings>

Preferences::Preferences(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_hiddenCalendarIds = settings.value(QStringLiteral("calendars/hidden")).toStringList();
    m_recentTimeZones = settings.value(QStringLiteral("calendar/recentTimeZones")).toStringList();
}

void Preferences::rememberTimeZone(const QString &zoneId)
{
    if (zoneId.isEmpty()) return;
    m_recentTimeZones.removeAll(zoneId);
    m_recentTimeZones.prepend(zoneId);
    while (m_recentTimeZones.size() > 5)
        m_recentTimeZones.removeLast();
    QSettings settings;
    settings.setValue(QStringLiteral("calendar/recentTimeZones"), m_recentTimeZones);
    settings.sync();
    emit recentTimeZonesChanged();
}

void Preferences::setHiddenCalendarIds(const QStringList &ids)
{
    if (m_hiddenCalendarIds == ids)
        return;

    m_hiddenCalendarIds = ids;
    QSettings settings;
    settings.setValue(QStringLiteral("calendars/hidden"), m_hiddenCalendarIds);
    settings.sync();
    emit hiddenCalendarIdsChanged();
}
