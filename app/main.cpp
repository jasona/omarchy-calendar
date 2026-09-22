#include "eventstore.h"
#include "preferences.h"
#include "quickentryparser.h"
#include "themeprovider.h"
#include "timezonehelper.h"
#include "version.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QTextStream>
#include <QVariantMap>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Omarchy Calendar"));
    app.setOrganizationName(QStringLiteral("Omarchy"));
    app.setDesktopFileName(QStringLiteral("org.omarchy.Calendar"));
    app.setApplicationVersion(QStringLiteral(OMARCHY_CALENDAR_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A native calendar for Omarchy"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption viewOption(QStringLiteral("view"),
        QStringLiteral("Open a specific view (day, week, month, agenda, search, settings, or onboarding)."),
        QStringLiteral("name"));
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
        QStringLiteral("Save a deterministic window capture and exit."), QStringLiteral("path"));
    QCommandLineOption releaseInfoOption(QStringLiteral("release-info"),
        QStringLiteral("Print public release metadata as JSON and exit."));
    parser.addOption(viewOption);
    parser.addOption(screenshotOption);
    parser.addOption(releaseInfoOption);
    parser.process(app);

    const QVariantMap releaseInfo {
        { QStringLiteral("appName"), app.applicationName() },
        { QStringLiteral("version"), app.applicationVersion() },
        { QStringLiteral("publisher"), QStringLiteral("Last Refuge Software, LLC.") },
        { QStringLiteral("publisherUrl"), QStringLiteral("https://lastrefuge.ai") },
        { QStringLiteral("homepageUrl"), QStringLiteral(OMARCHY_CALENDAR_HOMEPAGE_URL) },
        { QStringLiteral("privacyUrl"), QStringLiteral(OMARCHY_CALENDAR_PRIVACY_URL) },
        { QStringLiteral("termsUrl"), QStringLiteral(OMARCHY_CALENDAR_TERMS_URL) },
        { QStringLiteral("supportEmail"), QStringLiteral(OMARCHY_CALENDAR_SUPPORT_EMAIL) }
    };
    if (parser.isSet(releaseInfoOption)) {
        QTextStream(stdout) << QJsonDocument(QJsonObject::fromVariantMap(releaseInfo))
                                   .toJson(QJsonDocument::Compact) << Qt::endl;
        return EXIT_SUCCESS;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    ThemeProvider theme;
    EventStore eventStore;
    Preferences preferences;
    QuickEntryParser quickEntry;
    TimeZoneHelper timeZones;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("eventStore"), &eventStore);
    engine.rootContext()->setContextProperty(QStringLiteral("preferences"), &preferences);
    engine.rootContext()->setContextProperty(QStringLiteral("quickEntry"), &quickEntry);
    engine.rootContext()->setContextProperty(QStringLiteral("timeZones"), &timeZones);
    engine.rootContext()->setContextProperty(QStringLiteral("releaseInfo"), releaseInfo);
    engine.load(QUrl(QStringLiteral("qrc:/qml/App.qml")));

    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (parser.isSet(viewOption) && window)
        window->setProperty("currentView", parser.value(viewOption));
    if (parser.isSet(screenshotOption)) {
        if (!window) return EXIT_FAILURE;
        const QString path = QFileInfo(parser.value(screenshotOption)).absoluteFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QTimer::singleShot(1200, window, [window, path, &app] {
            const bool saved = window->grabWindow().save(path);
            app.exit(saved ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }

    return app.exec();
}
