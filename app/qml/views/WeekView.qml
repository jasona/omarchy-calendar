import QtQuick
import "../components"

Item {
    id: root
    Accessible.role: Accessible.Pane
    Accessible.name: "Week calendar"
    Accessible.description: "Use arrow keys to move between days and events. Press Enter for details."
    property var weekStart: new Date()
    property int startHour: 7
    property int endHour: 20
    property real hourHeight: 70
    property var weekEvents: []
    property var hiddenCalendarIds: []
    property var visibleWeekEvents: weekEvents.filter(function(item) { return root.calendarVisible(item) })
    property int cursorDay: initialCursorDay()
    property int eventCursor: -1
    property var canAdjustEvent: function(eventData) { return false }
    signal eventSelected(var eventData)
    signal dayActivated(var dayDate)
    signal eventAdjusted(var eventData, int minuteDelta, int dayDelta, int resizeDelta)

    function addDays(date, amount) {
        let result = new Date(date)
        result.setDate(result.getDate() + amount)
        return result
    }

    function refreshEvents() {
        let lastDay = addDays(weekStart, 6)
        weekEvents = eventStore.eventsForRange(
                    Qt.formatDate(weekStart, "yyyy-MM-dd"),
                    Qt.formatDate(lastDay, "yyyy-MM-dd"))
    }

    function dayIndex(dateKey) {
        let parts = dateKey.split("-")
        let eventDate = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]), 12)
        let start = new Date(weekStart.getFullYear(), weekStart.getMonth(), weekStart.getDate(), 12)
        return Math.round((eventDate.getTime() - start.getTime()) / 86400000)
    }

    function eventY(date) {
        return (date.getHours() - startHour + date.getMinutes() / 60) * hourHeight
    }

    function eventHeight(start, end) {
        return Math.max(38, (end.getTime() - start.getTime()) / 3600000 * hourHeight - 3)
    }

    function segmentBoundary(dateKey, nextDay) {
        let parts = dateKey.split("-")
        let result = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
        if (nextDay) result.setDate(result.getDate() + 1)
        return result
    }

    function segmentStart(eventData) {
        return new Date(Math.max(eventData.startMs, segmentBoundary(eventData.dateKey, false).getTime()))
    }

    function segmentEnd(eventData) {
        return new Date(Math.min(eventData.endMs, segmentBoundary(eventData.dateKey, true).getTime()))
    }

    function eventTime(eventData) {
        if (eventData.allDay)
            return "All day"
        let start = new Date(eventData.startMs)
        let end = new Date(eventData.endMs)
        return Qt.formatTime(start, "h:mm AP") + " – " + Qt.formatTime(end, "h:mm AP")
    }

    function calendarVisible(eventData) {
        return hiddenCalendarIds.indexOf(eventData.calendarId) < 0
    }

    function initialCursorDay() {
        let todayIndex = dayIndex(Qt.formatDate(new Date(), "yyyy-MM-dd"))
        return todayIndex >= 0 && todayIndex < 7 ? todayIndex : 0
    }

    function moveDay(amount) {
        cursorDay = Math.max(0, Math.min(6, cursorDay + amount))
        let key = Qt.formatDate(addDays(weekStart, cursorDay), "yyyy-MM-dd")
        eventCursor = visibleWeekEvents.findIndex(function(item) { return item.dateKey === key })
        if (eventCursor >= 0)
            eventSelected(visibleWeekEvents[eventCursor])
    }

    function moveEvent(amount) {
        if (!visibleWeekEvents.length)
            return
        eventCursor = Math.max(0, Math.min(visibleWeekEvents.length - 1, eventCursor < 0 ? (amount > 0 ? 0 : visibleWeekEvents.length - 1) : eventCursor + amount))
        let eventData = visibleWeekEvents[eventCursor]
        cursorDay = Math.max(0, Math.min(6, dayIndex(eventData.dateKey)))
        if (!eventData.allDay) {
            let targetY = eventY(new Date(eventData.startMs)) - hourHeight
            schedule.contentY = Math.max(0, Math.min(schedule.contentHeight - schedule.height, targetY))
        }
        eventSelected(eventData)
    }

    function eventIndex(eventData) {
        return visibleWeekEvents.findIndex(function(item) {
            return item.id === eventData.id && item.dateKey === eventData.dateKey
        })
    }

    function snapMinutes(pixels) {
        return Math.round((pixels / hourHeight * 60) / 15) * 15
    }

    function adjustSelected(minuteDelta, dayDelta, resizeDelta) {
        if (eventCursor < 0 || eventCursor >= visibleWeekEvents.length) return
        let data = visibleWeekEvents[eventCursor]
        if (!data.allDay && canAdjustEvent(data))
            eventAdjusted(data, minuteDelta, dayDelta, resizeDelta)
    }

    Component.onCompleted: refreshEvents()
    onWeekStartChanged: {
        cursorDay = initialCursorDay()
        eventCursor = -1
        refreshEvents()
    }
    onVisibleChanged: if (visible) forceActiveFocus()
    Keys.onLeftPressed: function(event) {
        if (event.modifiers & Qt.AltModifier) adjustSelected(0, -1, 0)
        else moveDay(-1)
    }
    Keys.onRightPressed: function(event) {
        if (event.modifiers & Qt.AltModifier) adjustSelected(0, 1, 0)
        else moveDay(1)
    }
    Keys.onUpPressed: function(event) {
        if (event.modifiers & Qt.AltModifier)
            adjustSelected(event.modifiers & Qt.ShiftModifier ? 0 : -15, 0,
                           event.modifiers & Qt.ShiftModifier ? -15 : 0)
        else moveEvent(-1)
    }
    Keys.onDownPressed: function(event) {
        if (event.modifiers & Qt.AltModifier)
            adjustSelected(event.modifiers & Qt.ShiftModifier ? 0 : 15, 0,
                           event.modifiers & Qt.ShiftModifier ? 15 : 0)
        else moveEvent(1)
    }
    Keys.onReturnPressed: {
        if (eventCursor >= 0)
            eventSelected(visibleWeekEvents[eventCursor])
        else
            dayActivated(addDays(weekStart, cursorDay))
    }
    Keys.onEnterPressed: {
        if (eventCursor >= 0)
            eventSelected(visibleWeekEvents[eventCursor])
        else
            dayActivated(addDays(weekStart, cursorDay))
    }
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_J) {
            moveEvent(1)
            event.accepted = true
        } else if (event.key === Qt.Key_K) {
            moveEvent(-1)
            event.accepted = true
        }
    }
    Connections {
        target: eventStore
        function onEventsChanged() { root.refreshEvents() }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background

        Row {
            id: dayHeader
            height: 84
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 58 }

            Repeater {
                model: 7
                delegate: Item {
                    required property int index
                    property var dayDate: root.addDays(root.weekStart, index)
                    property bool isToday: Qt.formatDate(dayDate, "yyyy-MM-dd") === Qt.formatDate(new Date(), "yyyy-MM-dd")
                    width: dayHeader.width / 7
                    height: dayHeader.height

                    Rectangle {
                        anchors { fill: parent; margins: 3 }
                        radius: 9
                        color: "transparent"
                        border.width: index === root.cursorDay && root.activeFocus ? 1 : 0
                        border.color: theme.accent
                    }

                    Column {
                        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 10 }
                        spacing: 3
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: Qt.formatDate(dayDate, "ddd").toUpperCase()
                            color: isToday ? theme.accent : theme.foregroundMuted
                            font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.1
                        }
                        Rectangle {
                            width: 30; height: 30; radius: 15
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: isToday ? theme.accent : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: dayDate.getDate()
                                color: isToday ? theme.backgroundDeep : theme.foreground
                                font.pixelSize: theme.baseFontSize + 1
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            Repeater {
                model: root.weekEvents.filter(function(item) { return item.allDay && root.calendarVisible(item) }).slice(0, 7)
                delegate: Rectangle {
                    required property var modelData
                    property int column: root.dayIndex(modelData.dateKey)
                    x: column * dayHeader.width / 7 + 4
                    y: 60
                    width: dayHeader.width / 7 - 8
                    height: 19
                    radius: 5
                    visible: column >= 0 && column < 7
                    color: modelData.color
                    opacity: 0.82
                    Text {
                        anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
                        text: modelData.title
                        color: theme.backgroundDeep
                        font.pixelSize: Math.max(9, theme.baseFontSize - 2)
                        font.weight: Font.DemiBold
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: pressed && root.canAdjustEvent(modelData)
                        property real pressX: 0
                        property bool dragged: false
                        onPressed: function(mouse) { pressX = mouse.x; dragged = false }
                        onPositionChanged: function(mouse) {
                            if (pressed && root.canAdjustEvent(modelData))
                                dragged = Math.abs(mouse.x - pressX) > 5
                        }
                        onReleased: function(mouse) {
                            if (!dragged || !root.canAdjustEvent(modelData)) return
                            let days = Math.round((mouse.x - pressX) / (dayHeader.width / 7))
                            if (days) root.eventAdjusted(modelData, 0, days, 0)
                            dragged = false
                        }
                        onClicked: {
                            root.eventCursor = root.eventIndex(modelData)
                            root.cursorDay = column
                            root.forceActiveFocus()
                            root.eventSelected(modelData)
                        }
                    }
                }
            }
        }

        Rectangle {
            height: 1
            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.09)
            anchors { left: parent.left; right: parent.right; top: dayHeader.bottom }
        }

        Flickable {
            id: schedule
            anchors { left: parent.left; right: parent.right; top: dayHeader.bottom; bottom: parent.bottom }
            contentHeight: (root.endHour - root.startHour) * root.hourHeight + 28
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentY: Math.max(0, (new Date().getHours() - root.startHour - 1) * root.hourHeight)

            Item {
                width: schedule.width
                height: schedule.contentHeight

                Repeater {
                    model: root.endHour - root.startHour + 1
                    delegate: Item {
                        required property int index
                        y: index * root.hourHeight
                        width: parent.width
                        height: 1
                        Text {
                            width: 48
                            y: -7
                            text: Qt.formatTime(new Date(2026, 0, 1, root.startHour + index), "h AP")
                            color: theme.foregroundMuted
                            opacity: 0.72
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: Math.max(9, theme.baseFontSize - 3)
                        }
                        Rectangle {
                            x: 58; width: parent.width - 58; height: 1
                            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.075)
                        }
                    }
                }

                Row {
                    anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; leftMargin: 58 }
                    Repeater {
                        model: 7
                        delegate: Item {
                            required property int index
                            width: parent.width / 7
                            height: parent.height
                            Rectangle {
                                visible: index > 0
                                width: 1; height: parent.height
                                color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.065)
                            }
                        }
                    }
                }

                Item {
                    id: eventLayer
                    x: 58
                    width: parent.width - 58
                    height: parent.height
                    property real columnWidth: width / 7

                    Repeater {
                        model: root.weekEvents.filter(function(item) { return !item.allDay && root.calendarVisible(item) })
                        delegate: EventCard {
                            required property var modelData
                            required property int index
                            property date startDate: root.segmentStart(modelData)
                            property date endDate: root.segmentEnd(modelData)
                            property int column: root.dayIndex(modelData.dateKey)
                            property int lane: modelData.layoutColumn || 0
                            property int laneCount: modelData.layoutColumnCount || 1
                            property real laneWidth: eventLayer.columnWidth / laneCount
                            x: column * eventLayer.columnWidth + lane * laneWidth + 4 + previewDeltaX
                            width: laneWidth - 7
                            y: root.eventY(startDate) + previewDeltaY
                            height: root.eventHeight(startDate, endDate) + previewHeightDelta
                            visible: column >= 0 && column < 7 && startDate.getHours() < root.endHour && endDate.getHours() >= root.startHour
                            title: modelData.title.length ? modelData.title : "Untitled event"
                            time: root.eventTime(modelData)
                            detail: modelData.location
                            eventColor: modelData.color.length ? modelData.color : theme.accent
                            eventData: modelData
                            emphasized: root.activeFocus && root.eventIndex(modelData) === root.eventCursor
                            interactive: root.canAdjustEvent(modelData)
                            onActivated: function(data) {
                                root.eventCursor = root.eventIndex(data)
                                root.cursorDay = column
                                root.forceActiveFocus()
                                root.eventSelected(data)
                            }
                            onMoved: function(data, deltaX, deltaY) {
                                let days = Math.round(deltaX / eventLayer.columnWidth)
                                let minutes = root.snapMinutes(deltaY)
                                if (days || minutes) root.eventAdjusted(data, minutes, days, 0)
                            }
                            onResized: function(data, deltaY) {
                                let minutes = root.snapMinutes(deltaY)
                                if (minutes) root.eventAdjusted(data, 0, 0, minutes)
                            }
                        }
                    }

                    Rectangle {
                        property date now: new Date()
                        property int todayColumn: root.dayIndex(Qt.formatDate(now, "yyyy-MM-dd"))
                        x: todayColumn * eventLayer.columnWidth
                        y: root.eventY(now)
                        width: eventLayer.columnWidth
                        height: 1
                        visible: todayColumn >= 0 && todayColumn < 7
                        color: theme.red
                        Rectangle { x: -3; y: -3; width: 7; height: 7; radius: 4; color: theme.red }
                    }
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 250; height: 74; radius: 12
            visible: root.weekEvents.filter(function(item) { return root.calendarVisible(item) }).length === 0
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.12)
            Column {
                anchors.centerIn: parent
                spacing: 4
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "No events this week"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 1; font.weight: Font.DemiBold }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "A clear week is a beautiful thing."; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize - 1 }
            }
        }
    }
}
