import QtQuick
import QtQuick.Controls

Item {
    id: root
    Accessible.role: Accessible.Pane
    Accessible.name: "Calendar search"
    property var hiddenCalendarIds: []
    property var results: []
    property var visibleResults: results.filter(function(item) {
        return root.hiddenCalendarIds.indexOf(item.calendarId) < 0
    })
    signal eventSelected(var eventData)

    function plainTerm() {
        return query.text.replace(/\b(calendar|after|before|organizer|response):(?:"[^"]+"|\S+)/gi, "")
            .trim()
    }

    function escaped(value) {
        return String(value || "").replace(/&/g, "&amp;").replace(/</g, "&lt;")
            .replace(/>/g, "&gt;").replace(/"/g, "&quot;")
    }

    function highlighted(value) {
        let safe = escaped(value)
        let term = plainTerm()
        if (!term.length) return safe
        let pattern = term.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
        return safe.replace(new RegExp("(" + pattern + ")", "ig"),
                            "<span style='color:" + theme.accent + ";font-weight:600'>$1</span>")
    }

    function matchContext(eventData) {
        let term = plainTerm().toLowerCase()
        if (!term.length) return ""
        if ((eventData.description || "").toLowerCase().indexOf(term) >= 0)
            return eventData.description
        let attendees = eventData.attendees || []
        for (let attendee of attendees) {
            let label = attendee.displayName || attendee.email || ""
            if (label.toLowerCase().indexOf(term) >= 0) return label
        }
        return ""
    }

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
            placeholderText: "Search events or use calendar:, after:, organizer:, response:"
            Accessible.name: "Search events"
            Accessible.description: "Search titles, notes, guests, organizers, locations, and calendars"
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
                id: searchResult
                required property var modelData
                required property int index
                width: resultsList.width
                height: root.matchContext(modelData).length ? 88 : 70
                radius: 11
                color: pointer.containsMouse
                       ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.075)
                       : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.035)
                border.width: 1
                border.color: index === resultsList.currentIndex && resultsList.activeFocus
                              ? theme.accent
                              : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.08)
                Accessible.role: Accessible.ListItem
                Accessible.name: (modelData.title || "Untitled event") + ", "
                                 + (modelData.allDay ? "all day" : Qt.formatTime(new Date(modelData.startMs), "h:mm AP"))
                                 + ", " + (modelData.calendarName || "")
                Accessible.onPressAction: root.eventSelected(modelData)

                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                    width: 4; radius: 2; color: modelData.color
                }

                Column {
                    anchors { left: parent.left; leftMargin: 25; right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
                    spacing: 5
                    Text {
                        width: parent.width
                        text: root.highlighted(modelData.title || "Untitled event")
                        textFormat: Text.RichText
                        color: theme.foreground
                        font.pixelSize: theme.baseFontSize + 1
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: Qt.formatDate(modelData.allDay
                                            ? new Date(modelData.allDayStartDate + "T12:00:00")
                                            : new Date(modelData.startMs), "ddd, MMM d, yyyy")
                              + "  ·  " + (modelData.allDay ? "All day" : Qt.formatTime(new Date(modelData.startMs), "h:mm AP"))
                              + "  ·  " + (modelData.location || modelData.calendarName)
                        color: theme.foregroundMuted
                        font.pixelSize: theme.baseFontSize - 1
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        visible: root.matchContext(modelData).length > 0
                        text: root.highlighted(root.matchContext(modelData))
                        textFormat: Text.RichText
                        color: theme.foregroundMuted
                        font.pixelSize: Math.max(10, theme.baseFontSize - 1)
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
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Search titles, notes, guests, organizers, locations, or calendars."; color: theme.foregroundMuted; font.pixelSize: theme.baseFontSize }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Filters: calendar:Work  after:2026-09-01  organizer:alex  response:accepted"; color: theme.foregroundMuted; font.pixelSize: Math.max(10, theme.baseFontSize - 1) }
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
