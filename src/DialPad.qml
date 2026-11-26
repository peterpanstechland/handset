import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string stateLabel: ""

    CallScreen {
        anchors.fill: parent
        visible: backend.hasLiveCall
        stateLabel: root.stateLabel
    }

    ColumnLayout {
        anchors.fill: parent
        visible: !backend.hasLiveCall
        spacing: 14

        Item { Layout.fillHeight: true; Layout.maximumHeight: 24 }

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: parent.width - 24
            horizontalAlignment: Text.AlignHCenter
            textFormat: Text.PlainText
            elide: Text.ElideRight
            font.pixelSize: 32
            font.bold: true
            color: backend.number === "" ? "#8a8a92" : "white"
            text: backend.number === "" ? qsTr("Number") : backend.number
        }

        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 3
            columnSpacing: 8
            rowSpacing: 8

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]

                KeyButton {
                    label: modelData
                    Layout.preferredWidth: 88
                    Layout.preferredHeight: 52
                    onActivated: backend.appendDigit(modelData)
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            KeyButton {
                label: "+"
                enabled: backend.number === ""
                Layout.preferredWidth: 88
                Layout.preferredHeight: 52
                onActivated: backend.appendDigit("+")
            }

            KeyButton {
                label: qsTr("Del")
                Layout.preferredWidth: 88
                Layout.preferredHeight: 52
                onActivated: backend.backspace()
            }

            KeyButton {
                label: backend.busy ? qsTr("Calling") : qsTr("Call")
                enabled: !backend.busy && backend.number.length >= 2
                accent: true
                Layout.preferredWidth: 88
                Layout.preferredHeight: 52
                onActivated: backend.callNumber(backend.number)
            }
        }

        Item { Layout.fillHeight: true }
    }
}
