#include "themeprovider.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {
QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString();
}
}

ThemeProvider::ThemeProvider(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ThemeProvider::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ThemeProvider::reload);
    reload();
}

QColor ThemeProvider::readColor(const QString &contents, const QString &key, const QColor &fallback)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*\"(#[0-9A-Fa-f]{6,8})\"")
            .arg(QRegularExpression::escape(key)));
    const auto match = expression.match(contents);
    if (!match.hasMatch())
        return fallback;

    const QColor parsed(match.captured(1));
    return parsed.isValid() ? parsed : fallback;
}

int ThemeProvider::readInteger(const QString &contents, const QString &key, int fallback)
{
    const QRegularExpression expression(
        QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*(\\d+)")
            .arg(QRegularExpression::escape(key)));
    const auto match = expression.match(contents);
    return match.hasMatch() ? match.captured(1).toInt() : fallback;
}

void ThemeProvider::watch(const QString &path)
{
    if (QFile::exists(path) && !m_watcher.files().contains(path))
        m_watcher.addPath(path);
}

void ThemeProvider::reload()
{
    const QString home = QDir::homePath();
    const QString colorsPath = home + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    const QString themeShellPath = home + QStringLiteral("/.local/state/omarchy/current/theme/shell.toml");
    const QString userShellPath = home + QStringLiteral("/.config/omarchy/shell.toml");

    const QString colors = readFile(colorsPath);
    const QString themeShell = readFile(themeShellPath);
    const QString userShell = readFile(userShellPath);

    m_background = readColor(colors, QStringLiteral("background"), m_background);
    m_backgroundDeep = readColor(colors, QStringLiteral("dark_background"), m_backgroundDeep);
    m_surface = readColor(colors, QStringLiteral("lighter_background"), m_surface);
    m_surfaceRaised = m_surface.lighter(112);
    m_foreground = readColor(colors, QStringLiteral("foreground"), m_foreground);
    m_foregroundMuted = readColor(colors, QStringLiteral("dark_foreground"), m_foregroundMuted);
    m_accent = readColor(colors, QStringLiteral("accent"), m_accent);
    m_red = readColor(colors, QStringLiteral("red"), m_red);
    m_green = readColor(colors, QStringLiteral("green"), m_green);
    m_cyan = readColor(colors, QStringLiteral("cyan"), m_cyan);
    m_baseFontSize = readInteger(userShell, QStringLiteral("base-size"),
                                 readInteger(themeShell, QStringLiteral("base-size"), 12));

    watch(colorsPath);
    watch(themeShellPath);
    watch(userShellPath);
    emit themeChanged();
}
