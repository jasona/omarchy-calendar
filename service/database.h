#pragma once

#include <QJsonDocument>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QString>

class Database final
{
public:
    explicit Database(QString path);
    ~Database();

    bool open();
    bool importCompatibilityFeed(const QString &feedPath);
    bool exportCompatibilityFeed(const QString &feedPath);

    QJsonDocument eventsForRange(const QString &firstDate, const QString &lastDate) const;
    QJsonDocument searchEvents(const QString &queryText, int limit) const;
    QJsonDocument calendars() const;
    QJsonDocument accounts() const;
    QJsonDocument nextEvent() const;
    QJsonDocument status() const;
    QJsonDocument pendingMutations() const;
    QJsonDocument takeNewInvitations();
    QJsonDocument dueReminders(qint64 nowMs) const;
    bool markReminderDelivered(const QString &reminderId, uint notificationId);
    bool snoozeReminder(const QString &reminderId, qint64 untilMs);
    bool dismissReminder(const QString &reminderId);
    QString reminderIdForNotification(uint notificationId) const;
    bool pruneReminderDeliveries(qint64 beforeStartMs);

    bool upsertAccount(const QString &id, const QString &provider,
                       const QString &providerAccountId, const QString &displayName,
                       const QString &email, const QString &syncState);
    bool updateAccountSyncState(const QString &id, const QString &syncState,
                                const QString &lastError = {});
    bool setAccountGrantedScopes(const QString &id, const QString &scopes);
    QString accountGrantedScopes(const QString &id) const;
    bool requeueBlockedMutations(const QString &accountId);
    bool setCalendarSelected(const QString &calendarId, bool selected);
    QString createPendingEvent(const QJsonObject &event);
    bool updatePendingEvent(const QJsonObject &event);
    bool respondPendingEvent(const QString &calendarId, const QString &eventId,
                             const QString &responseStatus);
    QString deletePendingEvent(const QString &calendarId, const QString &eventId);
    QString deletePendingEvent(const QJsonObject &event);
    bool undoPendingDelete(const QString &mutationId);
    bool finalizePendingDelete(const QString &mutationId);
    bool finalizeUndoableDeletes();
    QJsonDocument nextPendingMutation(const QString &accountId) const;
    bool setMutationState(const QString &mutationId, const QString &state,
                          const QString &error = {}, bool incrementAttempt = false);
    bool retryMutation(const QString &mutationId);
    bool discardMutation(const QString &mutationId);
    bool completeCreateMutation(const QString &mutationId, const QJsonObject &remoteEvent);
    bool rebaseUpdateMutation(const QString &mutationId, const QJsonObject &remoteEvent);
    bool rebaseRsvpMutation(const QString &mutationId, const QJsonObject &remoteEvent);
    bool completeUpdateMutation(const QString &mutationId, const QJsonObject &remoteEvent);
    bool completeMoveMutation(const QString &mutationId, const QJsonObject &remoteEvent);
    bool completeDeleteMutation(const QString &mutationId);
    bool removeAccount(const QString &id);
    bool setSyncCursor(const QString &accountId, const QString &calendarId,
                       const QString &cursor);
    bool replaceGoogleCalendars(const QString &accountId, const QJsonArray &items);
    QJsonDocument calendarsForAccount(const QString &accountId) const;
    QString syncCursor(const QString &accountId, const QString &calendarId) const;
    bool applyGoogleEvents(const QString &accountId, const QString &calendarId,
                           const QJsonArray &items, const QString &nextSyncToken,
                           bool fullSync);

    QString lastError() const { return m_lastError; }
    QString path() const { return m_path; }

private:
    bool migrate();
    bool execute(const QString &statement);
    void setError(const QString &context, const QString &detail);

    QString m_path;
    QString m_connectionName;
    mutable QString m_lastError;
    QSqlDatabase m_database;
};
