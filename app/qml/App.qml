import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "views"

ApplicationWindow {
    id: window
    width: 1420
    height: 880
    minimumWidth: 1060
    minimumHeight: 700
    visible: true
    title: "Omarchy Calendar"
    color: theme.backgroundDeep
    font.family: "Inter"
    font.pixelSize: theme.baseFontSize

    // Keep navigational dates as JavaScript Date objects. QML's `date` value
    // type can normalize an instant through UTC and advance the local day.
    property var weekStart: startOfWeek(new Date())
    property var dayDate: new Date()
    property var monthDate: new Date()
    property var selectedEvent: ({})
    property var upcomingEvent: ({})
    property var calendarList: []
    property var accountList: []
    property var providerStatus: ({})
    property string currentView: "week"
    property var hiddenCalendarIds: preferences.hiddenCalendarIds

    function startOfWeek(date) {
        let result = new Date(date)
        let offset = (result.getDay() + 6) % 7
        result.setDate(result.getDate() - offset)
        result.setHours(0, 0, 0, 0)
        return result
    }

    function addDays(date, amount) {
        let result = new Date(date)
        result.setDate(result.getDate() + amount)
        return result
    }

    function formatWeekTitle(start) {
        let end = addDays(start, 6)
        if (start.getFullYear() !== end.getFullYear())
            return Qt.formatDate(start, "MMMM d, yyyy") + " – " + Qt.formatDate(end, "MMMM d, yyyy")
        if (start.getMonth() !== end.getMonth())
            return Qt.formatDate(start, "MMMM d") + " – " + Qt.formatDate(end, "MMMM d, yyyy")
        return Qt.formatDate(start, "MMMM d") + "–" + Qt.formatDate(end, "d, yyyy")
    }

    function changeMonth(date, amount) {
        return new Date(date.getFullYear(), date.getMonth() + amount, 1, 12)
    }

    function navigatePrevious() {
        if (currentView === "day")
            dayDate = addDays(dayDate, -1)
        else if (currentView === "week")
            weekStart = addDays(weekStart, -7)
        else if (currentView === "month")
            monthDate = changeMonth(monthDate, -1)
    }

    function navigateNext() {
        if (currentView === "day")
            dayDate = addDays(dayDate, 1)
        else if (currentView === "week")
            weekStart = addDays(weekStart, 7)
        else if (currentView === "month")
            monthDate = changeMonth(monthDate, 1)
    }

    function goToday() {
        let today = new Date()
        dayDate = today
        weekStart = startOfWeek(today)
        monthDate = today
    }

    function formatEventTime(eventData) {
        if (!eventData || !eventData.id)
            return ""
        if (eventData.allDay)
            return "All day"
        return Qt.formatTime(new Date(eventData.startMs), "h:mm AP")
                + " – " + Qt.formatTime(new Date(eventData.endMs), "h:mm AP")
    }

    function refreshFeedMetadata() {
        calendarList = eventStore.calendars()
        accountList = eventStore.accounts()
        providerStatus = eventStore.providerStatus()
        upcomingEvent = eventStore.nextEvent()
        if (!selectedEvent.id)
            selectedEvent = upcomingEvent
    }

    function connectGoogle() {
        eventStore.beginGoogleAuthorization()
        providerStatus = eventStore.providerStatus()
    }

    function syncGoogle() {
        eventStore.syncNow()
        refreshFeedMetadata()
    }

    function disconnectGoogle() {
        eventStore.disconnectGoogle()
        refreshFeedMetadata()
    }

    function setCalendarSync(calendarId, enabled) {
        eventStore.setCalendarSelected(calendarId, enabled)
        refreshFeedMetadata()
    }

    function calendarVisible(calendarId) {
        return hiddenCalendarIds.indexOf(calendarId) < 0
    }

    function toggleCalendar(calendarId) {
        let updated = hiddenCalendarIds.slice()
        let position = updated.indexOf(calendarId)
        if (position < 0)
            updated.push(calendarId)
        else
            updated.splice(position, 1)
        preferences.hiddenCalendarIds = updated
    }

    Component.onCompleted: refreshFeedMetadata()
    Connections {
        target: eventStore
        function onEventsChanged() { window.refreshFeedMetadata() }
        function onAccountsChanged() { window.refreshFeedMetadata() }
        function onProviderStatusChanged() { window.providerStatus = eventStore.providerStatus() }
        function onAuthorizationRequired(url) {
            window.providerStatus = eventStore.providerStatus()
            Qt.openUrlExternally(url)
        }
    }

    Shortcut { sequence: "T"; enabled: window.currentView !== "search"; onActivated: window.goToday() }
    Shortcut { sequence: "1"; enabled: window.currentView !== "search"; onActivated: window.currentView = "day" }
    Shortcut { sequence: "2"; enabled: window.currentView !== "search"; onActivated: window.currentView = "week" }
    Shortcut { sequence: "3"; enabled: window.currentView !== "search"; onActivated: window.currentView = "month" }
    Shortcut { sequence: "4"; enabled: window.currentView !== "search"; onActivated: window.currentView = "agenda" }
    Shortcut { sequence: "/"; enabled: window.currentView !== "search"; onActivated: window.currentView = "search" }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.currentView = "settings" }
    Shortcut { sequence: "Escape"; enabled: window.currentView === "search" || window.currentView === "settings"; onActivated: window.currentView = "week" }

    Rectangle {
        anchors { fill: parent; margins: 12 }
        radius: 16
        color: theme.background
        border.width: 1
        border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.12)
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                color: theme.backgroundDeep

                RowLayout {
                    anchors { fill: parent; leftMargin: 18; rightMargin: 18 }
                    spacing: 10

                    Text {
                        text: "CALENDAR"
                        color: theme.foreground
                        font.pixelSize: theme.baseFontSize + 1
                        font.weight: Font.Bold
                        font.letterSpacing: 1.7
                        Layout.preferredWidth: 185
                    }

                    CalendarButton { text: "‹"; visible: ["day", "week", "month"].indexOf(window.currentView) >= 0; Layout.preferredWidth: visible ? 36 : 0; onClicked: window.navigatePrevious() }
                    CalendarButton { text: "›"; visible: ["day", "week", "month"].indexOf(window.currentView) >= 0; Layout.preferredWidth: visible ? 36 : 0; onClicked: window.navigateNext() }
                    CalendarButton { text: "Today"; visible: window.currentView !== "search" && window.currentView !== "settings"; onClicked: window.goToday() }

                    Text {
                        id: periodTitle
                        text: window.currentView === "agenda" ? "Agenda"
                              : window.currentView === "search" ? "Search"
                              : window.currentView === "settings" ? "Settings"
                              : window.currentView === "day" ? Qt.formatDate(window.dayDate, "dddd, MMMM d, yyyy")
                              : window.currentView === "month" ? Qt.formatDate(window.monthDate, "MMMM yyyy")
                              : window.formatWeekTitle(window.weekStart)
                        color: theme.foreground
                        font.pixelSize: theme.baseFontSize + 7
                        font.weight: Font.DemiBold
                        Layout.leftMargin: 8
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.preferredWidth: viewButtons.width + 8
                        Layout.preferredHeight: 42
                        radius: 11
                        color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.045)
                        Row {
                            id: viewButtons
                            anchors.centerIn: parent
                            spacing: 2
                            CalendarButton { id: dayButton; text: "Day"; selected: window.currentView === "day"; onClicked: window.currentView = "day" }
                            CalendarButton { id: weekButton; text: "Week"; selected: window.currentView === "week"; onClicked: window.currentView = "week" }
                            CalendarButton { id: monthButton; text: "Month"; selected: window.currentView === "month"; onClicked: window.currentView = "month" }
                        }
                    }

                    CalendarButton { text: "⌕"; Layout.preferredWidth: 38; selected: window.currentView === "search"; onClicked: window.currentView = "search" }
                    CalendarButton { text: "+  New event"; accentColor: theme.accent; selected: true }
                }

                Rectangle {
                    height: 1
                    color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.10)
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    Layout.preferredWidth: 224
                    Layout.fillHeight: true
                    color: theme.backgroundDeep

                    Column {
                        anchors { fill: parent; margins: 18 }
                        spacing: 18

                        Column {
                            width: parent.width
                            spacing: 4
                            CalendarButton { width: parent.width; text: "▦   Calendar"; selected: ["day", "week", "month"].indexOf(window.currentView) >= 0; onClicked: window.currentView = "week" }
                            CalendarButton { width: parent.width; text: "≡   Agenda"; selected: window.currentView === "agenda"; onClicked: window.currentView = "agenda" }
                            CalendarButton { width: parent.width; text: "⌕   Search"; selected: window.currentView === "search"; onClicked: window.currentView = "search" }
                            CalendarButton { width: parent.width; text: "⚙   Settings"; selected: window.currentView === "settings"; onClicked: window.currentView = "settings" }
                        }

                        Rectangle { width: parent.width; height: 1; color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09) }

                        Row {
                            width: parent.width
                            Text { text: "MY CALENDARS"; color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 2); font.weight: Font.Bold; font.letterSpacing: 1.2 }
                        }

                        Repeater {
                            model: window.calendarList
                            delegate: Rectangle {
                                required property var modelData
                                width: parent.width
                                height: 28
                                radius: 7
                                color: calendarPointer.containsMouse ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.06) : "transparent"
                                opacity: window.calendarVisible(modelData.id) ? 1 : 0.48
                                Row {
                                    anchors { fill: parent; leftMargin: 2; rightMargin: 5 }
                                    spacing: 10
                                    Rectangle { width: 9; height: 9; radius: 5; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: modelData.name; color: theme.foreground; font.pixelSize: theme.baseFontSize; width: 145; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideRight }
                                    Text { text: window.calendarVisible(modelData.id) ? "✓" : ""; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize; anchors.verticalCenter: parent.verticalCenter }
                                }
                                MouseArea {
                                    id: calendarPointer
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: window.toggleCalendar(modelData.id)
                                }
                            }
                        }

                        Text {
                            width: parent.width
                            text: (eventStore.serviceBacked ? "LOCAL SERVICE" : "COMPATIBILITY FEED")
                                  + "  ·  " + eventStore.eventCount + " EVENTS"
                            color: theme.foregroundMuted
                            opacity: 0.68
                            font.pixelSize: Math.max(9, theme.baseFontSize - 3)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.7
                            elide: Text.ElideRight
                        }

                        Item { width: 1; height: 6 }

                        Rectangle {
                            width: parent.width
                            height: 118
                            radius: 12
                            color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.10)
                            border.width: 1
                            border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.22)
                            Column {
                                anchors { fill: parent; margins: 13 }
                                spacing: 7
                                Text { text: "UP NEXT"; color: theme.accent; font.pixelSize: Math.max(9, theme.baseFontSize - 2); font.weight: Font.Bold; font.letterSpacing: 1.1 }
                                Text { width: parent.width; text: window.upcomingEvent.title || "Nothing scheduled"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 1; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: window.upcomingEvent.id ? Qt.formatDate(new Date(window.upcomingEvent.startMs), "ddd, MMM d") + " · " + Qt.formatTime(new Date(window.upcomingEvent.startMs), "h:mm AP") : "Your calendar is clear"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize - 1 }
                                CalendarButton { text: "View event"; selected: true; width: 118; height: 30; enabled: !!window.upcomingEvent.id; onClicked: window.selectedEvent = window.upcomingEvent }
                            }
                        }
                    }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.10) }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    WeekView {
                        anchors.fill: parent
                        visible: window.currentView === "week"
                        weekStart: window.weekStart
                        hiddenCalendarIds: window.hiddenCalendarIds
                        onEventSelected: function(data) { window.selectedEvent = data }
                        onDayActivated: function(date) {
                            window.dayDate = date
                            window.currentView = "day"
                        }
                    }

                    DayView {
                        anchors.fill: parent
                        visible: window.currentView === "day"
                        selectedDate: window.dayDate
                        hiddenCalendarIds: window.hiddenCalendarIds
                        onEventSelected: function(data) { window.selectedEvent = data }
                        onDateNavigation: function(amount) { window.dayDate = window.addDays(window.dayDate, amount) }
                    }

                    MonthView {
                        anchors.fill: parent
                        visible: window.currentView === "month"
                        monthDate: window.monthDate
                        hiddenCalendarIds: window.hiddenCalendarIds
                        onEventSelected: function(data) { window.selectedEvent = data }
                        onDayActivated: function(date) {
                            window.dayDate = date
                            window.currentView = "day"
                        }
                    }

                    AgendaView {
                        anchors.fill: parent
                        visible: window.currentView === "agenda"
                        anchorDate: new Date()
                        hiddenCalendarIds: window.hiddenCalendarIds
                        onEventSelected: function(data) { window.selectedEvent = data }
                    }

                    SearchView {
                        anchors.fill: parent
                        visible: window.currentView === "search"
                        hiddenCalendarIds: window.hiddenCalendarIds
                        onEventSelected: function(data) { window.selectedEvent = data }
                    }

                    SettingsView {
                        anchors.fill: parent
                        visible: window.currentView === "settings"
                        accounts: window.accountList
                        calendars: window.calendarList
                        providerStatus: window.providerStatus
                        onConnectGoogle: window.connectGoogle()
                        onSyncGoogle: window.syncGoogle()
                        onDisconnectGoogle: window.disconnectGoogle()
                        onSetCalendarSync: function(calendarId, enabled) { window.setCalendarSync(calendarId, enabled) }
                    }
                }

                Rectangle { visible: window.currentView !== "settings"; Layout.preferredWidth: visible ? 1 : 0; Layout.fillHeight: true; color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.10) }

                Rectangle {
                    visible: window.currentView !== "settings"
                    Layout.preferredWidth: visible ? 278 : 0
                    Layout.fillHeight: true
                    color: theme.backgroundDeep

                    Column {
                        anchors { fill: parent; margins: 20 }
                        spacing: 18

                        Row {
                            width: parent.width
                            Text { text: "Event details"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 4; font.weight: Font.DemiBold; width: parent.width - 24 }
                            Text { text: "×"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize + 5 }
                        }

                        Rectangle { width: 34; height: 4; radius: 2; color: window.selectedEvent.color || theme.accent }

                        Text {
                            width: parent.width
                            text: window.selectedEvent.title || "Select an event"
                            color: theme.foreground
                            font.pixelSize: theme.baseFontSize + 8
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        Column {
                            spacing: 5
                            Text { text: window.selectedEvent.id ? Qt.formatDate(new Date(window.selectedEvent.startMs), "dddd, MMMM d").toUpperCase() : ""; color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 2); font.weight: Font.Bold; font.letterSpacing: 0.8 }
                            Text { text: window.formatEventTime(window.selectedEvent); color: theme.foreground; font.pixelSize: theme.baseFontSize + 1 }
                        }

                        Rectangle { width: parent.width; height: 1; color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09) }

                        Column {
                            spacing: 9
                            Text { text: window.selectedEvent.location || "No location"; color: theme.foreground; font.pixelSize: theme.baseFontSize }
                            Text { text: window.selectedEvent.calendarName || ""; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                            Text { text: window.selectedEvent.allDay ? "All-day event" : "Scheduled event"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                        }

                        Text {
                            width: parent.width
                            text: window.selectedEvent.id ? "Stored offline by the local Omarchy Calendar service. Editing will arrive with Google account synchronization." : "Choose an event to see its details."
                            color: theme.foregroundMuted
                            font.pixelSize: theme.baseFontSize
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }

                        Item { width: 1; height: 4 }
                        CalendarButton {
                            width: parent.width
                            text: "Open in calendar"
                            selected: true
                            enabled: !!window.selectedEvent.eventUrl
                            onClicked: Qt.openUrlExternally(window.selectedEvent.eventUrl)
                        }
                    }
                }
            }
        }
    }
}
