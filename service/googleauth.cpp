#include "googleauth.h"

#include "database.h"

#include <QAbstractOAuth>
#include <QAbstractOAuth2>
#include <QHostAddress>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QTimer>
#include <QUrl>

#include <chrono>

GoogleAuth::GoogleAuth(Database &database, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_network(new QNetworkAccessManager(this))
    , m_oauth(new QOAuth2AuthorizationCodeFlow(m_network, this))
    , m_replyHandler(new QOAuthHttpServerReplyHandler(this))
    , m_clientId(qEnvironmentVariable("OMARCHY_CALENDAR_GOOGLE_CLIENT_ID"))
    , m_clientSecret(qEnvironmentVariable("OMARCHY_CALENDAR_GOOGLE_CLIENT_SECRET"))
{
    configure();
    QTimer::singleShot(0, this, &GoogleAuth::restoreAccount);
}

void GoogleAuth::configure()
{
    m_replyHandler->setCallbackPath(QStringLiteral("/oauth2/callback"));
    m_replyHandler->setCallbackText(QStringLiteral(
        "Omarchy Calendar is connected. You can close this browser tab and return to the calendar."));
    m_oauth->setReplyHandler(m_replyHandler);
    m_oauth->setAuthorizationUrl(QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")));
    m_oauth->setTokenUrl(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    m_oauth->setClientIdentifier(m_clientId);
    m_oauth->setClientIdentifierSharedKey(m_clientSecret);
    m_oauth->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
    m_oauth->setAutoRefresh(true);
    m_oauth->setRefreshLeadTime(std::chrono::seconds(90));
    m_oauth->setRequestedScopeTokens({
        QByteArrayLiteral("openid"),
        QByteArrayLiteral("email"),
        QByteArrayLiteral("profile"),
        QByteArrayLiteral("https://www.googleapis.com/auth/calendar.calendarlist.readonly"),
        QByteArrayLiteral("https://www.googleapis.com/auth/calendar.events")
    });
    m_oauth->setModifyParametersFunction([](QAbstractOAuth::Stage stage,
                                             QMultiMap<QString, QVariant> *parameters) {
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
            parameters->insert(QStringLiteral("access_type"), QStringLiteral("offline"));
            parameters->insert(QStringLiteral("prompt"), QStringLiteral("consent"));
        }
    });

    connect(m_oauth, &QAbstractOAuth::authorizeWithBrowser, this, [this](const QUrl &url) {
        m_redirectUrl = url.toString();
        emit authorizationRequired(m_redirectUrl);
        emit stateChanged();
    });
    connect(m_oauth, &QAbstractOAuth::granted, this, [this] {
        if (m_state == QStringLiteral("authorizing"))
            finishAuthorization();
        else if (m_state == QStringLiteral("refreshing"))
            finishRefresh();
    });
    connect(m_oauth, &QAbstractOAuth::tokenChanged, this, [this](const QString &token) {
        if (m_state == QStringLiteral("connected") && !m_currentAccountId.isEmpty() && !token.isEmpty())
            emit accessTokenReady(m_currentAccountId, token);
    });
    connect(m_oauth, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error) {
        setError(QStringLiteral("Google authorization request failed"));
    });
    connect(m_oauth, &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString &error, const QString &description, const QUrl &) {
        setError(description.isEmpty() ? error : description);
    });
}

bool GoogleAuth::beginAuthorization()
{
    m_lastError.clear();
    m_redirectUrl.clear();
    if (m_clientId.isEmpty() || m_clientSecret.isEmpty()) {
        setError(QStringLiteral("Google OAuth client credentials are not configured"));
        return false;
    }
    if (!m_replyHandler->isListening()
        && !m_replyHandler->listen(QHostAddress::LocalHost, 0)) {
        setError(QStringLiteral("Could not open the local OAuth callback listener"));
        return false;
    }

    m_state = QStringLiteral("authorizing");
    emit stateChanged();
    m_oauth->grant();
    return true;
}

bool GoogleAuth::disconnectAccount()
{
    if (m_currentAccountId.isEmpty())
        return false;

    QString secretError;
    if (!m_secretStore.clearRefreshToken(m_currentAccountId, &secretError)) {
        setError(QStringLiteral("Refresh token could not be removed from Secret Service: ") + secretError);
        return false;
    }
    if (!m_database.removeAccount(m_currentAccountId)) {
        setError(m_database.lastError());
        return false;
    }

    m_oauth->setToken({});
    m_oauth->setRefreshToken({});
    m_currentAccountId.clear();
    m_writeAccess = false;
    m_state = QStringLiteral("disconnected");
    m_lastError.clear();
    m_redirectUrl.clear();
    emit accountDisconnected();
    emit stateChanged();
    return true;
}

QJsonObject GoogleAuth::decodeIdToken(const QString &token)
{
    const auto parts = token.toUtf8().split('.');
    if (parts.size() < 2)
        return {};
    const auto document = QJsonDocument::fromJson(QByteArray::fromBase64(parts.at(1), QByteArray::Base64UrlEncoding));
    return document.isObject() ? document.object() : QJsonObject {};
}

