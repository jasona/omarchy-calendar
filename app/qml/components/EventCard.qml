import QtQuick

Rectangle {
    id: card
    signal activated(var eventData)
    signal moved(var eventData, real deltaX, real deltaY)
    signal resized(var eventData, real deltaY)
    required property string title
    required property string time
    property var eventData: ({})
    property string detail: ""
    property color eventColor: theme.accent
    property bool emphasized: false
    property bool interactive: false
    property real previewDeltaX: 0
    property real previewDeltaY: 0
    property real previewHeightDelta: 0
    property bool manipulating: pointer.dragged || resizePointer.pressed

    radius: 8
    color: Qt.rgba(eventColor.r, eventColor.g, eventColor.b, emphasized ? 0.31 : 0.20)
    border.width: emphasized ? 1 : 0
    border.color: Qt.rgba(eventColor.r, eventColor.g, eventColor.b, 0.72)
    clip: true
    z: manipulating ? 20 : 1

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: card.interactive && pressed
        cursorShape: card.interactive ? Qt.OpenHandCursor : Qt.PointingHandCursor
        property real pressParentX: 0
        property real pressParentY: 0
        property bool dragged: false
        onPressed: function(mouse) {
            pressParentX = card.x + mouse.x
            pressParentY = card.y + mouse.y
            dragged = false
        }
        onPositionChanged: function(mouse) {
            if (!pressed || !card.interactive) return
            let nextX = card.x + mouse.x - pressParentX
            let nextY = card.y + mouse.y - pressParentY
            if (Math.abs(nextX) > 3 || Math.abs(nextY) > 3) dragged = true
            card.previewDeltaX = nextX
            card.previewDeltaY = nextY
        }
        onReleased: {
            let dx = card.previewDeltaX
            let dy = card.previewDeltaY
            card.previewDeltaX = 0
            card.previewDeltaY = 0
            if (dragged && card.interactive) card.moved(card.eventData, dx, dy)
            else card.activated(card.eventData)
            dragged = false
        }
        onCanceled: {
            card.previewDeltaX = 0
            card.previewDeltaY = 0
            dragged = false
        }
    }

    scale: pointer.pressed ? 0.985 : 1
    Behavior on scale { NumberAnimation { duration: 80 } }

    Rectangle {
        width: 3
        radius: 2
        color: card.eventColor
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 5 }
    }

    Column {
        anchors { fill: parent; leftMargin: 13; rightMargin: 7; topMargin: 7; bottomMargin: 5 }
        spacing: 2

        Text {
            width: parent.width
            text: card.title
            color: theme.foreground
            font.pixelSize: Math.max(11, theme.baseFontSize - 1)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: card.time
            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.72)
            font.pixelSize: Math.max(10, theme.baseFontSize - 2)
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: card.detail.length > 0 && card.height > 64
            text: card.detail
            color: theme.foregroundMuted
            font.pixelSize: Math.max(10, theme.baseFontSize - 2)
            elide: Text.ElideRight
        }
    }

    Rectangle {
        visible: card.interactive
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 9
        color: "transparent"
        z: 3
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(28, parent.width - 12)
            height: 2
            radius: 1
            color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.55)
        }
        MouseArea {
            id: resizePointer
            anchors.fill: parent
            cursorShape: Qt.SizeVerCursor
            preventStealing: pressed
            property real pressBottom: 0
            onPressed: function(mouse) {
                pressBottom = card.y + parent.y + mouse.y
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                let current = card.y + parent.y + mouse.y
                card.previewHeightDelta = Math.max(38 - card.height + card.previewHeightDelta,
                                                   current - pressBottom)
            }
            onReleased: {
                let delta = card.previewHeightDelta
                card.previewHeightDelta = 0
                if (Math.abs(delta) > 3) card.resized(card.eventData, delta)
            }
            onCanceled: card.previewHeightDelta = 0
        }
    }
}
