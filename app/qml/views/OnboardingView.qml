import QtQuick
import QtQuick.Layouts
import "../components"

Item {
    id: root
    Accessible.role: Accessible.Pane
    Accessible.name: "Welcome to Omarchy Calendar"
    property var accounts: []
    property var calendars: []
    property var providerStatus: ({})
    property var syncStatus: providerStatus.sync || ({})
    property bool connected: accounts.some(function(account) { return account.provider === "google" })
    signal connectGoogle()
    signal finish()

    Rectangle {
        anchors.fill: parent
        color: theme.background

        Rectangle {
            width: Math.min(850, parent.width - 80)
            height: Math.min(620, parent.height - 70)
            anchors.centerIn: parent
            radius: theme.panelRadius + 6
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.13)

            Rectangle {
                width: 8
                height: parent.height - 48
                x: 24
                y: 24
                radius: 4
                color: theme.accent
            }

            ColumnLayout {
                anchors { fill: parent; leftMargin: 68; rightMargin: 56; topMargin: 48; bottomMargin: 42 }
                spacing: 20

                Text {
                    text: root.connected ? "Your time, ready." : "A calendar that belongs here."
                    color: theme.foreground
                    font.pixelSize: theme.baseFontSize + 22
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                Text {
                    text: root.connected
                          ? "Google Calendar is connected. Your events stay available offline and changes are safely queued whenever the network is unavailable."
                          : "Omarchy Calendar combines a native desktop experience with your Google calendars, offline access, fast keyboard control, and dependable background reminders."
                    color: theme.foregroundMuted
                    font.pixelSize: theme.baseFontSize + 2
                    lineHeight: 1.4
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Repeater {
                        model: [
                            { title: "Private by design", body: "Refresh tokens stay in Secret Service. Calendar data stays in your local SQLite database." },
                            { title: "Works offline", body: "Browse, search, and make changes without a connection. Sync resumes automatically." },
                            { title: "Built for Omarchy", body: "Your active theme, fonts, keyboard workflow, and desktop notifications work together." }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: theme.cardRadius
                            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.045)
                            Column {
                                anchors { fill: parent; margins: 16 }
                                spacing: 9
                                Rectangle { width: 28; height: 4; radius: 2; color: theme.accent }
                                Text { text: modelData.title; color: theme.foreground; font.pixelSize: theme.baseFontSize + 1; font.weight: Font.DemiBold }
                                Text { width: parent.width; text: modelData.body; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize; lineHeight: 1.3; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 112
                    radius: theme.cardRadius
                    color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.09)
                    border.width: 1
                    border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.22)
                    RowLayout {
                        anchors { fill: parent; margins: 18 }
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Text {
                                text: root.connected ? root.calendars.length + " calendar" + (root.calendars.length === 1 ? "" : "s") + " connected"
                                                     : root.providerStatus.configured ? "Connect Google Calendar" : "Google setup is required"
                                color: theme.foreground
                                font.pixelSize: theme.baseFontSize + 3
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.connected
                                      ? (root.syncStatus.state === "syncing" ? "Finishing your first synchronization…" : "Everything is ready for your first day.")
                                      : root.providerStatus.configured
                                        ? "A browser window will open for Google’s secure consent flow."
                                        : "This build needs its public OAuth client before sign-in can begin."
                                color: theme.foregroundMuted
                                font.pixelSize: theme.baseFontSize
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                visible: !!(root.providerStatus.lastError || root.syncStatus.lastError)
                                text: root.providerStatus.lastError || root.syncStatus.lastError || ""
                                color: theme.red
                                font.pixelSize: theme.baseFontSize
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        CalendarButton {
                            visible: !root.connected
                            text: root.providerStatus.state === "authorizing" ? "Waiting for Google…" : "Connect Google"
                            selected: true
                            enabled: !!root.providerStatus.configured && root.providerStatus.state !== "authorizing"
                            onClicked: root.connectGoogle()
                        }
                        CalendarButton {
                            visible: root.connected
                            text: root.syncStatus.state === "syncing" ? "Syncing…" : "Open calendar"
                            selected: true
                            enabled: root.syncStatus.state !== "syncing"
                            onClicked: root.finish()
                        }
                    }
                }

                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "You can change accounts and synced calendars later in Settings."; color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 1) }
                    Item { Layout.fillWidth: true }
                    CalendarButton { visible: !root.connected; text: "Explore offline"; onClicked: root.finish() }
                }
            }
        }
    }
}