void GoogleAuth::finishAuthorization()
{
    const QJsonObject identity = decodeIdToken(m_oauth->idToken());
    const QString providerId = identity.value(QStringLiteral("sub")).toString();
    const QString email = identity.value(QStringLiteral("email")).toString();
    const QString displayName = identity.value(QStringLiteral("name")).toString(email);
    const QString refreshToken = m_oauth->refreshToken();
    if (providerId.isEmpty() || refreshToken.isEmpty()) {
        setError(QStringLiteral("Google did not return an account identity and refresh token"));
        return;
    }

    const QString accountId = QStringLiteral("google:") + providerId;
    QString secretError;
    if (!m_secretStore.storeRefreshToken(accountId, refreshToken, &secretError)) {
        setError(QStringLiteral("Refresh token could not be stored in Secret Service: ") + secretError);
        return;
    }
    if (!m_database.upsertAccount(accountId, QStringLiteral("google"), providerId,
                                  displayName, email, QStringLiteral("connected"))) {
        m_secretStore.clearRefreshToken(accountId);
        setError(m_database.lastError());
        return;
    }
    const QString grantedScopes = QStringLiteral(
        "openid email profile calendar.calendarlist.readonly calendar.events");
    if (!m_database.setAccountGrantedScopes(accountId, grantedScopes)) {
        setError(m_database.lastError());
        return;
    }
    if (!m_database.requeueBlockedMutations(accountId)) {
        setError(m_database.lastError());
        return;
    }

    m_state = QStringLiteral("connected");
    m_currentAccountId = accountId;
    m_writeAccess = true;
    m_lastError.clear();
    m_redirectUrl.clear();
    m_replyHandler->close();
    emit accountConnected();
    emit stateChanged();
    emit accessTokenReady(m_currentAccountId, m_oauth->token());
}

void GoogleAuth::restoreAccount()
{
    const auto accountValues = m_database.accounts().array();
    for (const auto &value : accountValues) {
        const auto account = value.toObject();
        if (account.value(QStringLiteral("provider")).toString() != QStringLiteral("google"))
            continue;
        m_currentAccountId = account.value(QStringLiteral("id")).toString();
        m_writeAccess = m_database.accountGrantedScopes(m_currentAccountId)
                            .contains(QStringLiteral("calendar.events"));
        break;
    }
    if (m_currentAccountId.isEmpty())
        return;
    if (m_clientId.isEmpty() || m_clientSecret.isEmpty()) {
        setError(QStringLiteral("Google OAuth client credentials are not configured"));
        return;
    }
    QString secretError;
    const QString token = m_secretStore.refreshToken(m_currentAccountId, &secretError);
    if (token.isEmpty()) {
        setError(secretError.isEmpty()
                 ? QStringLiteral("The Google refresh token is missing from Secret Service")
                 : QStringLiteral("Refresh token could not be loaded: ") + secretError);
        return;
    }
    m_state = QStringLiteral("refreshing");
    m_oauth->setRefreshToken(token);
    emit stateChanged();
    m_oauth->refreshTokens();
}

void GoogleAuth::finishRefresh()
{
    if (m_oauth->token().isEmpty()) {
        setError(QStringLiteral("Google did not return an access token"));
        return;
    }
    if (!m_oauth->refreshToken().isEmpty()) {
        QString secretError;
        if (!m_secretStore.storeRefreshToken(m_currentAccountId, m_oauth->refreshToken(), &secretError)) {
            setError(QStringLiteral("Rotated refresh token could not be stored: ") + secretError);
            return;
        }
    }
    m_state = QStringLiteral("connected");
    m_lastError.clear();
    m_database.updateAccountSyncState(m_currentAccountId, QStringLiteral("connected"));
    emit stateChanged();
    emit accessTokenReady(m_currentAccountId, m_oauth->token());
}

QString GoogleAuth::accessToken() const
{
    return m_oauth->token();
}

void GoogleAuth::setError(const QString &message)
{
    m_state = QStringLiteral("error");
    m_lastError = message;
    m_replyHandler->close();
    emit stateChanged();
}

QJsonDocument GoogleAuth::status() const
{
    return QJsonDocument(QJsonObject {
        { QStringLiteral("provider"), QStringLiteral("google") },
        { QStringLiteral("available"), true },
        { QStringLiteral("configured"), !m_clientId.isEmpty() && !m_clientSecret.isEmpty() },
        { QStringLiteral("state"), m_state },
        { QStringLiteral("authorizationUrl"), m_redirectUrl },
        { QStringLiteral("lastError"), m_lastError },
        { QStringLiteral("tokenStorage"), QStringLiteral("secret-service") },
        { QStringLiteral("scope"), QStringLiteral("calendar.calendarlist.readonly calendar.events") },
        { QStringLiteral("writeAccessAvailable"), m_writeAccess }
    });
}
