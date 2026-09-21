import QtQuick
import "../components"

Item {
    id: root
    property var selectedDate: new Date()
    property int startHour: 7
    property int endHour: 20
    property real hourHeight: 70
    property var dayEvents: []
    property var hiddenCalendarIds: []
    property var visibleTimedEvents: dayEvents.filter(function(item) { return !item.allDay && root.calendarVisible(item) })
    property int eventCursor: visibleTimedEvents.length ? 0 : -1
    property var canAdjustEvent: function(eventData) { return false }
    signal eventSelected(var eventData)
    signal dateNavigation(int amount)
    signal eventAdjusted(var eventData, int minuteDelta, int dayDelta, int resizeDelta)

    function refreshEvents() {
        let key = Qt.formatDate(selectedDate, "yyyy-MM-dd")
        dayEvents = eventStore.eventsForRange(key, key)
    }

    function calendarVisible(eventData) {
        return hiddenCalendarIds.indexOf(eventData.calendarId) < 0
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
        let start = new Date(eventData.startMs)
        let end = new Date(eventData.endMs)
        return Qt.formatTime(start, "h:mm AP") + " – " + Qt.formatTime(end, "h:mm AP")
    }

    function moveEventCursor(amount) {
        if (!visibleTimedEvents.length)
            return
        eventCursor = Math.max(0, Math.min(visibleTimedEvents.length - 1, eventCursor + amount))
        let eventData = visibleTimedEvents[eventCursor]
        let targetY = eventY(new Date(eventData.startMs)) - hourHeight
        schedule.contentY = Math.max(0, Math.min(schedule.contentHeight - schedule.height, targetY))
        eventSelected(eventData)
    }

    function snapMinutes(pixels) {
        return Math.round((pixels / hourHeight * 60) / 15) * 15
    }

    function adjustSelected(minuteDelta, dayDelta, resizeDelta) {
        if (eventCursor < 0 || eventCursor >= visibleTimedEvents.length) return
        let data = visibleTimedEvents[eventCursor]
        if (canAdjustEvent(data)) eventAdjusted(data, minuteDelta, dayDelta, resizeDelta)
    }

    Component.onCompleted: refreshEvents()
    onSelectedDateChanged: refreshEvents()
    onVisibleTimedEventsChanged: eventCursor = visibleTimedEvents.length ? Math.min(Math.max(0, eventCursor), visibleTimedEvents.length - 1) : -1
    onVisibleChanged: if (visible) forceActiveFocus()
    Keys.onLeftPressed: function(event) {
        if (event.modifiers & Qt.AltModifier) adjustSelected(0, -1, 0)
        else dateNavigation(-1)
    }
    Keys.onRightPressed: function(event) {
        if (event.modifiers & Qt.AltModifier) adjustSelected(0, 1, 0)
        else dateNavigation(1)
    }
    Keys.onUpPressed: function(event) {
        if (event.modifiers & Qt.AltModifier)
            adjustSelected(event.modifiers & Qt.ShiftModifier ? 0 : -15, 0,
                           event.modifiers & Qt.ShiftModifier ? -15 : 0)
        else moveEventCursor(-1)
    }
    Keys.onDownPressed: function(event) {
        if (event.modifiers & Qt.AltModifier)
            adjustSelected(event.modifiers & Qt.ShiftModifier ? 0 : 15, 0,
                           event.modifiers & Qt.ShiftModifier ? 15 : 0)
        else moveEventCursor(1)
    }
    Keys.onReturnPressed: if (eventCursor >= 0) eventSelected(visibleTimedEvents[eventCursor])
    Keys.onEnterPressed: if (eventCursor >= 0) eventSelected(visibleTimedEvents[eventCursor])
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_J) {
            moveEventCursor(1)
            event.accepted = true
        } else if (event.key === Qt.Key_K) {
            moveEventCursor(-1)
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

        Item {
            id: dayHeader
            height: 84
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 58 }

            Column {
                anchors.centerIn: parent
                spacing: 3
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Qt.formatDate(root.selectedDate, "dddd").toUpperCase()
                    color: Qt.formatDate(root.selectedDate, "yyyy-MM-dd") === Qt.formatDate(new Date(), "yyyy-MM-dd") ? theme.accent : theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.1
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Qt.formatDate(root.selectedDate, "MMMM d")
                    color: theme.foreground
                    font.pixelSize: theme.baseFontSize + 5
                    font.weight: Font.DemiBold
                }
            }

            Row {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 10; rightMargin: 10; bottomMargin: 5 }
                spacing: 6
                Repeater {
                    model: root.dayEvents.filter(function(item) { return item.allDay && root.calendarVisible(item) })
                    delegate: Rectangle {
                        required property var modelData
                        width: Math.min(210, Math.max(90, eventTitle.implicitWidth + 20))
                        height: 20
                        radius: 5
                        color: modelData.color
                        opacity: 0.86
                        Text {
                            id: eventTitle
                            anchors { fill: parent; leftMargin: 7; rightMargin: 6 }
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
                            onClicked: root.eventSelected(modelData)
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

                Item {
                    id: eventLayer
                    x: 64
                    width: parent.width - 76
                    height: parent.height

                    Repeater {
                        model: root.visibleTimedEvents
                        delegate: EventCard {
                            required property var modelData
                            required property int index
                            property date startDate: root.segmentStart(modelData)
                            property date endDate: root.segmentEnd(modelData)
                            property int lane: modelData.layoutColumn || 0
                            property int laneCount: modelData.layoutColumnCount || 1
                            property real laneWidth: eventLayer.width / laneCount
                            x: lane * laneWidth + 3 + previewDeltaX
                            width: laneWidth - 6
                            y: root.eventY(startDate) + previewDeltaY
                            height: root.eventHeight(startDate, endDate) + previewHeightDelta
                            visible: startDate.getHours() < root.endHour && endDate.getHours() >= root.startHour
                            title: modelData.title.length ? modelData.title : "Untitled event"
                            time: root.eventTime(modelData)
                            detail: modelData.location
                            eventColor: modelData.color.length ? modelData.color : theme.accent
                            eventData: modelData
                            emphasized: index === root.eventCursor && root.activeFocus
                            interactive: root.canAdjustEvent(modelData)
                            onActivated: function(data) {
                                root.eventCursor = index
                                root.forceActiveFocus()
                                root.eventSelected(data)
                            }
                            onMoved: function(data, deltaX, deltaY) {
                                let minutes = root.snapMinutes(deltaY)
                                if (minutes) root.eventAdjusted(data, minutes, 0, 0)
                            }
                            onResized: function(data, deltaY) {
                                let minutes = root.snapMinutes(deltaY)
                                if (minutes) root.eventAdjusted(data, 0, 0, minutes)
                            }
                        }
                    }

                    Rectangle {
                        property date now: new Date()
                        y: root.eventY(now)
                        width: parent.width
                        height: 1
                        visible: Qt.formatDate(root.selectedDate, "yyyy-MM-dd") === Qt.formatDate(now, "yyyy-MM-dd")
                        color: theme.red
                        Rectangle { x: -3; y: -3; width: 7; height: 7; radius: 4; color: theme.red }
                    }
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 250; height: 74; radius: 12
            visible: root.dayEvents.filter(function(item) { return root.calendarVisible(item) }).length === 0
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.12)
            Column {
                anchors.centerIn: parent
                spacing: 4
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Nothing scheduled"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 1; font.weight: Font.DemiBold }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "This day is yours."; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize - 1 }
            }
        }
    }
}
