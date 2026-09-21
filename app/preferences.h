#pragma once

#include <QObject>
#include <QStringList>

class Preferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList hiddenCalendarIds READ hiddenCalendarIds WRITE setHiddenCalendarIds NOTIFY hiddenCalendarIdsChanged)

public:
    explicit Preferences(QObject *parent = nullptr);

    QStringList hiddenCalendarIds() const { return m_hiddenCalendarIds; }
    void setHiddenCalendarIds(const QStringList &ids);

signals:
    void hiddenCalendarIdsChanged();

private:
    QStringList m_hiddenCalendarIds;
};
