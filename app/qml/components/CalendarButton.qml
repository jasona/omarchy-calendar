import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool selected: false
    property color accentColor: theme.accent

    implicitHeight: 36
    padding: 0
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        color: control.selected ? theme.foreground : theme.foregroundMuted
        font.pixelSize: theme.baseFontSize
        font.weight: control.selected ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 9
        color: control.down
               ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.24)
               : control.selected
                 ? Qt.rgba(control.accentColor.r, control.accentColor.g, control.accentColor.b, 0.17)
                 : control.hovered
                   ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.07)
                   : "transparent"
        border.width: control.activeFocus ? 1 : 0
        border.color: theme.accent

        Behavior on color { ColorAnimation { duration: 110 } }
    }
}
