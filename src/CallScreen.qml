import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 16
    property string stateLabel: ""

    Item { Layout.fillHeight: true }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: root.width - 24
        textFormat: Text.PlainText
        elide: Text.ElideRight
        color: "white"
        font.pixelSize: 32
        font.bold: true
        text: {
            if (backend.liveName !== "") return backend.liveName
            if (backend.liveNumber !== "") return backend.liveNumber
            return qsTr("Unknown")
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        visible: backend.liveName !== "" && backend.liveNumber !== ""
        textFormat: Text.PlainText
        color: "#b8b8c0"
        font.pixelSize: 16
        text: backend.liveNumber
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        textFormat: Text.PlainText
        color: backend.liveIncoming && backend.liveState === "ringing" ? "#e06a62" : "#d0d0d6"
        font.pixelSize: 16
        font.bold: true
        text: root.stateLabel
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 12

        KeyButton {
            visible: backend.liveIncoming && backend.liveState === "ringing"
            label: qsTr("Answer")
            enabled: !backend.busy
            accent: true
            implicitWidth: 140
            implicitHeight: 56
            onActivated: backend.answer()
        }

        KeyButton {
            label: qsTr("Hang up")
            enabled: !backend.busy
            danger: true
            implicitWidth: 140
            implicitHeight: 56
            onActivated: backend.hangup()
        }
    }

    Item { Layout.fillHeight: true }
}
