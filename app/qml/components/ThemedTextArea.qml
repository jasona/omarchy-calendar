import QtQuick
import QtQuick.Controls

TextArea {
    id: control
    implicitHeight: 112
    padding: 13
    color: theme.foreground
    placeholderTextColor: Qt.rgba(theme.foregroundMuted.r, theme.foregroundMuted.g, theme.foregroundMuted.b, 0.78)
    selectionColor: theme.accent
    selectedTextColor: theme.backgroundDeep
    font.family: "Inter"
    font.pixelSize: theme.baseFontSize
    wrapMode: TextEdit.Wrap

    background: Rectangle {
        radius: 9
        color: control.activeFocus
               ? Qt.rgba(theme.surfaceRaised.r, theme.surfaceRaised.g, theme.surfaceRaised.b, 0.94)
               : theme.surface
        border.width: 1
        border.color: control.activeFocus
                      ? theme.accent
                      : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.14)
        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }
}
