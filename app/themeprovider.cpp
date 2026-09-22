#include "themeprovider.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace {
QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll())
        : QString();
}

double relativeLuminance(const QColor &color)
{
    const auto channel = [](double value) {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(color.red()) + 0.7152 * channel(color.green())
        + 0.0722 * channel(color.blue());
}

double contrastRatio(const QColor &first, const QColor &second)
{
    const double a = relativeLuminance(first);
    const double b = relativeLuminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

QColor ensureContrast(QColor candidate, const QColor &background, double minimum)
{
    const bool darkBackground = relativeLuminance(background) < 0.35;
    for (int attempt = 0; attempt < 18 && contrastRatio(candidate, background) < minimum; ++attempt)
        candidate = darkBackground ? candidate.lighter(108) : candidate.darker(108);
    if (contrastRatio(candidate, background) >= minimum) return candidate;
    return darkBackground ? QColor(Qt::white) : QColor(Qt::black);
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
    m_foreground = ensureContrast(m_foreground, m_background, 7.0);
    m_foregroundMuted = ensureContrast(m_foregroundMuted, m_background, 4.5);
    m_accent = ensureContrast(m_accent, m_background, 4.5);
    m_red = ensureContrast(m_red, m_background, 4.5);
    m_green = ensureContrast(m_green, m_background, 4.5);
    m_baseFontSize = qBound(10, readInteger(userShell, QStringLiteral("base-size"),
                                 readInteger(themeShell, QStringLiteral("base-size"), 12)), 20);

    watch(colorsPath);
    watch(themeShellPath);
    watch(userShellPath);
    emit themeChanged();
}
