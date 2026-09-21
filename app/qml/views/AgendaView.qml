import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var anchorDate: new Date()
    property var hiddenCalendarIds: []
    property var events: []
    property var visibleEvents: events.filter(function(item) {
        return root.hiddenCalendarIds.indexOf(item.calendarId) < 0
    })
    signal eventSelected(var eventData)

    function addDays(date, amount) {
        let result = new Date(date)
        result.setDate(result.getDate() + amount)
        return result
    }

    function refresh() {
        events = eventStore.eventsForRange(
                    Qt.formatDate(anchorDate, "yyyy-MM-dd"),
                    Qt.formatDate(addDays(anchorDate, 45), "yyyy-MM-dd"))
    }

    function eventTimeLabel(eventData) {
        if (eventData.allDay)
            return "ALL DAY"
        let startKey = Qt.formatDate(new Date(eventData.startMs), "yyyy-MM-dd")
        if (eventData.dateKey !== startKey)
            return "CONTINUES"
        return Qt.formatTime(new Date(eventData.startMs), "h:mm AP")
    }

    function moveSelection(amount) {
        if (!visibleEvents.length)
            return
        let next = Math.max(0, Math.min(visibleEvents.length - 1, list.currentIndex < 0 ? 0 : list.currentIndex + amount))
        list.currentIndex = next
        list.positionViewAtIndex(next, ListView.Contain)
        eventSelected(visibleEvents[next])
    }

    Component.onCompleted: refresh()
    onAnchorDateChanged: refresh()
    onVisibleEventsChanged: list.currentIndex = visibleEvents.length ? Math.min(Math.max(0, list.currentIndex), visibleEvents.length - 1) : -1
    onVisibleChanged: if (visible) list.forceActiveFocus()
    Connections {
        target: eventStore
        function onEventsChanged() { root.refresh() }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background

        ListView {
            id: list
            anchors { fill: parent; margins: 24 }
            clip: true
            spacing: 4
            boundsBehavior: Flickable.StopAtBounds
            model: root.visibleEvents
            currentIndex: root.visibleEvents.length ? 0 : -1
            keyNavigationEnabled: false
            Keys.onUpPressed: root.moveSelection(-1)
            Keys.onDownPressed: root.moveSelection(1)
            Keys.onReturnPressed: if (currentIndex >= 0) root.eventSelected(root.visibleEvents[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) root.eventSelected(root.visibleEvents[currentIndex])
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_J) {
                    root.moveSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_K) {
                    root.moveSelection(-1)
                    event.accepted = true
                }
            }

            delegate: Item {
                required property var modelData
                required property int index
                property bool startsDay: index === 0 || root.visibleEvents[index - 1].dateKey !== modelData.dateKey
                width: list.width
                height: 76 + (startsDay ? 38 : 0)

                Text {
                    visible: startsDay
                    anchors { left: parent.left; top: parent.top; topMargin: 8 }
                    text: Qt.formatDate(new Date(modelData.dateKey + "T12:00:00"), "dddd, MMMM d").toUpperCase()
                    color: theme.foregroundMuted
                    font.pixelSize: Math.max(10, theme.baseFontSize - 2)
                    font.weight: Font.Bold
                    font.letterSpacing: 1
                }

                Rectangle {
                    id: card
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    height: 70
                    radius: 11
                    color: cardPointer.containsMouse
                           ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.075)
                           : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.035)
                    border.width: 1
                    border.color: index === list.currentIndex && list.activeFocus
                                  ? theme.accent
                                  : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.08)

                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                        width: 4; radius: 2
                        color: modelData.color
                    }

                    Text {
                        anchors { left: parent.left; leftMargin: 24; verticalCenter: parent.verticalCenter }
                        width: 112
                        text: root.eventTimeLabel(modelData)
                        color: theme.foregroundMuted
                        font.pixelSize: theme.baseFontSize - 1
                        font.weight: Font.DemiBold
                    }

                    Column {
                        anchors { left: parent.left; leftMargin: 146; right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                        spacing: 5
                        Text {
                            width: parent.width
                            text: modelData.title || "Untitled event"
                            color: theme.foreground
                            font.pixelSize: theme.baseFontSize + 1
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: modelData.location || modelData.calendarName
                            color: theme.foregroundMuted
                            font.pixelSize: theme.baseFontSize - 1
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: cardPointer
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            list.currentIndex = index
                            list.forceActiveFocus()
                            root.eventSelected(modelData)
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Text {
            anchors.centerIn: parent
            visible: root.visibleEvents.length === 0
            text: "Nothing scheduled in the next 45 days"
            color: theme.foregroundMuted
            font.pixelSize: theme.baseFontSize + 1
        }
    }
}
