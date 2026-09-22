#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

class Database;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class GoogleMutations final : public QObject
{
    Q_OBJECT
public:
    explicit GoogleMutations(Database &database, QObject *parent = nullptr);
    bool start(const QString &accountId, const QString &accessToken);
    void cancel();
    QJsonDocument status() const;

signals:
    void stateChanged();
    void finished(bool changed);

private:
    void processNext();
    void handleReply(QNetworkReply *reply);
    void handleConflictReply(QNetworkReply *reply);
    void fetchSeriesMaster();
    void handleSeriesMasterReply(QNetworkReply *reply);
    void handleFutureCreateReply(QNetworkReply *reply);
    void truncateOriginalSeries();
    void handleFutureTruncateReply(QNetworkReply *reply);
    void fetchCurrentEvent();
    void fail(const QString &message, int httpStatus);
    void stopForStorageError();
    void complete();

    Database &m_database;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_retryTimer = nullptr;
    QString m_apiBaseUrl;
    QString m_accountId;
    QString m_accessToken;
    QString m_currentMutationId;
    QString m_currentOperation;
    QString m_currentProviderCalendarId;
    QString m_currentGoogleEventId;
    QString m_currentBaseEtag;
    QJsonObject m_currentPayload;
    QJsonObject m_currentSeriesParent;
    QJsonArray m_truncatedRecurrence;
    QString m_lastError;
    QString m_retryAt;
    bool m_busy = false;
    bool m_retryPending = false;
    bool m_changed = false;
    bool m_conflict = false;
};
