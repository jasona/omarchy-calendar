#pragma once

#include <QDateTime>
#include <QObject>
#include <QVariantMap>

class QuickEntryParser final : public QObject
{
    Q_OBJECT
public:
    explicit QuickEntryParser(QObject *parent = nullptr);
    Q_INVOKABLE QVariantMap parse(const QString &text) const;
    static QVariantMap parseAt(const QString &text, const QDateTime &now);
};
