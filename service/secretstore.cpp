#include "secretstore.h"

#ifdef signals
#undef signals
#endif
#include <libsecret/secret.h>

namespace {
const SecretSchema tokenSchema = {
    "org.omarchy.Calendar.RefreshToken",
    SECRET_SCHEMA_NONE,
    {
        { "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
        { "provider", SECRET_SCHEMA_ATTRIBUTE_STRING },
        { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
    },
    0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

void assignError(QString *target, GError *error)
{
    if (target)
        *target = error ? QString::fromUtf8(error->message) : QString();
}
}

bool SecretStore::storeRefreshToken(const QString &accountId, const QString &token, QString *error) const
{
    GError *secretError = nullptr;
    const QByteArray account = accountId.toUtf8();
    const QByteArray password = token.toUtf8();
    const QByteArray label = QStringLiteral("Omarchy Calendar Google account %1").arg(accountId).toUtf8();
    const gboolean stored = secret_password_store_sync(
        &tokenSchema, SECRET_COLLECTION_DEFAULT, label.constData(), password.constData(),
        nullptr, &secretError,
        "account", account.constData(), "provider", "google", nullptr);
    assignError(error, secretError);
    if (secretError)
        g_error_free(secretError);
    return stored;
}

QString SecretStore::refreshToken(const QString &accountId, QString *error) const
{
    GError *secretError = nullptr;
    const QByteArray account = accountId.toUtf8();
    gchar *password = secret_password_lookup_sync(
        &tokenSchema, nullptr, &secretError,
        "account", account.constData(), "provider", "google", nullptr);
    assignError(error, secretError);
    if (secretError)
        g_error_free(secretError);
    const QString result = password ? QString::fromUtf8(password) : QString();
    secret_password_free(password);
    return result;
}

bool SecretStore::clearRefreshToken(const QString &accountId, QString *error) const
{
    GError *secretError = nullptr;
    const QByteArray account = accountId.toUtf8();
    const gboolean cleared = secret_password_clear_sync(
        &tokenSchema, nullptr, &secretError,
        "account", account.constData(), "provider", "google", nullptr);
    assignError(error, secretError);
    if (secretError)
        g_error_free(secretError);
    return cleared;
}
