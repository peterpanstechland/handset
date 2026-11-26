import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string label: ""
    property bool accent: false
    property bool danger: false
    signal activated()

    implicitWidth: 88
    implicitHeight: 48
    radius: Math.min(4, theme.radius)
    color: mouse.pressed && enabled ? Qt.darker(fill, 1.2) : fill

    property color fill: !enabled ? "#24242a" : accent ? theme.accent : danger ? "#c4362c" : "#2c2c34"

    Text {
        anchors.centerIn: parent
        width: parent.width - 12
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
        color: !root.enabled ? "#6e6e76" : root.accent ? theme.accentForeground : "white"
        font.pixelSize: 16
        text: root.label
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        onClicked: root.activated()
    }
}
