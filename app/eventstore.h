#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

class EventStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int eventCount READ eventCount NOTIFY eventsChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY eventsChanged)
    Q_PROPERTY(bool serviceBacked READ serviceBacked NOTIFY eventsChanged)

public:
    explicit EventStore(QObject *parent = nullptr);

    int eventCount() const { return m_eventCount; }
    QString sourcePath() const { return m_sourcePath; }
    QString lastError() const { return m_lastError; }
    bool serviceBacked() const;

    Q_INVOKABLE QVariantList eventsForRange(const QString &firstDate, const QString &lastDate) const;
    Q_INVOKABLE QVariantList searchEvents(const QString &queryText, int limit = 60) const;
    Q_INVOKABLE QVariantList calendars() const;
    Q_INVOKABLE QVariantList accounts() const;
    Q_INVOKABLE QVariantMap providerStatus() const;
    Q_INVOKABLE QVariantList pendingMutations() const;
    Q_INVOKABLE bool beginGoogleAuthorization() const;
    Q_INVOKABLE bool disconnectGoogle() const;
    Q_INVOKABLE bool setCalendarSelected(const QString &calendarId, bool selected) const;
    Q_INVOKABLE QString createEvent(const QVariantMap &event) const;
    Q_INVOKABLE bool updateEvent(const QVariantMap &event) const;
    Q_INVOKABLE bool respondToInvitation(const QString &calendarId, const QString &eventId,
                                         const QString &responseStatus) const;
    Q_INVOKABLE QString deleteEvent(const QString &calendarId, const QString &eventId) const;
    Q_INVOKABLE QString deleteEventScoped(const QVariantMap &event) const;
    Q_INVOKABLE bool undoDelete(const QString &mutationId) const;
    Q_INVOKABLE bool retryMutation(const QString &mutationId) const;
    Q_INVOKABLE bool discardMutation(const QString &mutationId) const;
    Q_INVOKABLE bool syncNow() const;
    Q_INVOKABLE QVariantMap nextEvent() const;
    Q_INVOKABLE bool copyDiagnostics() const;

signals:
    void eventsChanged();
    void accountsChanged();
    void providerStatusChanged();
    void authorizationRequired(const QString &url);

private slots:
    void reload();
    void forwardAccountsChanged();
    void forwardProviderStatusChanged();
    void forwardAuthorizationUrl(const QString &url);

private:
    static QVariantMap toVariant(const QJsonObject &event);
    static QVariantList withCollisionLayout(QVariantList events);
    QVariantList serviceArray(const QString &method, const QVariantList &arguments = {}) const;
    QVariantMap serviceObject(const QString &method) const;
    void ensureWatching();

    QString m_sourcePath;
    QString m_lastError;
    int m_eventCount = 0;
    QList<QJsonObject> m_events;
    QFileSystemWatcher m_watcher;
    class QDBusInterface *m_service = nullptr;
};
