#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>
#include <QQueue>
#include <QVariantMap>

class Database;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QUrl;

class GoogleSync final : public QObject
{
    Q_OBJECT

public:
    explicit GoogleSync(Database &database, QObject *parent = nullptr);

    bool start(const QString &accountId, const QString &accessToken);
    void cancel();
    bool busy() const { return m_busy; }
    QJsonDocument status() const;

signals:
    void finished(bool changed);
    void stateChanged();

private:
    void requestCalendarPage(const QString &pageToken = {});
    void handleCalendarPage(QNetworkReply *reply);
    void syncNextCalendar();
    void requestEventPage(const QString &pageToken = {});
    void handleEventPage(QNetworkReply *reply);
    QNetworkReply *get(const QUrl &url);
    void beginAttempt();
    void fail(const QString &message, bool retryable = true);
    void scheduleRetry();
    void complete();

    Database &m_database;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_retryTimer = nullptr;
    QString m_apiBaseUrl;
    QString m_accountId;
    QString m_accessToken;
    QString m_currentStage;
    QString m_lastError;
    QString m_lastAttemptAt;
    QString m_lastSuccessAt;
    QString m_retryAt;
    bool m_busy = false;
    bool m_retryPending = false;
    bool m_online = true;
    bool m_changed = false;
    bool m_fullSync = true;
    int m_failureCount = 0;
    int m_retryBaseSeconds = 30;
    int m_calendarsCompleted = 0;
    int m_calendarsTotal = 0;
    QJsonArray m_calendarItems;
    QJsonArray m_eventItems;
    QQueue<QVariantMap> m_calendarQueue;
    QVariantMap m_currentCalendar;
};
