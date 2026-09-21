#include "eventstore.h"
#include "preferences.h"
#include "themeprovider.h"
#include "timezonehelper.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Omarchy Calendar"));
    app.setOrganizationName(QStringLiteral("Omarchy"));
    app.setDesktopFileName(QStringLiteral("org.omarchy.Calendar"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    ThemeProvider theme;
    EventStore eventStore;
    Preferences preferences;
    TimeZoneHelper timeZones;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("eventStore"), &eventStore);
    engine.rootContext()->setContextProperty(QStringLiteral("preferences"), &preferences);
    engine.rootContext()->setContextProperty(QStringLiteral("timeZones"), &timeZones);
    engine.load(QUrl(QStringLiteral("qrc:/qml/App.qml")));

    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    return app.exec();
}
