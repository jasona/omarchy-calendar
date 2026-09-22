#include "themeprovider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cmath>

namespace {
double luminance(const QColor &color)
{
    const auto channel = [](double value) {
        value /= 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return .2126 * channel(color.red()) + .7152 * channel(color.green()) + .0722 * channel(color.blue());
}
double contrast(const QColor &a, const QColor &b)
{
    const double first = luminance(a), second = luminance(b);
    return (std::max(first, second) + .05) / (std::min(first, second) + .05);
}
bool write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}

int main(int argc, char **argv)
{
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 2;
    qputenv("HOME", temporary.path().toUtf8());
    const QString theme = temporary.path() + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    const QString shell = temporary.path() + QStringLiteral("/.config/omarchy/shell.toml");
    if (!write(theme, "background = \"#111111\"\nforeground = \"#181818\"\ndark_foreground = \"#202020\"\naccent = \"#222222\"\nred = \"#242424\"\ngreen = \"#262626\"\n")
        || !write(shell, "base-size = 60\n")) return 3;
    QCoreApplication app(argc, argv);
    ThemeProvider provider;
    if (provider.baseFontSize() != 20
        || contrast(provider.foreground(), provider.background()) < 7.0
        || contrast(provider.foregroundMuted(), provider.background()) < 4.5
        || contrast(provider.accent(), provider.background()) < 4.5
        || contrast(provider.red(), provider.background()) < 4.5) return 4;
    return 0;
}
