#pragma once

#include <QDate>
#include <QList>
#include <QString>

struct RecurrenceOccurrence
{
    QDate date;
    qint64 startMs = 0;
    qint64 endMs = 0;
};

class Recurrence final
{
public:
    static QList<RecurrenceOccurrence> expand(const QString &rule,
                                              qint64 startMs, qint64 endMs,
                                              const QString &timeZone,
                                              const QDate &rangeStart,
                                              const QDate &rangeEnd,
                                              int maximum = 1000);
};
