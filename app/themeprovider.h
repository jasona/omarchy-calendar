#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>

class ThemeProvider final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
    Q_PROPERTY(QColor backgroundDeep READ backgroundDeep NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceRaised READ surfaceRaised NOTIFY themeChanged)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY themeChanged)
    Q_PROPERTY(QColor foregroundMuted READ foregroundMuted NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor red READ red NOTIFY themeChanged)
    Q_PROPERTY(QColor green READ green NOTIFY themeChanged)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY themeChanged)
    Q_PROPERTY(int baseFontSize READ baseFontSize NOTIFY themeChanged)
    Q_PROPERTY(int controlRadius READ controlRadius CONSTANT)
    Q_PROPERTY(int cardRadius READ cardRadius CONSTANT)
    Q_PROPERTY(int panelRadius READ panelRadius CONSTANT)
    Q_PROPERTY(int standardSpacing READ standardSpacing CONSTANT)

public:
    explicit ThemeProvider(QObject *parent = nullptr);

    QColor background() const { return m_background; }
    QColor backgroundDeep() const { return m_backgroundDeep; }
    QColor surface() const { return m_surface; }
    QColor surfaceRaised() const { return m_surfaceRaised; }
    QColor foreground() const { return m_foreground; }
    QColor foregroundMuted() const { return m_foregroundMuted; }
    QColor accent() const { return m_accent; }
    QColor red() const { return m_red; }
    QColor green() const { return m_green; }
    QColor cyan() const { return m_cyan; }
    int baseFontSize() const { return m_baseFontSize; }
    int controlRadius() const { return 9; }
    int cardRadius() const { return 14; }
    int panelRadius() const { return 18; }
    int standardSpacing() const { return 10; }

signals:
    void themeChanged();

private slots:
    void reload();

private:
    static QColor readColor(const QString &contents, const QString &key, const QColor &fallback);
    static int readInteger(const QString &contents, const QString &key, int fallback);
    void watch(const QString &path);

    QFileSystemWatcher m_watcher;
    QColor m_background {"#0b121a"};
    QColor m_backgroundDeep {"#080e14"};
    QColor m_surface {"#111b26"};
    QColor m_surfaceRaised {"#182532"};
    QColor m_foreground {"#b2dff5"};
    QColor m_foregroundMuted {"#86a7b8"};
    QColor m_accent {"#6c8cdb"};
    QColor m_red {"#c86776"};
    QColor m_green {"#72a5f1"};
    QColor m_cyan {"#6bc2ff"};
    int m_baseFontSize = 12;
};
