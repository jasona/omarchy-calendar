#pragma once

#include <QObject>
#include <QStringList>

class Preferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList hiddenCalendarIds READ hiddenCalendarIds WRITE setHiddenCalendarIds NOTIFY hiddenCalendarIdsChanged)
    Q_PROPERTY(QStringList recentTimeZones READ recentTimeZones NOTIFY recentTimeZonesChanged)
    Q_PROPERTY(bool onboardingCompleted READ onboardingCompleted WRITE setOnboardingCompleted NOTIFY onboardingCompletedChanged)
    Q_PROPERTY(QString interfaceDensity READ interfaceDensity WRITE setInterfaceDensity NOTIFY interfaceDensityChanged)

public:
    explicit Preferences(QObject *parent = nullptr);

    QStringList hiddenCalendarIds() const { return m_hiddenCalendarIds; }
    QStringList recentTimeZones() const { return m_recentTimeZones; }
    bool onboardingCompleted() const { return m_onboardingCompleted; }
    QString interfaceDensity() const { return m_interfaceDensity; }
    void setHiddenCalendarIds(const QStringList &ids);
    Q_INVOKABLE void rememberTimeZone(const QString &zoneId);
    void setOnboardingCompleted(bool completed);
    void setInterfaceDensity(const QString &density);

signals:
    void hiddenCalendarIdsChanged();
    void recentTimeZonesChanged();
    void onboardingCompletedChanged();
    void interfaceDensityChanged();

private:
    QStringList m_hiddenCalendarIds;
    QStringList m_recentTimeZones;
    bool m_onboardingCompleted = false;
    QString m_interfaceDensity { QStringLiteral("comfortable") };
};
