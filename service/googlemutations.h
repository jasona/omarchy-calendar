#pragma once

#include <QJsonDocument>
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
    void fail(const QString &message, int httpStatus);
    void complete();

    Database &m_database;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_retryTimer = nullptr;
    QString m_apiBaseUrl;
    QString m_accountId;
    QString m_accessToken;
    QString m_currentMutationId;
    QString m_currentOperation;
    QString m_currentGoogleEventId;
    QString m_currentBaseEtag;
    QString m_lastError;
    QString m_retryAt;
    bool m_busy = false;
    bool m_retryPending = false;
    bool m_changed = false;
};
