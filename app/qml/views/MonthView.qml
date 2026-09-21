import QtQuick

Item {
    id: root
    property var monthDate: new Date()
    property var monthEvents: []
    property var hiddenCalendarIds: []
    property var gridStart: startOfGrid(monthDate)
    property int cursorIndex: initialCursorIndex()
    property var selectedEvent: ({})
    property var canAdjustEvent: function(eventData) { return false }
    signal eventSelected(var eventData)
    signal dayActivated(var dayDate)
    signal eventAdjusted(var eventData, int minuteDelta, int dayDelta, int resizeDelta)

    function addDays(date, amount) {
        let result = new Date(date)
        result.setDate(result.getDate() + amount)
        return result
    }

    function startOfGrid(date) {
        let first = new Date(date.getFullYear(), date.getMonth(), 1, 12)
        let offset = (first.getDay() + 6) % 7
        first.setDate(first.getDate() - offset)
        return first
    }

    function refreshEvents() {
        let first = startOfGrid(monthDate)
        let last = addDays(first, 41)
        monthEvents = eventStore.eventsForRange(
                    Qt.formatDate(first, "yyyy-MM-dd"),
                    Qt.formatDate(last, "yyyy-MM-dd"))
    }

    function calendarVisible(eventData) {
        return hiddenCalendarIds.indexOf(eventData.calendarId) < 0
    }

    function eventsForDate(dayDate) {
        let key = Qt.formatDate(dayDate, "yyyy-MM-dd")
        return monthEvents.filter(function(item) {
            return item.dateKey === key && root.calendarVisible(item)
        })
    }

    function initialCursorIndex() {
        let today = new Date()
        let target = today.getFullYear() === monthDate.getFullYear()
                && today.getMonth() === monthDate.getMonth()
                ? today : new Date(monthDate.getFullYear(), monthDate.getMonth(), 1, 12)
        return Math.max(0, Math.min(41, Math.round((target.getTime() - startOfGrid(monthDate).getTime()) / 86400000)))
    }

    function moveCursor(amount) {
        cursorIndex = Math.max(0, Math.min(41, cursorIndex + amount))
    }

    Component.onCompleted: refreshEvents()
    onMonthDateChanged: {
        cursorIndex = initialCursorIndex()
        refreshEvents()
    }
    onVisibleChanged: if (visible) forceActiveFocus()
    Keys.onLeftPressed: function(event) {
        if ((event.modifiers & Qt.AltModifier) && canAdjustEvent(selectedEvent))
            eventAdjusted(selectedEvent, 0, -1, 0)
        else moveCursor(-1)
    }
    Keys.onRightPressed: function(event) {
        if ((event.modifiers & Qt.AltModifier) && canAdjustEvent(selectedEvent))
            eventAdjusted(selectedEvent, 0, 1, 0)
        else moveCursor(1)
    }
    Keys.onUpPressed: function(event) {
        if ((event.modifiers & Qt.AltModifier) && canAdjustEvent(selectedEvent))
            eventAdjusted(selectedEvent, 0, -7, 0)
        else moveCursor(-7)
    }
    Keys.onDownPressed: function(event) {
        if ((event.modifiers & Qt.AltModifier) && canAdjustEvent(selectedEvent))
            eventAdjusted(selectedEvent, 0, 7, 0)
        else moveCursor(7)
    }
    Keys.onReturnPressed: dayActivated(addDays(gridStart, cursorIndex))
    Keys.onEnterPressed: dayActivated(addDays(gridStart, cursorIndex))
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_J) {
            moveCursor(7)
            event.accepted = true
        } else if (event.key === Qt.Key_K) {
            moveCursor(-7)
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
            id: weekdayHeader
            height: 42
            anchors { left: parent.left; right: parent.right; top: parent.top }
            Repeater {
                model: ["MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"]
                delegate: Item {
                    required property string modelData
                    width: weekdayHeader.width / 7
                    height: weekdayHeader.height
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: theme.foregroundMuted
                        font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.0
                    }
                }
            }
        }

        Grid {
            id: monthGrid
            anchors { left: parent.left; right: parent.right; top: weekdayHeader.bottom; bottom: parent.bottom }
            columns: 7
            rows: 6

            Repeater {
                model: 42
                delegate: Rectangle {
                    id: dayCell
                    required property int index
                    property var dayDate: root.addDays(root.gridStart, index)
                    property var dayEvents: root.eventsForDate(dayDate)
                    property bool inMonth: dayDate.getMonth() === root.monthDate.getMonth()
                    property bool isToday: Qt.formatDate(dayDate, "yyyy-MM-dd") === Qt.formatDate(new Date(), "yyyy-MM-dd")
                    width: monthGrid.width / 7
                    height: monthGrid.height / 6
                    color: cellPointer.containsMouse
                           ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.045)
                           : (inMonth ? theme.background : Qt.rgba(theme.backgroundDeep.r, theme.backgroundDeep.g, theme.backgroundDeep.b, 0.55))
                    border.width: 1
                    border.color: index === root.cursorIndex && root.activeFocus
                                  ? theme.accent
                                  : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.075)

                    MouseArea {
                        id: cellPointer
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.cursorIndex = dayCell.index
                            root.forceActiveFocus()
                        }
                        onDoubleClicked: root.dayActivated(dayCell.dayDate)
                    }

                    Rectangle {
                        width: 26; height: 26; radius: 13
                        x: 8; y: 7
                        color: dayCell.isToday ? theme.accent : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: dayCell.dayDate.getDate()
                            color: dayCell.isToday ? theme.backgroundDeep : (dayCell.inMonth ? theme.foreground : theme.foregroundMuted)
                            opacity: dayCell.inMonth || dayCell.isToday ? 1 : 0.55
                            font.pixelSize: theme.baseFontSize
                            font.weight: dayCell.isToday ? Font.Bold : Font.Medium
                        }
                    }

                    Column {
                        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 7; rightMargin: 7; topMargin: 38 }
                        spacing: 3
                        Repeater {
                            model: dayCell.dayEvents.slice(0, 3)
                            delegate: Rectangle {
                                required property var modelData
                                width: parent.width
                                height: 20
                                radius: 5
                                color: "transparent"
                                Rectangle {
                                    anchors.fill: parent
                                    radius: parent.radius
                                    color: modelData.color
                                    opacity: 0.20
                                }
                                Rectangle { width: 3; radius: 2; color: modelData.color; anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 4 } }
                                Text {
                                    anchors { fill: parent; leftMargin: 11; rightMargin: 4 }
                                    text: modelData.title.length ? modelData.title : "Untitled event"
                                    color: theme.foreground
                                    font.pixelSize: Math.max(9, theme.baseFontSize - 3)
                                    font.weight: Font.DemiBold
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    preventStealing: pressed && root.canAdjustEvent(modelData)
                                    property point pressInGrid: Qt.point(0, 0)
                                    property bool dragged: false
                                    onPressed: function(mouse) {
                                        pressInGrid = mapToItem(monthGrid, mouse.x, mouse.y)
                                        dragged = false
                                    }
                                    onPositionChanged: function(mouse) {
                                        if (!pressed || !root.canAdjustEvent(modelData)) return
                                        let point = mapToItem(monthGrid, mouse.x, mouse.y)
                                        dragged = Math.abs(point.x - pressInGrid.x) > 5
                                               || Math.abs(point.y - pressInGrid.y) > 5
                                    }
                                    onClicked: {
                                        root.cursorIndex = dayCell.index
                                        root.selectedEvent = modelData
                                        root.forceActiveFocus()
                                        root.eventSelected(modelData)
                                    }
                                    onReleased: function(mouse) {
                                        if (!dragged || !root.canAdjustEvent(modelData)) return
                                        let point = mapToItem(monthGrid, mouse.x, mouse.y)
                                        let column = Math.max(0, Math.min(6, Math.floor(point.x / (monthGrid.width / 7))))
                                        let row = Math.max(0, Math.min(5, Math.floor(point.y / (monthGrid.height / 6))))
                                        let target = row * 7 + column
                                        let days = target - dayCell.index
                                        if (days) root.eventAdjusted(modelData, 0, days, 0)
                                        dragged = false
                                    }
                                    onCanceled: dragged = false
                                }
                            }
                        }
                        Text {
                            visible: dayCell.dayEvents.length > 3
                            text: "+" + (dayCell.dayEvents.length - 3) + " more"
                            color: theme.foregroundMuted
                            font.pixelSize: Math.max(9, theme.baseFontSize - 3)
                            font.weight: Font.DemiBold
                            leftPadding: 5
                        }
                    }
                }
            }
        }
    }
}
