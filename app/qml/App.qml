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
    property string pendingDeleteToken: ""
    property string pendingDeleteTitle: ""
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
        let zone = eventData.timeZone || timeZones.systemTimeZoneId
        let start = timeZones.wallTime(eventData.startMs, zone)
        let end = timeZones.wallTime(eventData.endMs, zone)
        return start.timeLabel + " – " + end.timeLabel + "  " + start.abbreviation
    }

    function formatEventDate(eventData) {
        if (!eventData || !eventData.id)
            return ""
        let start = eventData.allDay
                  ? new Date(eventData.allDayStartDate + "T12:00:00")
                  : new Date(timeZones.wallTime(eventData.startMs, eventData.timeZone || timeZones.systemTimeZoneId).date + "T12:00:00")
        let end = eventData.allDay
                ? new Date(eventData.allDayEndDate + "T12:00:00")
                : new Date(timeZones.wallTime(eventData.endMs - 1, eventData.timeZone || timeZones.systemTimeZoneId).date + "T12:00:00")
        if (eventData.allDay)
            end.setDate(end.getDate() - 1)
        if (Qt.formatDate(start, "yyyy-MM-dd") === Qt.formatDate(end, "yyyy-MM-dd"))
            return Qt.formatDate(start, "dddd, MMMM d")
        return Qt.formatDate(start, "MMM d") + " – " + Qt.formatDate(end, "MMM d, yyyy")
    }

    function localEquivalent(eventData) {
        if (!eventData || !eventData.id || eventData.allDay
                || !eventData.timeZone || eventData.timeZone === timeZones.systemTimeZoneId)
            return ""
        let start = timeZones.wallTime(eventData.startMs, timeZones.systemTimeZoneId)
        let end = timeZones.wallTime(eventData.endMs, timeZones.systemTimeZoneId)
        let dates = start.date === end.date ? start.dateLabel : start.dateLabel + " – " + end.dateLabel
        return "Local time · " + dates + " · " + start.timeLabel + " – " + end.timeLabel
                + "  " + start.abbreviation
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

    function openEventEditor() {
        eventEditor.openForDate(currentView === "day" ? dayDate : new Date())
    }

    function eventEditable(eventData) {
        if (!eventData || !eventData.id || eventData.source === "compat-json"
                )
            return false
        return calendarList.some(function(calendar) {
            return calendar.id === eventData.calendarId
                    && (calendar.accessRole === "owner" || calendar.accessRole === "writer")
        })
    }

    function editSelectedEvent() {
        if (eventEditable(selectedEvent))
            eventEditor.openForEvent(selectedEvent)
    }

    function adjustEvent(eventData, minuteDelta, dayDelta, resizeDelta) {
        let source = selectedEvent.id === eventData.id
                   && selectedEvent.calendarId === eventData.calendarId ? selectedEvent : eventData
        if (!eventEditable(source)) return false
        let start = new Date(source.startMs)
        let end = new Date(source.endMs)
        if (source.allDay) {
            let startDate = new Date(source.allDayStartDate + "T12:00:00")
            let endDate = new Date(source.allDayEndDate + "T12:00:00")
            startDate.setDate(startDate.getDate() + dayDelta)
            endDate.setDate(endDate.getDate() + dayDelta + Math.round(resizeDelta / 1440))
            if (endDate <= startDate) return false
            start = new Date(startDate.getFullYear(), startDate.getMonth(), startDate.getDate())
            end = new Date(endDate.getFullYear(), endDate.getMonth(), endDate.getDate())
            minuteDelta = 0
            dayDelta = 0
        }
        if (dayDelta) {
            start.setDate(start.getDate() + dayDelta)
            end.setDate(end.getDate() + dayDelta)
        }
        if (minuteDelta) {
            start.setMinutes(start.getMinutes() + minuteDelta)
            end.setMinutes(end.getMinutes() + minuteDelta)
        }
        if (resizeDelta)
            end.setMinutes(end.getMinutes() + resizeDelta)
        if (end.getTime() - start.getTime() < 15 * 60000) return false
        let payload = {
            id: source.id, calendarId: source.calendarId,
            title: source.title, description: source.description || "",
            location: source.location || "", allDay: false,
            startMs: start.getTime(), endMs: end.getTime(),
            timeZone: source.timeZone || ""
        }
        if (source.allDay) {
            payload.allDay = true
            payload.allDayStartDate = Qt.formatDate(start, "yyyy-MM-dd")
            payload.allDayEndDate = Qt.formatDate(end, "yyyy-MM-dd")
        }
        if (!eventStore.updateEvent(payload)) return false
        selectedEvent = Object.assign({}, source, payload)
        return true
    }

    function eventDeletable(eventData) {
        if (!eventData || !eventData.id || eventData.source === "compat-json"
                || pendingDeleteToken.length > 0)
            return false
        return calendarList.some(function(calendar) {
            return calendar.id === eventData.calendarId
                    && (calendar.accessRole === "owner" || calendar.accessRole === "writer")
        })
    }

    function deleteSelectedEvent() {
        if (!eventDeletable(selectedEvent)) return
        if (selectedEvent.isRecurring) {
            deleteScopePopup.deleteCandidate = Object.assign({}, selectedEvent)
            deleteScopePopup.open()
            return
        }
        performDelete(selectedEvent, "occurrence")
    }

    function performDelete(eventData, scope) {
        let title = eventData.title || "Event"
        let token = eventStore.deleteEventScoped({
            calendarId: eventData.calendarId,
            id: eventData.id,
            scope: scope,
            seriesId: eventData.seriesId || eventData.id,
            originalStartMs: eventData.originalStartMs || eventData.startMs,
            originalStartDate: eventData.originalStartDate || eventData.allDayStartDate || ""
        })
        if (!token) return
        pendingDeleteTitle = title
        pendingDeleteToken = token
        selectedEvent = ({})
        deleteUndoTimer.restart()
        refreshFeedMetadata()
    }

    function undoLastDelete() {
        if (!pendingDeleteToken || !eventStore.undoDelete(pendingDeleteToken)) return
        deleteUndoTimer.stop()
        pendingDeleteToken = ""
        pendingDeleteTitle = ""
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
    Shortcut { sequence: "N"; enabled: !eventEditor.opened; onActivated: window.openEventEditor() }
    Shortcut { sequence: "E"; enabled: !eventEditor.opened && window.eventEditable(window.selectedEvent); onActivated: window.editSelectedEvent() }
    Shortcut { sequence: "Delete"; enabled: !eventEditor.opened && window.eventDeletable(window.selectedEvent); onActivated: window.deleteSelectedEvent() }
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
                    CalendarButton { text: "+  New event"; accentColor: theme.accent; selected: true; onClicked: window.openEventEditor() }
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
                        canAdjustEvent: function(data) { return window.eventEditable(data) }
                        onEventAdjusted: function(data, minuteDelta, dayDelta, resizeDelta) { window.adjustEvent(data, minuteDelta, dayDelta, resizeDelta) }
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
                        canAdjustEvent: function(data) { return window.eventEditable(data) }
                        onEventAdjusted: function(data, minuteDelta, dayDelta, resizeDelta) { window.adjustEvent(data, minuteDelta, dayDelta, resizeDelta) }
                        onEventSelected: function(data) { window.selectedEvent = data }
                        onDateNavigation: function(amount) { window.dayDate = window.addDays(window.dayDate, amount) }
                    }

                    MonthView {
                        anchors.fill: parent
                        visible: window.currentView === "month"
                        monthDate: window.monthDate
                        hiddenCalendarIds: window.hiddenCalendarIds
                        canAdjustEvent: function(data) { return window.eventEditable(data) }
                        onEventAdjusted: function(data, minuteDelta, dayDelta, resizeDelta) { window.adjustEvent(data, minuteDelta, dayDelta, resizeDelta) }
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
                            Text { text: window.formatEventDate(window.selectedEvent).toUpperCase(); color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 2); font.weight: Font.Bold; font.letterSpacing: 0.8 }
                            Text { text: window.formatEventTime(window.selectedEvent); color: theme.foreground; font.pixelSize: theme.baseFontSize + 1 }
                            Text {
                                visible: text.length > 0
                                text: window.localEquivalent(window.selectedEvent)
                                color: theme.accent
                                font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                            }
                        }

                        Rectangle { width: parent.width; height: 1; color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09) }

                        Column {
                            spacing: 9
                            Text { text: window.selectedEvent.location || "No location"; color: theme.foreground; font.pixelSize: theme.baseFontSize }
                            Text { text: window.selectedEvent.calendarName || ""; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                            Text { text: window.selectedEvent.allDay ? "All-day event" : "Scheduled event"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                            Text { visible: !window.selectedEvent.allDay && !!window.selectedEvent.timeZone; text: window.selectedEvent.timeZone || ""; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                            Text {
                                visible: !!window.selectedEvent.isRecurring
                                text: window.selectedEvent.isSeriesMaster ? "Recurring series"
                                      : window.selectedEvent.isException
                                        ? "Recurring series · changed occurrence"
                                        : "Recurring occurrence"
                                color: theme.accent
                                font.pixelSize: theme.baseFontSize
                            }
                        }

                        Text {
                            width: parent.width
                            text: !window.selectedEvent.id ? "Choose an event to see its details."
                                  : window.selectedEvent.id.indexOf("local:") === 0
                                    ? "Queued locally · waiting for Google write access. This event is safely stored and will survive a restart."
                                    : "Stored offline by the local Omarchy Calendar service."
                            color: theme.foregroundMuted
                            font.pixelSize: theme.baseFontSize
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            visible: window.eventEditable(window.selectedEvent)
                            width: parent.width
                            text: window.selectedEvent.allDay
                                  ? "Drag across Week or Month to move · Alt+←/→ moves by a day\nEdit the event to switch between all-day and scheduled time."
                                  : "Drag to move · drag the lower edge to resize\nAlt+arrows move · Alt+Shift+↑/↓ resize"
                            color: theme.accent
                            opacity: 0.82
                            font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                            lineHeight: 1.35
                            wrapMode: Text.WordWrap
                        }

                        Item { width: 1; height: 4 }
                        CalendarButton {
                            width: parent.width
                            text: "Edit event"
                            selected: true
                            enabled: window.eventEditable(window.selectedEvent)
                            onClicked: window.editSelectedEvent()
                        }
                        CalendarButton {
                            width: parent.width
                            text: "Open in calendar"
                            enabled: !!window.selectedEvent.eventUrl
                            onClicked: Qt.openUrlExternally(window.selectedEvent.eventUrl)
                        }
                        CalendarButton {
                            width: parent.width
                            text: "Delete event"
                            selected: true
                            accentColor: theme.red
                            enabled: window.eventDeletable(window.selectedEvent)
                            onClicked: window.deleteSelectedEvent()
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: deleteUndoTimer
        interval: 6000
        onTriggered: {
            window.pendingDeleteToken = ""
            window.pendingDeleteTitle = ""
        }
    }

    Rectangle {
        visible: window.pendingDeleteToken.length > 0
        z: 100
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 34
        width: Math.min(460, parent.width - 48)
        height: 58
        radius: 14
        color: theme.backgroundDeep
        border.width: 1
        border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.18)

        RowLayout {
            anchors { fill: parent; leftMargin: 18; rightMargin: 10 }
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: window.pendingDeleteTitle + " deleted"
                color: theme.foreground
                font.pixelSize: theme.baseFontSize
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            CalendarButton {
                text: "Undo"
                selected: true
                onClicked: window.undoLastDelete()
            }
        }
    }


    Popup {
        id: deleteScopePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 430
        modal: true
        focus: true
        padding: 24
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property var deleteCandidate: ({})

        function choose(scope) {
            let candidate = deleteCandidate
            close()
            window.performDelete(candidate, scope)
        }

        background: Rectangle {
            radius: 18
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.18)
        }

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "Delete recurring event"
                color: theme.foreground
                font.pixelSize: theme.baseFontSize + 8
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: "Choose how much of this series to remove. You can undo for six seconds."
                color: theme.foregroundMuted
                font.pixelSize: theme.baseFontSize
                wrapMode: Text.WordWrap
            }
            CalendarButton {
                visible: !deleteScopePopup.deleteCandidate.isSeriesMaster
                Layout.fillWidth: true
                text: "This occurrence"
                onClicked: deleteScopePopup.choose("occurrence")
            }
            CalendarButton {
                visible: !deleteScopePopup.deleteCandidate.isSeriesMaster
                Layout.fillWidth: true
                text: "This and following"
                onClicked: deleteScopePopup.choose("future")
            }
            CalendarButton {
                Layout.fillWidth: true
                text: "Entire series"
                selected: true
                accentColor: theme.red
                onClicked: deleteScopePopup.choose("series")
            }
            CalendarButton {
                Layout.fillWidth: true
                text: "Cancel"
                onClicked: deleteScopePopup.close()
            }
        }
    }


    Popup {
        id: eventEditor
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 470
        height: Math.min(700, window.height - 28)
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property var editingEvent: ({})
        property bool editing: !!editingEvent.id
        property var writableCalendars: window.calendarList.filter(function(calendar) {
            return calendar.accessRole === "owner" || calendar.accessRole === "writer"
        })
        property var timezoneOptions: timeZones.options(Date.now(), preferences.recentTimeZones)
        property var repeatOptions: [
            { label: "Does not repeat" },
            { label: "Daily" },
            { label: "Every weekday" },
            { label: "Weekly" },
            { label: "Monthly" },
            { label: "Yearly" },
            { label: "Custom…" }
        ]
        property var customUnitOptions: [
            { label: "days", frequency: "DAILY" },
            { label: "weeks", frequency: "WEEKLY" },
            { label: "months", frequency: "MONTHLY" },
            { label: "years", frequency: "YEARLY" }
        ]
        property var repeatEndOptions: [ { label: "Never" }, { label: "After" } ]
        property var editScopeOptions: [
            { label: "This occurrence" },
            { label: "This and following" },
            { label: "Entire series" }
        ]
        property bool preferLaterStart: false
        property bool preferLaterEnd: false
        property var startResolution: allDayToggle.checked ? ({ valid: true })
            : timeZones.resolveWallTime(dateField.text, startField.text,
                                        selectedTimeZone(), preferLaterStart)
        property var endResolution: allDayToggle.checked ? ({ valid: true })
            : timeZones.resolveWallTime(endDateField.text, endField.text,
                                        selectedTimeZone(), preferLaterEnd)

        function selectedTimeZone() {
            return timezoneField.currentIndex >= 0 && timezoneField.currentIndex < timezoneOptions.length
                    ? timezoneOptions[timezoneField.currentIndex].id : timeZones.systemTimeZoneId
        }

        function selectTimeZone(zoneId) {
            let target = zoneId || timeZones.systemTimeZoneId
            for (let index = 0; index < timezoneOptions.length; ++index) {
                if (timezoneOptions[index].id === target) {
                    timezoneField.currentIndex = index
                    return
                }
            }
            timezoneField.currentIndex = 0
        }

        function selectCalendarTimeZone() {
            if (calendarField.currentIndex < 0 || calendarField.currentIndex >= writableCalendars.length)
                return
            selectTimeZone(writableCalendars[calendarField.currentIndex].timeZone)
        }

        function validDateText(value) {
            if (!/^\d{4}-\d{2}-\d{2}$/.test(value)) return false
            let parts = value.split("-")
            let date = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]), 12)
            return Qt.formatDate(date, "yyyy-MM-dd") === value
        }

        function recurrenceRule() {
            let rule = ""
            if (repeatField.currentIndex === 1)
                rule = "RRULE:FREQ=DAILY"
            else if (repeatField.currentIndex === 2)
                rule = "RRULE:FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR"
            else if (repeatField.currentIndex === 3) {
                let tokens = ["SU", "MO", "TU", "WE", "TH", "FR", "SA"]
                let date = new Date(dateField.text + "T12:00:00")
                rule = "RRULE:FREQ=WEEKLY;BYDAY=" + tokens[date.getDay()]
            } else if (repeatField.currentIndex === 4)
                rule = "RRULE:FREQ=MONTHLY"
            else if (repeatField.currentIndex === 5)
                rule = "RRULE:FREQ=YEARLY"
            else if (repeatField.currentIndex === 6) {
                let unit = customUnitOptions[customUnitField.currentIndex]
                let interval = Number(customIntervalField.text)
                rule = "RRULE:FREQ=" + unit.frequency + ";INTERVAL=" + interval
                if (unit.frequency === "WEEKLY") {
                    let tokens = ["SU", "MO", "TU", "WE", "TH", "FR", "SA"]
                    let date = new Date(dateField.text + "T12:00:00")
                    rule += ";BYDAY=" + tokens[date.getDay()]
                }
            }
            if (rule.length > 0 && repeatEndField.currentIndex === 1)
                rule += ";COUNT=" + Number(repeatCountField.text)
            return rule
        }

        function openForDate(date) {
            editingEvent = ({})
            let start = new Date(date)
            let now = new Date()
            start.setHours(date.toDateString() === now.toDateString() ? now.getHours() + 1 : 9, 0, 0, 0)
            titleField.text = ""
            locationField.text = ""
            descriptionField.text = ""
            allDayToggle.checked = false
            repeatField.currentIndex = 0
            customIntervalField.text = "2"
            customUnitField.currentIndex = 1
            repeatEndField.currentIndex = 0
            repeatCountField.text = "10"
            dateField.text = Qt.formatDate(start, "yyyy-MM-dd")
            endDateField.text = Qt.formatDate(start, "yyyy-MM-dd")
            startField.text = Qt.formatTime(start, "HH:mm")
            let end = new Date(start.getTime() + 60 * 60 * 1000)
            endField.text = Qt.formatTime(end, "HH:mm")
            preferLaterStart = false
            preferLaterEnd = false
            selectCalendarTimeZone()
            errorText.text = ""
            open()
            titleField.forceActiveFocus()
        }

        function openForEvent(eventData) {
            editingEvent = eventData
            titleField.text = eventData.title || ""
            locationField.text = eventData.location || ""
            descriptionField.text = eventData.description || ""
            let zone = eventData.timeZone || timeZones.systemTimeZoneId
            let start = timeZones.wallTime(eventData.startMs, zone)
            let end = timeZones.wallTime(eventData.endMs, zone)
            allDayToggle.checked = !!eventData.allDay
            repeatField.currentIndex = 0
            repeatEndField.currentIndex = 0
            editScopeField.currentIndex = 0
            dateField.text = eventData.allDay ? eventData.allDayStartDate : start.date
            let inclusiveEnd = new Date(eventData.allDayEndDate + "T12:00:00")
            if (eventData.allDay) inclusiveEnd.setDate(inclusiveEnd.getDate() - 1)
            endDateField.text = eventData.allDay ? Qt.formatDate(inclusiveEnd, "yyyy-MM-dd") : end.date
            startField.text = start.time
            endField.text = end.time
            preferLaterStart = start.ambiguous && start.occurrence === 2
            preferLaterEnd = end.ambiguous && end.occurrence === 2
            errorText.text = ""
            for (let index = 0; index < writableCalendars.length; ++index) {
                if (writableCalendars[index].id === eventData.calendarId) {
                    calendarField.currentIndex = index
                    break
                }
            }
            selectTimeZone(zone)
            open()
            titleField.forceActiveFocus()
        }

        function saveEvent() {
            let calendar = writableCalendars[calendarField.currentIndex]
            let zone = selectedTimeZone()
            let startMs = 0
            let endMs = 0
            let allDayEndDate = ""
            if (allDayToggle.checked) {
                if (!validDateText(dateField.text) || !validDateText(endDateField.text)) {
                    errorText.text = "Enter valid dates as YYYY-MM-DD."
                    return
                }
                let startDate = new Date(dateField.text + "T12:00:00")
                let inclusiveEnd = new Date(endDateField.text + "T12:00:00")
                inclusiveEnd.setDate(inclusiveEnd.getDate() + 1)
                startMs = startDate.getTime()
                endMs = inclusiveEnd.getTime()
                allDayEndDate = Qt.formatDate(inclusiveEnd, "yyyy-MM-dd")
            } else {
                if (!startResolution.valid || !endResolution.valid) {
                    errorText.text = !startResolution.valid ? startResolution.error : endResolution.error
                    return
                }
                startMs = startResolution.epochMs
                endMs = endResolution.epochMs
            }
            if (endMs <= startMs) {
                errorText.text = "The end time must be after the start time."
                return
            }
            if (!editing && repeatField.currentIndex === 6
                    && (!/^\d+$/.test(customIntervalField.text)
                        || Number(customIntervalField.text) < 1
                        || Number(customIntervalField.text) > 99)) {
                errorText.text = "Custom intervals must be between 1 and 99."
                return
            }
            if (!editing && repeatField.currentIndex > 0 && repeatEndField.currentIndex === 1
                    && (!/^\d+$/.test(repeatCountField.text)
                        || Number(repeatCountField.text) < 1
                        || Number(repeatCountField.text) > 999)) {
                errorText.text = "Occurrences must be between 1 and 999."
                return
            }
            let payload = { calendarId: calendar.id, title: titleField.text.trim(), location: locationField.text.trim(), description: descriptionField.text.trim(), startMs: startMs, endMs: endMs, allDay: allDayToggle.checked, timeZone: zone }
            if (allDayToggle.checked) {
                payload.allDayStartDate = dateField.text
                payload.allDayEndDate = allDayEndDate
            }
            let rule = recurrenceRule()
            if (!editing && rule.length > 0)
                payload.recurrence = [rule]
            if (editing) {
                payload.id = editingEvent.id
                if (editingEvent.isRecurring) {
                    let futureScope = !editingEvent.isSeriesMaster && editScopeField.currentIndex === 1
                    let seriesScope = editingEvent.isSeriesMaster || editScopeField.currentIndex === 2
                    payload.scope = futureScope ? "future" : seriesScope ? "series" : "occurrence"
                    if (seriesScope) {
                        payload.seriesId = editingEvent.seriesId || editingEvent.id
                        payload.scopeBaseStartMs = editingEvent.startMs
                        payload.scopeBaseEndMs = editingEvent.endMs
                        payload.scopeBaseAllDayStartDate = editingEvent.allDayStartDate || ""
                        payload.scopeBaseAllDayEndDate = editingEvent.allDayEndDate || ""
                    }
                    if (futureScope) {
                        payload.seriesId = editingEvent.seriesId
                        payload.scopeBaseStartMs = editingEvent.startMs
                        payload.scopeBaseEndMs = editingEvent.endMs
                        payload.scopeBaseAllDayStartDate = editingEvent.allDayStartDate || ""
                        payload.scopeBaseAllDayEndDate = editingEvent.allDayEndDate || ""
                        payload.scopeOriginalStartMs = editingEvent.originalStartMs || editingEvent.startMs
                        payload.scopeOriginalStartDate = editingEvent.originalStartDate
                                                    || editingEvent.allDayStartDate || ""
                    }
                }
            }
            let saved = editing ? eventStore.updateEvent(payload) : !!eventStore.createEvent(payload)
            if (!saved) {
                errorText.text = endMs <= startMs ? "The end time must be after the start time." : "The event could not be saved."
                return
            }
            preferences.rememberTimeZone(zone)
            if (editing)
                window.selectedEvent = Object.assign({}, window.selectedEvent, payload)
            close()
            window.refreshFeedMetadata()
        }

        Shortcut { sequence: "Ctrl+Return"; enabled: eventEditor.opened && saveButton.enabled; onActivated: eventEditor.saveEvent() }

        background: Rectangle {
            radius: 18
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.18)
        }

        ColumnLayout {
            anchors { fill: parent; margins: 26 }
            spacing: 14
            Text { text: eventEditor.editing ? "Edit event" : "New event"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 10; font.weight: Font.DemiBold }
            Text {
                text: eventEditor.editing && eventEditor.editingEvent.isRecurring
                      ? eventEditor.editingEvent.isSeriesMaster || editScopeField.currentIndex === 2
                        ? "Changes apply to the entire series and synchronize with Google."
                        : editScopeField.currentIndex === 1
                          ? "A new series begins here; earlier occurrences stay unchanged."
                        : "Changes apply to this occurrence and synchronize with Google."
                      : eventEditor.editing
                        ? "Changes are saved locally first, then synchronized with Google."
                        : "Saved instantly on this device and queued for Google."
                color: theme.foregroundMuted
                font.pixelSize: theme.baseFontSize
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            ThemedComboBox {
                id: editScopeField
                visible: eventEditor.editing && eventEditor.editingEvent.isRecurring
                         && !eventEditor.editingEvent.isSeriesMaster
                Layout.fillWidth: true
                model: eventEditor.editScopeOptions
                textRole: "label"
            }
            ThemedTextField { id: titleField; Layout.fillWidth: true; placeholderText: "Event title"; font.pixelSize: theme.baseFontSize + 3 }
            ThemedComboBox {
                id: calendarField
                Layout.fillWidth: true
                model: eventEditor.writableCalendars
                textRole: "name"
                enabled: !eventEditor.editing
                onActivated: if (!eventEditor.editing) eventEditor.selectCalendarTimeZone()
            }
            CalendarButton {
                id: allDayToggle
                checkable: true
                text: checked ? "✓  All-day event" : "All-day event"
                selected: checked
                Layout.alignment: Qt.AlignLeft
            }
            ThemedComboBox {
                id: repeatField
                visible: !eventEditor.editing
                Layout.fillWidth: true
                model: eventEditor.repeatOptions
                textRole: "label"
            }
            RowLayout {
                visible: !eventEditor.editing && repeatField.currentIndex === 6
                Layout.fillWidth: true
                Text { text: "Every"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                ThemedTextField {
                    id: customIntervalField
                    Layout.preferredWidth: 72
                    text: "2"
                    inputMethodHints: Qt.ImhDigitsOnly
                    horizontalAlignment: TextInput.AlignHCenter
                }
                ThemedComboBox {
                    id: customUnitField
                    Layout.fillWidth: true
                    model: eventEditor.customUnitOptions
                    textRole: "label"
                }
            }
            RowLayout {
                visible: !eventEditor.editing && repeatField.currentIndex > 0
                Layout.fillWidth: true
                Text { text: "Ends"; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
                ThemedComboBox {
                    id: repeatEndField
                    Layout.fillWidth: true
                    model: eventEditor.repeatEndOptions
                    textRole: "label"
                }
                ThemedTextField {
                    id: repeatCountField
                    visible: repeatEndField.currentIndex === 1
                    Layout.preferredWidth: visible ? 72 : 0
                    text: "10"
                    inputMethodHints: Qt.ImhDigitsOnly
                    horizontalAlignment: TextInput.AlignHCenter
                }
                Text {
                    visible: repeatEndField.currentIndex === 1
                    text: "occurrences"
                    color: theme.foregroundMuted
                    font.pixelSize: theme.baseFontSize
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedTextField { id: dateField; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD" }
                ThemedTextField { id: startField; visible: !allDayToggle.checked; Layout.preferredWidth: visible ? 92 : 0; placeholderText: "09:00" }
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedTextField { id: endDateField; Layout.fillWidth: true; placeholderText: "End date · YYYY-MM-DD" }
                ThemedTextField { id: endField; visible: !allDayToggle.checked; Layout.preferredWidth: visible ? 92 : 0; placeholderText: "10:00" }
            }
            ThemedComboBox {
                id: timezoneField
                visible: !allDayToggle.checked
                Layout.fillWidth: true
                model: eventEditor.timezoneOptions
                textRole: "label"
                onActivated: {
                    eventEditor.preferLaterStart = false
                    eventEditor.preferLaterEnd = false
                }
            }
            Text {
                visible: !allDayToggle.checked
                Layout.fillWidth: true
                text: {
                    if (!eventEditor.startResolution.valid)
                        return eventEditor.startResolution.error || ""
                    let source = eventEditor.startResolution.timeLabel + " "
                               + eventEditor.startResolution.abbreviation + " · "
                               + eventEditor.startResolution.offsetLabel
                    if (!eventEditor.startResolution.isLocal)
                        source += "  →  Local " + eventEditor.startResolution.localDateLabel
                                + " · " + eventEditor.startResolution.localTimeLabel + " "
                                + eventEditor.startResolution.localAbbreviation
                    return source
                }
                color: eventEditor.startResolution.valid ? theme.foregroundMuted : theme.red
                font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                wrapMode: Text.WordWrap
            }
            RowLayout {
                visible: !allDayToggle.checked && eventEditor.startResolution.valid
                         && eventEditor.startResolution.ambiguous
                Layout.fillWidth: true
                Text {
                    text: "Start time repeats:"
                    color: theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                }
                Item { Layout.fillWidth: true }
                CalendarButton {
                    text: "First"
                    selected: !eventEditor.preferLaterStart
                    onClicked: eventEditor.preferLaterStart = false
                }
                CalendarButton {
                    text: "Second"
                    selected: eventEditor.preferLaterStart
                    onClicked: eventEditor.preferLaterStart = true
                }
            }
            RowLayout {
                visible: !allDayToggle.checked && eventEditor.endResolution.valid
                         && eventEditor.endResolution.ambiguous
                Layout.fillWidth: true
                Text {
                    text: "End time repeats:"
                    color: theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 1)
                }
                Item { Layout.fillWidth: true }
                CalendarButton {
                    text: "First"
                    selected: !eventEditor.preferLaterEnd
                    onClicked: eventEditor.preferLaterEnd = false
                }
                CalendarButton {
                    text: "Second"
                    selected: eventEditor.preferLaterEnd
                    onClicked: eventEditor.preferLaterEnd = true
                }
            }
            ThemedTextField { id: locationField; Layout.fillWidth: true; placeholderText: "Location (optional)" }
            ThemedTextArea { id: descriptionField; Layout.fillWidth: true; Layout.fillHeight: true; placeholderText: "Notes (optional)" }
            Text { id: errorText; Layout.fillWidth: true; color: theme.red; font.pixelSize: theme.baseFontSize; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                CalendarButton { text: "Cancel"; onClicked: eventEditor.close() }
                CalendarButton {
                    id: saveButton
                    text: eventEditor.editing ? "Save changes" : "Create event"
                    selected: true
                    enabled: titleField.text.trim().length > 0 && calendarField.currentIndex >= 0
                    onClicked: eventEditor.saveEvent()
                }
            }
        }
    }
}
