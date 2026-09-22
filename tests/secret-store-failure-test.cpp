#include "secretstore.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/tmp/omarchy-calendar-no-secret-service");
    QCoreApplication app(argc, argv);
    SecretStore store;
    QString error;
    const QString token = store.refreshToken(QStringLiteral("google:missing"), &error);
    if (!token.isEmpty() || error.isEmpty()) return 2;
    return 0;
}
