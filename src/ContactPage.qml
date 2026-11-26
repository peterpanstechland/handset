import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal placeCall(string number)
    property bool contactInvalid: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Text {
            textFormat: Text.PlainText
            color: "#b8b8c0"
            font.pixelSize: 13
            text: qsTr("%n contacts", "", backend.contacts.length)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: nameField
                placeholderText: qsTr("Name")
                Layout.fillWidth: true
                color: "white"
                onAccepted: numberField.forceActiveFocus()
            }

            TextField {
                id: numberField
                placeholderText: qsTr("Number")
                Layout.fillWidth: true
                color: "white"
                inputMethodHints: Qt.ImhDialableCharactersOnly
                validator: RegularExpressionValidator { regularExpression: /^\+?[0-9*#\s-]{0,24}$/ }
                onAccepted: root.add()
            }

            KeyButton {
                label: qsTr("Add")
                accent: true
                implicitWidth: 96
                onActivated: root.add()
            }
        }

        Text {
            visible: root.contactInvalid
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: "#e06a62"
            font.pixelSize: 13
            text: qsTr("Use a name and a dialable number.")
        }

        Text {
            visible: backend.contacts.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: "#8a8a92"
            text: qsTr("No contacts yet.")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: backend.contacts

            delegate: Item {
                required property var modelData
                width: ListView.view.width
                height: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "white"
                            font.pixelSize: 16
                            text: modelData.name
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#b8b8c0"
                            font.pixelSize: 13
                            text: modelData.number
                        }
                    }

                    KeyButton {
                        label: qsTr("Call")
                        enabled: !backend.busy && !backend.hasLiveCall
                        accent: true
                        implicitWidth: 88
                        onActivated: root.placeCall(modelData.number)
                    }

                    KeyButton {
                        label: qsTr("Remove")
                        implicitWidth: 96
                        onActivated: backend.removeContact(modelData.id)
                    }
                }
            }
        }
    }

    function add() {
        if (backend.addContact(nameField.text, numberField.text)) {
            nameField.text = ""
            numberField.text = ""
            contactInvalid = false
        } else {
            contactInvalid = true
        }
    }
}
