#pragma once

#include "secretstore.h"

#include <QJsonDocument>
#include <QObject>

class Database;
class QNetworkAccessManager;
class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;
class QUrl;

class GoogleAuth final : public QObject
{
    Q_OBJECT

public:
    explicit GoogleAuth(Database &database, QObject *parent = nullptr);

    bool beginAuthorization();
    bool disconnectAccount();
    QJsonDocument status() const;
    QString accessToken() const;
    QString currentAccountId() const { return m_currentAccountId; }
    bool writeAccessAvailable() const { return m_writeAccess; }

signals:
    void authorizationRequired(const QString &url);
    void accountConnected();
    void accountDisconnected();
    void stateChanged();
    void accessTokenReady(const QString &accountId, const QString &accessToken);

private:
    void configure();
    void finishAuthorization();
    void restoreAccount();
    void finishRefresh();
    void setError(const QString &message);
    static QJsonObject decodeIdToken(const QString &token);

    Database &m_database;
    SecretStore m_secretStore;
    QNetworkAccessManager *m_network = nullptr;
    QOAuth2AuthorizationCodeFlow *m_oauth = nullptr;
    QOAuthHttpServerReplyHandler *m_replyHandler = nullptr;
    QString m_clientId;
    QString m_clientSecret;
    QString m_state = QStringLiteral("disconnected");
    QString m_lastError;
    QString m_redirectUrl;
    QString m_currentAccountId;
    bool m_writeAccess = false;
};
