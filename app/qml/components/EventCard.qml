import QtQuick

Rectangle {
    id: card
    signal activated(var eventData)
    required property string title
    required property string time
    property var eventData: ({})
    property string detail: ""
    property color eventColor: theme.accent
    property bool emphasized: false

    radius: 8
    color: Qt.rgba(eventColor.r, eventColor.g, eventColor.b, emphasized ? 0.31 : 0.20)
    border.width: emphasized ? 1 : 0
    border.color: Qt.rgba(eventColor.r, eventColor.g, eventColor.b, 0.72)
    clip: true

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.activated(card.eventData)
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
}
