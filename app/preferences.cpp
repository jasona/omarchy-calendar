#include "preferences.h"

#include <QSettings>

Preferences::Preferences(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_hiddenCalendarIds = settings.value(QStringLiteral("calendars/hidden")).toStringList();
    m_recentTimeZones = settings.value(QStringLiteral("calendar/recentTimeZones")).toStringList();
    m_onboardingCompleted = settings.value(QStringLiteral("onboarding/completed"), false).toBool();
    m_interfaceDensity = settings.value(QStringLiteral("appearance/density"),
                                         QStringLiteral("comfortable")).toString();
    if (m_interfaceDensity != QStringLiteral("comfortable")
        && m_interfaceDensity != QStringLiteral("compact"))
        m_interfaceDensity = QStringLiteral("comfortable");
}

void Preferences::setInterfaceDensity(const QString &density)
{
    if ((density != QStringLiteral("comfortable") && density != QStringLiteral("compact"))
        || m_interfaceDensity == density) return;
    m_interfaceDensity = density;
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/density"), density);
    settings.sync();
    emit interfaceDensityChanged();
}

void Preferences::setOnboardingCompleted(bool completed)
{
    if (m_onboardingCompleted == completed) return;
    m_onboardingCompleted = completed;
    QSettings settings;
    settings.setValue(QStringLiteral("onboarding/completed"), completed);
    settings.sync();
    emit onboardingCompletedChanged();
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
