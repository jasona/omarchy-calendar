#pragma once

#include <QString>

class SecretStore final
{
public:
    bool storeRefreshToken(const QString &accountId, const QString &token, QString *error = nullptr) const;
    QString refreshToken(const QString &accountId, QString *error = nullptr) const;
    bool clearRefreshToken(const QString &accountId, QString *error = nullptr) const;
};
