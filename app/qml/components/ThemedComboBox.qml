import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 42
    leftPadding: 13
    rightPadding: 38
    font.family: "Inter"
    font.pixelSize: theme.baseFontSize

    contentItem: Text {
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        text: control.displayText
        color: theme.foreground
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 13
        y: (control.height - height) / 2 - 1
        text: "⌄"
        color: control.activeFocus ? theme.accent : theme.foregroundMuted
        font.pixelSize: theme.baseFontSize + 4
    }

    background: Rectangle {
        radius: 9
        color: control.activeFocus || control.down
               ? Qt.rgba(theme.surfaceRaised.r, theme.surfaceRaised.g, theme.surfaceRaised.b, 0.94)
               : theme.surface
        border.width: 1
        border.color: control.activeFocus
                      ? theme.accent
                      : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.14)
    }

    delegate: ItemDelegate {
        id: option
        required property var modelData
        required property int index
        width: control.width - 8
        height: 38
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: option.modelData[control.textRole]
            color: theme.foreground
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 7
            color: option.highlighted
                   ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.18)
                   : option.hovered
                     ? Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.07)
                     : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 240)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
        background: Rectangle {
            radius: 10
            color: theme.backgroundDeep
            border.width: 1
            border.color: Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.18)
        }
    }
}
