import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var hiddenCalendarIds: []
    property var results: []
    property var visibleResults: results.filter(function(item) {
        return root.hiddenCalendarIds.indexOf(item.calendarId) < 0
    })
    signal eventSelected(var eventData)

    function search() {
        results = query.text.trim().length ? eventStore.searchEvents(query.text, 100) : []
    }

    function moveSelection(amount) {
        if (!visibleResults.length)
            return
        let next = Math.max(0, Math.min(visibleResults.length - 1, resultsList.currentIndex < 0 ? 0 : resultsList.currentIndex + amount))
        resultsList.currentIndex = next
        resultsList.positionViewAtIndex(next, ListView.Contain)
        eventSelected(visibleResults[next])
    }

    Connections {
        target: eventStore
        function onEventsChanged() { root.search() }
    }
    onVisibleChanged: if (visible) query.forceActiveFocus()
    onVisibleResultsChanged: resultsList.currentIndex = visibleResults.length ? 0 : -1

    Timer {
        id: debounce
        interval: 160
        repeat: false
        onTriggered: root.search()
    }

    Rectangle {
        anchors.fill: parent
        color: theme.background

        TextField {
            id: query
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 24 }
            height: 48
            leftPadding: 18
            rightPadding: 18
            placeholderText: "Search titles, locations, and calendars"
            color: theme.foreground
            placeholderTextColor: theme.foregroundMuted
            selectionColor: theme.accent
            selectedTextColor: theme.backgroundDeep
            font.pixelSize: theme.baseFontSize + 2
            onTextChanged: debounce.restart()
            Keys.onDownPressed: {
                if (root.visibleResults.length) {
                    resultsList.currentIndex = Math.max(0, resultsList.currentIndex)
                    resultsList.forceActiveFocus()
                    root.eventSelected(root.visibleResults[resultsList.currentIndex])
                }
            }
            background: Rectangle {
                radius: 12
                color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.05)
                border.width: query.activeFocus ? 1 : 0
                border.color: theme.accent
            }
        }

        ListView {
            id: resultsList
            anchors { left: parent.left; right: parent.right; top: query.bottom; bottom: parent.bottom; margins: 24; topMargin: 18 }
            model: root.visibleResults
            spacing: 7
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: root.visibleResults.length ? 0 : -1
            keyNavigationEnabled: false
            Keys.onUpPressed: {
                if (currentIndex === 0)
                    query.forceActiveFocus()
                else
                    root.moveSelection(-1)
            }
            Keys.onDownPressed: root.moveSelection(1)
            Keys.onReturnPressed: if (currentIndex >= 0) root.eventSelected(root.visibleResults[currentIndex])
            Keys.onEnterPressed: if (currentIndex >= 0) root.eventSelected(root.visibleResults[currentIndex])
            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_J) {
                    root.moveSelection(1)
                    event.accepted = true
                } else if (event.key === Qt.Key_K) {
                    root.moveSelection(-1)
                    event.accepted = true
                }
            }

            delegate: Rectangle {
                required property var modelData
                required property int index
                width: resultsList.width
                height: 70
                radius: 11
                color: pointer.containsMouse
                       ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.075)
                       : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.035)
                border.width: 1
                border.color: index === resultsList.currentIndex && resultsList.activeFocus
                              ? theme.accent
                              : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.08)

                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                    width: 4; radius: 2; color: modelData.color
                }

                Column {
                    anchors { left: parent.left; leftMargin: 25; right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
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
                        text: Qt.formatDate(new Date(modelData.startMs), "ddd, MMM d, yyyy")
                              + "  ·  " + (modelData.allDay ? "All day" : Qt.formatTime(new Date(modelData.startMs), "h:mm AP"))
                              + "  ·  " + (modelData.location || modelData.calendarName)
                        color: theme.foregroundMuted
                        font.pixelSize: theme.baseFontSize - 1
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        resultsList.currentIndex = index
                        resultsList.forceActiveFocus()
                        root.eventSelected(modelData)
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }

        Column {
            anchors.centerIn: parent
            spacing: 7
            visible: query.text.length === 0
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Find anything"; color: theme.foreground; font.pixelSize: theme.baseFontSize + 5; font.weight: Font.DemiBold }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Search events by title, location, or calendar."; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
        }

        Text {
            anchors.centerIn: parent
            visible: query.text.length > 0 && root.visibleResults.length === 0
            text: "No matching events"
            color: theme.foregroundMuted
            font.pixelSize: theme.baseFontSize + 1
        }
    }
}
