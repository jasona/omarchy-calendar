import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    Accessible.role: Accessible.Pane
    Accessible.name: "Calendar settings"
    property string currentSection: "general"
    property var accounts: []
    property var calendars: []
    property var providerStatus: ({})
    property var syncStatus: providerStatus.sync || ({})
    property var mutationStatus: providerStatus.mutations || ({})
    property var googleAccount: accounts.filter(function(account) { return account.provider === "google" })[0] || ({})
    property var googleCalendars: calendars.filter(function(calendar) { return calendar.source === "google" })
    property bool googleConnected: !!googleAccount.id
    property var pendingChanges: []
    property var discardCandidate: ({})
    property bool diagnosticsCopied: false
    signal connectGoogle()
    signal syncGoogle()
    signal disconnectGoogle()
    signal setCalendarSync(string calendarId, bool enabled)
    signal showOnboarding()

    function refreshPendingChanges() {
        pendingChanges = eventStore.pendingMutations()
    }

    function operationLabel(operation) {
        if (operation === "create") return "Create"
        if (operation === "move") return "Move"
        if (operation.indexOf("delete") === 0) return "Delete"
        if (operation.indexOf("update") === 0) return "Update"
        if (operation === "rsvp") return "RSVP"
        return operation
    }

    onVisibleChanged: if (visible) refreshPendingChanges()
    onMutationStatusChanged: if (visible) refreshPendingChanges()

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

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: settingsMenu
                Layout.preferredWidth: 176
                Layout.fillHeight: true
                color: theme.backgroundDeep

                Column {
                    anchors { fill: parent; margins: 18 }
                    spacing: 8

                    Text {
                        text: "SETTINGS"
                        color: theme.foregroundMuted
                        font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                        bottomPadding: 8
                    }
                    CalendarButton {
                        width: parent.width
                        text: "General"
                        accessibleName: "General settings"
                        selected: root.currentSection === "general"
                        onClicked: root.currentSection = "general"
                    }
                    CalendarButton {
                        width: parent.width
                        text: "About"
                        accessibleName: "About Omarchy Calendar"
                        selected: root.currentSection === "about"
                        onClicked: root.currentSection = "about"
                    }
                }

                Rectangle {
                    anchors { top: parent.top; right: parent.right; bottom: parent.bottom }
                    width: 1
                    color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09)
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    anchors.fill: parent
                    visible: root.currentSection === "general"

                    ScrollView {
                        id: settingsScroll
                        anchors { fill: parent; margins: 28 }
                        contentWidth: availableWidth
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                        width: settingsScroll.availableWidth
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
                height: 78
                radius: 14
                color: theme.backgroundDeep
                border.width: 1
                border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.11)
                RowLayout {
                    anchors { fill: parent; margins: 16 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text { text: "Interface density"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 1; font.weight: Font.DemiBold }
                        Text { text: "Choose comfortable spacing or fit more of the day on screen."; color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 1) }
                    }
                    CalendarButton {
                        text: "Comfortable"
                        selected: preferences.interfaceDensity === "comfortable"
                        onClicked: preferences.interfaceDensity = "comfortable"
                    }
                    CalendarButton {
                        text: "Compact"
                        selected: preferences.interfaceDensity === "compact"
                        onClicked: preferences.interfaceDensity = "compact"
                    }
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

            Rectangle {
                visible: root.pendingChanges.length > 0
                width: parent.width
                height: 76 + mutationList.height
                radius: 14
                color: theme.backgroundDeep
                border.width: 1
                border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.11)

                Column {
                    anchors { fill: parent; margins: 18 }
                    spacing: 10
                    Text {
                        text: "Queued changes"
                        color: theme.foreground
                        font.pixelSize: theme.baseFontSize + 2
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Changes are stored locally until Google confirms them. Failed changes stay here for review."
                        color: theme.foregroundMuted
                        font.pixelSize: theme.baseFontSize - 1
                    }
                    ListView {
                        id: mutationList
                        width: parent.width
                        height: Math.min(count, 3) * 82
                        model: root.pendingChanges
                        clip: true
                        spacing: 6
                        delegate: Rectangle {
                            required property var modelData
                            width: mutationList.width
                            height: 76
                            radius: 9
                            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.045)
                            RowLayout {
                                anchors { fill: parent; margins: 10 }
                                spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.operationLabel(modelData.operation) + " · " + modelData.title
                                        color: theme.foreground
                                        font.pixelSize: theme.baseFontSize
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.calendarName + " · " + modelData.state
                                              + (modelData.attemptCount ? " · attempt " + modelData.attemptCount : "")
                                        color: modelData.state === "failed" || modelData.state === "conflict"
                                               ? theme.red : theme.foregroundMuted
                                        font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        visible: !!modelData.lastError
                                        Layout.fillWidth: true
                                        text: modelData.lastError || ""
                                        color: theme.foregroundMuted
                                        font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                                        elide: Text.ElideRight
                                    }
                                }
                                CalendarButton {
                                    visible: modelData.canRetry
                                    text: "Retry"
                                    selected: true
                                    onClicked: {
                                        eventStore.retryMutation(modelData.id)
                                        root.refreshPendingChanges()
                                    }
                                }
                                CalendarButton {
                                    visible: modelData.canDiscard
                                    text: "Discard"
                                    onClicked: {
                                        root.discardCandidate = modelData
                                        discardDialog.open()
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar {}
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
            Row {
                width: parent.width
                spacing: 10
                CalendarButton {
                    text: root.diagnosticsCopied ? "✓ Diagnostics copied" : "Copy redacted diagnostics"
                    selected: root.diagnosticsCopied
                    onClicked: {
                        root.diagnosticsCopied = eventStore.copyDiagnostics()
                        diagnosticsReset.restart()
                    }
                }
                CalendarButton {
                    text: "Show welcome guide"
                    onClicked: root.showOnboarding()
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(180, parent.width - 390)
                    text: "Includes versions, counts, sync state, and errors. Excludes tokens, event content, and account identity."
                    color: theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                    wrapMode: Text.WordWrap
                }
            }
            Column {
                width: parent.width
                spacing: 8
                Flow {
                    width: parent.width
                    spacing: 10
                    CalendarButton {
                        text: "Website"
                        accessibleName: "Open the Omarchy Calendar website"
                        onClicked: Qt.openUrlExternally(releaseInfo.homepageUrl)
                    }
                    CalendarButton {
                        text: "Privacy policy"
                        accessibleName: "Open Omarchy Calendar privacy policy"
                        onClicked: Qt.openUrlExternally(releaseInfo.privacyUrl)
                    }
                    CalendarButton {
                        text: "Terms"
                        accessibleName: "Open Omarchy Calendar terms of use"
                        onClicked: Qt.openUrlExternally(releaseInfo.termsUrl)
                    }
                    CalendarButton {
                        text: "Email support"
                        accessibleName: "Email Omarchy Calendar support"
                        onClicked: Qt.openUrlExternally("mailto:" + releaseInfo.supportEmail)
                    }
                }
                Text {
                    width: parent.width
                    text: "Google data stays on this computer and is never sent to a project server."
                    color: theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                    wrapMode: Text.WordWrap
                }
            }
                        Item { width: 1; height: 12 }
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: root.currentSection === "about"

                    ScrollView {
                        id: aboutScroll
                        anchors { fill: parent; margins: 28 }
                        contentWidth: availableWidth
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            width: aboutScroll.availableWidth
                            spacing: 22

                            Column {
                                width: parent.width
                                spacing: 6
                                Text {
                                    text: "About"
                                    color: theme.foreground
                                    font.pixelSize: theme.baseFontSize + 8
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: "Product information, version details, and privacy resources."
                                    color: theme.foregroundMuted
                                    font.pixelSize: theme.baseFontSize
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 330
                                radius: 16
                                color: theme.backgroundDeep
                                border.width: 1
                                border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.11)

                                Column {
                                    anchors { fill: parent; margins: 24 }
                                    spacing: 20

                                    Row {
                                        width: parent.width
                                        spacing: 18
                                        Rectangle {
                                            width: 64
                                            height: 64
                                            radius: 16
                                            color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.17)
                                            Text {
                                                anchors.centerIn: parent
                                                text: "OC"
                                                color: theme.accent
                                                font.pixelSize: theme.baseFontSize + 11
                                                font.weight: Font.Bold
                                            }
                                        }
                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 5
                                            Text {
                                                text: releaseInfo.appName || "Omarchy Calendar"
                                                color: theme.foreground
                                                font.pixelSize: theme.baseFontSize + 7
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                text: "Version " + (releaseInfo.version || "Unknown")
                                                color: theme.foregroundMuted
                                                font.pixelSize: theme.baseFontSize + 1
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: parent.width
                                        height: 1
                                        color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09)
                                    }

                                    Column {
                                        width: parent.width
                                        spacing: 6
                                        Text {
                                            text: "A product of"
                                            color: theme.foregroundMuted
                                            font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                                        }
                                        Text {
                                            text: releaseInfo.publisher || "Last Refuge Software, LLC."
                                            color: theme.foreground
                                            font.pixelSize: theme.baseFontSize + 3
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    Flow {
                                        width: parent.width
                                        spacing: 10
                                        CalendarButton {
                                            text: releaseInfo.publisherUrl || "https://lastrefuge.ai"
                                            accessibleName: "Open Last Refuge website"
                                            outlined: true
                                            onClicked: Qt.openUrlExternally(releaseInfo.publisherUrl || "https://lastrefuge.ai")
                                        }
                                        CalendarButton {
                                            text: "Privacy policy"
                                            accessibleName: "Open Omarchy Calendar privacy policy"
                                            outlined: true
                                            onClicked: Qt.openUrlExternally(releaseInfo.privacyUrl || "https://lastrefuge.ai/privacy")
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: releaseInfo.privacyUrl || "https://lastrefuge.ai/privacy"
                                        color: theme.foregroundMuted
                                        font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: "Omarchy Calendar is designed and maintained by Last Refuge Software, LLC."
                                color: theme.foregroundMuted
                                font.pixelSize: theme.baseFontSize
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: diagnosticsReset
        interval: 3000
        onTriggered: root.diagnosticsCopied = false
    }

    Popup {
        id: discardDialog
        parent: root
        anchors.centerIn: parent
        width: 430
        height: 215
        modal: true
        dim: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.46) }
        background: Rectangle {
            radius: 14
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.16)
        }
        contentItem: Column {
            x: 24; y: 22; width: parent.width - 48; spacing: 12
            Text {
                text: "Discard this local change?"
                color: theme.foreground
                font.pixelSize: theme.baseFontSize + 4
                font.weight: Font.DemiBold
            }
            Text {
                width: 382
                text: "“" + (root.discardCandidate.title || "Untitled event")
                      + "” will return to Google’s saved version after synchronization. An event that was never uploaded will be removed."
                color: theme.foregroundMuted
                font.pixelSize: theme.baseFontSize
                wrapMode: Text.WordWrap
            }
            Row {
                spacing: 8
                CalendarButton { text: "Keep change"; onClicked: discardDialog.close() }
                CalendarButton {
                    text: "Discard"
                    selected: true
                    accentColor: theme.red
                    onClicked: {
                        eventStore.discardMutation(root.discardCandidate.id)
                        discardDialog.close()
                        root.refreshPendingChanges()
                    }
                }
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
