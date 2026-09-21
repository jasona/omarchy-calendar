#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class Database;
class GoogleAuth;
class GoogleSync;
class GoogleMutations;

class CalendarService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.omarchy.Calendar1")

public:
    CalendarService(Database &database, GoogleAuth &googleAuth, GoogleSync &googleSync,
                    GoogleMutations &googleMutations,
                    QString feedPath, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE QString GetEvents(const QString &firstDate, const QString &lastDate) const;
    Q_SCRIPTABLE QString SearchEvents(const QString &queryText, int limit) const;
    Q_SCRIPTABLE QString GetCalendars() const;
    Q_SCRIPTABLE QString GetAccounts() const;
    Q_SCRIPTABLE QString GetNextEvent() const;
    Q_SCRIPTABLE QString GetStatus() const;
    Q_SCRIPTABLE QString GetProviderStatus() const;
    Q_SCRIPTABLE bool BeginGoogleAuthorization();
    Q_SCRIPTABLE bool DisconnectGoogle();
    Q_SCRIPTABLE bool SetCalendarSelected(const QString &calendarId, bool selected);
    Q_SCRIPTABLE QString CreateEvent(const QString &eventJson);
    Q_SCRIPTABLE bool UpdateEvent(const QString &eventJson);
    Q_SCRIPTABLE QString DeleteEvent(const QString &calendarId, const QString &eventId);
    Q_SCRIPTABLE QString DeleteEventScoped(const QString &eventJson);
    Q_SCRIPTABLE bool UndoDelete(const QString &mutationId);
    Q_SCRIPTABLE bool SyncNow();
    Q_SCRIPTABLE bool Reload();

signals:
    Q_SCRIPTABLE void EventsChanged();
    Q_SCRIPTABLE void AccountsChanged();
    Q_SCRIPTABLE void ProviderStatusChanged();
    Q_SCRIPTABLE void AuthorizationRequired(const QString &url);

private slots:
    void feedChanged();

private:
    void ensureWatching();

    Database &m_database;
    GoogleAuth &m_googleAuth;
    GoogleSync &m_googleSync;
    GoogleMutations &m_googleMutations;
    QString m_feedPath;
    QFileSystemWatcher m_watcher;
    QTimer m_mutationUploadDelay;
};
