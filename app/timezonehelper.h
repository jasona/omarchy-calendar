#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class TimeZoneHelper final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString systemTimeZoneId READ systemTimeZoneId CONSTANT)

public:
    explicit TimeZoneHelper(QObject *parent = nullptr) : QObject(parent) {}

    QString systemTimeZoneId() const;

    Q_INVOKABLE QVariantList options(qint64 atMs, const QStringList &preferred = {}) const;
    Q_INVOKABLE QVariantMap resolveWallTime(const QString &date, const QString &time,
                                            const QString &zoneId, bool preferLater = false) const;
    Q_INVOKABLE QVariantMap wallTime(qint64 epochMs, const QString &zoneId) const;
};
