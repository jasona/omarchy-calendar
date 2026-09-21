#pragma once

#include <QObject>
#include <QStringList>

class Preferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList hiddenCalendarIds READ hiddenCalendarIds WRITE setHiddenCalendarIds NOTIFY hiddenCalendarIdsChanged)
    Q_PROPERTY(QStringList recentTimeZones READ recentTimeZones NOTIFY recentTimeZonesChanged)

public:
    explicit Preferences(QObject *parent = nullptr);

    QStringList hiddenCalendarIds() const { return m_hiddenCalendarIds; }
    QStringList recentTimeZones() const { return m_recentTimeZones; }
    void setHiddenCalendarIds(const QStringList &ids);
    Q_INVOKABLE void rememberTimeZone(const QString &zoneId);

signals:
    void hiddenCalendarIdsChanged();
    void recentTimeZonesChanged();

private:
    QStringList m_hiddenCalendarIds;
    QStringList m_recentTimeZones;
};
