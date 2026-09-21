#include "preferences.h"

#include <QSettings>

Preferences::Preferences(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_hiddenCalendarIds = settings.value(QStringLiteral("calendars/hidden")).toStringList();
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
