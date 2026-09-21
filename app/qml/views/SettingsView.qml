import QtQuick
import QtQuick.Controls
import "../components"

Item {
    id: root
    property var accounts: []
    property var calendars: []
    property var providerStatus: ({})
    property var syncStatus: providerStatus.sync || ({})
    property var mutationStatus: providerStatus.mutations || ({})
    property var googleAccount: accounts.filter(function(account) { return account.provider === "google" })[0] || ({})
    property var googleCalendars: calendars.filter(function(calendar) { return calendar.source === "google" })
    property bool googleConnected: !!googleAccount.id
    signal connectGoogle()
    signal syncGoogle()
    signal disconnectGoogle()
    signal setCalendarSync(string calendarId, bool enabled)

    function syncSummary() {
        if (mutationStatus.state === "uploading")
            return "Uploading " + mutationStatus.pendingCount + " queued change" + (mutationStatus.pendingCount === 1 ? "" : "s")
        if (mutationStatus.state === "retrying")
            return "A queued change will retry automatically"
        if (mutationStatus.state === "conflict")
            return "A calendar change needs review before it can upload"
        if (syncStatus.state === "syncing") {
            var progress = syncStatus.calendarsTotal > 0
                    ? " · " + syncStatus.calendarsCompleted + " of " + syncStatus.calendarsTotal + " calendars"
                    : ""
            return "Syncing " + (syncStatus.stage || "Google Calendar") + progress
        }
        if (syncStatus.state === "retrying")
            return syncStatus.online ? "Sync paused · retry scheduled automatically" : "Sync paused · waiting for a network connection"
        var lastSuccess = syncStatus.lastSuccessAt || googleAccount.lastSyncAt
        if (lastSuccess) {
            var date = new Date(lastSuccess)
            return "Last synced " + date.toLocaleString(Qt.locale(), Locale.ShortFormat)
        }
        return "Connected · ready to sync"
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background

        Column {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
            spacing: 22

            Column {
                spacing: 6
                Text {
                    text: "Accounts"
                    color: theme.foreground
                    font.pixelSize: theme.baseFontSize + 8
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Connect calendar providers and control how their data is stored."
                    color: theme.foregroundMuted
                    font.pixelSize: theme.baseFontSize
                }
            }

            Rectangle {
                width: parent.width
                height: 272
                radius: 14
                color: theme.backgroundDeep
                border.width: 1
                border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.11)

                Row {
                    anchors { fill: parent; margins: 22 }
                    spacing: 18

                    Rectangle {
                        width: 46; height: 46; radius: 12
                        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.16)
                        Text {
                            anchors.centerIn: parent
                            text: "G"
                            color: theme.accent
                            font.pixelSize: theme.baseFontSize + 10
                            font.weight: Font.Bold
                        }
                    }

                    Column {
                        width: parent.width - 64
                        spacing: 10
                        Text {
                            text: root.googleConnected ? "Google Calendar connected" : "Google Calendar"
                            color: theme.foreground
                            font.pixelSize: theme.baseFontSize + 4
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: root.googleConnected
                                  ? (root.googleAccount.email || root.googleAccount.displayName)
                                    + "\n" + root.syncSummary()
                                    + (!root.providerStatus.writeAccessAvailable
                                       ? "\nEnable editing once to send locally queued changes to Google."
                                       : "")
                                  : root.providerStatus.configured
                                    ? "Connect securely in your browser. Calendar-list access stays read-only; event access enables synchronization and editing."
                                    : "Google connection is ready for an OAuth client. Add the application credentials to the user service to enable account sign-in."
                            color: theme.foregroundMuted
                            font.pixelSize: theme.baseFontSize
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            visible: root.syncStatus.state === "syncing"
                            width: parent.width
                            height: 4
                            radius: 2
                            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.1)
                            Rectangle {
                                height: parent.height
                                radius: parent.radius
                                color: theme.accent
                                width: root.syncStatus.calendarsTotal > 0
                                       ? parent.width * Math.max(0.06, root.syncStatus.calendarsCompleted / root.syncStatus.calendarsTotal)
                                       : parent.width * 0.12
                            }
                        }

                        Row {
                            spacing: 9
                            Rectangle {
                                width: secureLabel.implicitWidth + 18; height: 27; radius: 7
                                color: Qt.rgba(theme.green.r, theme.green.g, theme.green.b, 0.12)
                                Text { id: secureLabel; anchors.centerIn: parent; text: "Secret Service"; color: theme.green; font.pixelSize: theme.baseFontSize - 2; font.weight: Font.DemiBold }
                            }
                            Rectangle {
                                width: scopeLabel.implicitWidth + 18; height: 27; radius: 7
                                color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                                Text { id: scopeLabel; anchors.centerIn: parent; text: root.providerStatus.writeAccessAvailable ? "Editing enabled" : "Read-only access"; color: theme.accent; font.pixelSize: theme.baseFontSize - 2; font.weight: Font.DemiBold }
                            }
                        }

                        Row {
                            spacing: 8
                            CalendarButton {
                                width: 158
                                text: root.googleConnected ? (root.syncStatus.state === "syncing" ? "Syncing…" : "Sync now")
                                      : !root.providerStatus.configured ? "OAuth setup required"
                                      : root.providerStatus.state === "authorizing" ? "Waiting for Google…"
                                      : "Connect Google"
                                selected: true
                                enabled: root.googleConnected
                                         ? root.syncStatus.state !== "syncing"
                                         : !!root.providerStatus.configured && root.providerStatus.state !== "authorizing"
                                onClicked: root.googleConnected ? root.syncGoogle() : root.connectGoogle()
                            }
                            CalendarButton {
                                visible: root.googleConnected && !root.providerStatus.writeAccessAvailable
                                text: root.providerStatus.state === "authorizing" ? "Waiting for Google…" : "Enable editing"
                                selected: true
                                enabled: root.providerStatus.state !== "authorizing"
                                onClicked: root.connectGoogle()
                            }
                            CalendarButton {
                                visible: root.googleConnected
                                text: "Disconnect"
                                onClicked: disconnectDialog.open()
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: root.googleConnected && root.googleCalendars.length > 0
                width: parent.width
                height: 96 + calendarSyncList.height
                radius: 14
                color: theme.backgroundDeep
                border.width: 1
                border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.11)

                Column {
                    anchors { fill: parent; margins: 18 }
                    spacing: 10
                    Text {
                        text: "Calendars to sync"
                        color: theme.foreground
                        font.pixelSize: theme.baseFontSize + 2
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Paused calendars stay cached and can be restored instantly."
                        color: theme.foregroundMuted
                        font.pixelSize: theme.baseFontSize - 1
                    }
                    ListView {
                        id: calendarSyncList
                        width: parent.width
                        height: Math.min(count, 4) * 48
                        clip: true
                        model: root.googleCalendars
                        spacing: 2
                        delegate: Item {
                            required property var modelData
                            width: calendarSyncList.width
                            height: 46
                            Row {
                                anchors.fill: parent
                                spacing: 10
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 9; height: 9; radius: 5
                                    color: modelData.color || theme.accent
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 116
                                    spacing: 2
                                    Text {
                                        width: parent.width
                                        text: modelData.name
                                        color: theme.foreground
                                        font.pixelSize: theme.baseFontSize
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.accessRole === "owner" ? "Owned by you" : "Shared calendar"
                                        color: theme.foregroundMuted
                                        font.pixelSize: theme.baseFontSize - 2
                                    }
                                }
                                CalendarButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 88
                                    text: modelData.selected ? "Synced" : "Paused"
                                    selected: modelData.selected
                                    enabled: root.syncStatus.state !== "syncing"
                                    onClicked: root.setCalendarSync(modelData.id, !modelData.selected)
                                }
                            }
                        }
                    }
                }
            }

            Text {
                visible: (syncStatus.lastError && syncStatus.lastError.length > 0)
                         || (mutationStatus.lastError && mutationStatus.lastError.length > 0)
                         || (providerStatus.lastError && providerStatus.lastError.length > 0)
                width: parent.width
                text: mutationStatus.lastError || syncStatus.lastError || providerStatus.lastError || ""
                color: theme.red
                font.pixelSize: theme.baseFontSize
                wrapMode: Text.WordWrap
            }

            Text {
                text: "Refresh tokens stay in the desktop Secret Service. The calendar database stores account metadata, synchronization state, and event data only."
                width: parent.width
                color: theme.foregroundMuted
                font.pixelSize: theme.baseFontSize
                lineHeight: 1.35
                wrapMode: Text.WordWrap
            }
        }
    }

    Popup {
        id: disconnectDialog
        parent: root
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        width: 410
        height: 205
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0

        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.46) }
        background: Rectangle {
            radius: 14
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.16)
        }

        contentItem: Column {
            x: 24
            y: 22
            width: parent.width - 48
            spacing: 12
            Text {
                text: "Disconnect Google Calendar?"
                color: theme.foreground
                font.pixelSize: theme.baseFontSize + 4
                font.weight: Font.DemiBold
            }
            Text {
                width: 362
                text: "This removes the account, its cached events, and the saved refresh token from this computer."
                color: theme.foregroundMuted
                font.pixelSize: theme.baseFontSize
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }
            Row {
                spacing: 8
                CalendarButton {
                    text: "Keep account"
                    onClicked: disconnectDialog.close()
                }
                CalendarButton {
                    text: "Disconnect"
                    selected: true
                    accentColor: theme.red
                    onClicked: {
                        disconnectDialog.close()
                        root.disconnectGoogle()
                    }
                }
            }
        }
    }
}
