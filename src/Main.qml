import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 920
    height: 620
    minimumWidth: 760
    minimumHeight: 500
    visible: true
    title: qsTr("Phone")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property string page: "dial"
    property bool helpVisible: false
    property bool typing: activeFocusItem instanceof TextField || activeFocusItem instanceof TextInput

    function callStateLabel(state, incoming) {
        if (incoming && state === "ringing") return qsTr("Incoming")
        if (state === "dialing") return qsTr("Calling")
        if (state === "ringing") return qsTr("Ringing")
        if (state === "active") return qsTr("On the call")
        if (state === "held" || state === "waiting") return qsTr("On hold")
        if (state === "terminated") return qsTr("Ended")
        return ""
    }

    function party() {
        if (backend.liveName !== "") return backend.liveName
        if (backend.liveNumber !== "") return backend.liveNumber
        return qsTr("Unknown")
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.helpVisible; onActivated: backend.primaryAction() }
    Shortcut { sequence: "Return"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.helpVisible; onActivated: backend.primaryAction() }
    Shortcut { sequence: "H"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.hangup() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.answer() }
    Shortcut { sequence: "Backspace"; context: Qt.ApplicationShortcut; enabled: !win.typing && win.page === "dial" && !backend.hasLiveCall; onActivated: backend.backspace() }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.page = "dial" }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.page = "contacts" }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: win.page = "calls" }

    Repeater {
        model: ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "#", "+"]
        Item {
            required property string modelData
            Shortcut {
                sequence: parent.modelData
                context: Qt.ApplicationShortcut
                enabled: !win.typing && win.page === "dial" && !backend.hasLiveCall && !win.helpVisible
                onActivated: backend.appendDigit(parent.modelData)
            }
        }
    }

    Connections {
        target: backend
        function onRaiseRequested() {
            win.show()
            win.raise()
            win.requestActivate()
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    textFormat: Text.PlainText
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                    text: qsTr("Phone")
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    color: "#8a8a92"
                    font.pixelSize: 13
                    text: qsTr("Sound is the headset plugged into the 4G board.")
                }

                Text {
                    visible: backend.problem !== ""
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    color: "#e06a62"
                    font.pixelSize: 13
                    text: backend.problem === "invalid" ? qsTr("That number cannot be dialed")
                        : backend.problem === "modem" ? qsTr("The modem did not answer")
                        : qsTr("The call did not go through")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                ColumnLayout {
                    Layout.preferredWidth: 160
                    Layout.minimumWidth: 160
                    Layout.maximumWidth: 160
                    Layout.fillHeight: true
                    spacing: 8

                    KeyButton {
                        Layout.fillWidth: true
                        label: qsTr("Dial")
                        accent: win.page === "dial"
                        onActivated: win.page = "dial"
                    }
                    KeyButton {
                        Layout.fillWidth: true
                        label: qsTr("Contacts")
                        accent: win.page === "contacts"
                        onActivated: win.page = "contacts"
                    }
                    KeyButton {
                        Layout.fillWidth: true
                        label: qsTr("Calls")
                        accent: win.page === "calls"
                        onActivated: win.page = "calls"
                    }
                    Item { Layout.fillHeight: true }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    DialPad {
                        anchors.fill: parent
                        visible: win.page === "dial"
                        stateLabel: win.callStateLabel(backend.liveState, backend.liveIncoming)
                    }

                    ContactPage {
                        anchors.fill: parent
                        visible: win.page === "contacts"
                        onPlaceCall: function(number) {
                            backend.callNumber(number)
                            win.page = "dial"
                        }
                    }

                    RecentPage {
                        anchors.fill: parent
                        visible: win.page === "calls"
                        onPlaceCall: function(number) {
                            backend.callNumber(number)
                            win.page = "dial"
                        }
                    }
                }
            }

            Rectangle {
                visible: backend.hasLiveCall && win.page !== "dial"
                Layout.fillWidth: true
                implicitHeight: visible ? banner.implicitHeight + 16 : 0
                radius: Math.min(12, theme.radius)
                color: "#1c1c22"

                RowLayout {
                    id: banner
                    anchors.fill: parent
                    anchors.margins: 8
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
                            font.bold: true
                            text: win.party()
                        }

                        Text {
                            textFormat: Text.PlainText
                            color: "#b8b8c0"
                            font.pixelSize: 13
                            text: win.callStateLabel(backend.liveState, backend.liveIncoming)
                        }
                    }

                    KeyButton {
                        visible: backend.liveIncoming && backend.liveState === "ringing"
                        label: qsTr("Answer")
                        enabled: !backend.busy
                        accent: true
                        implicitWidth: 120
                        onActivated: backend.answer()
                    }

                    KeyButton {
                        label: qsTr("Hang up")
                        enabled: !backend.busy
                        danger: true
                        implicitWidth: 120
                        onActivated: backend.hangup()
                    }
                }
            }
        }

        Rectangle {
            visible: win.helpVisible
            anchors.fill: parent
            color: "#cc000000"

            MouseArea {
                anchors.fill: parent
                onClicked: win.helpVisible = false
            }

            Rectangle {
                width: Math.min(440, parent.width - 48)
                height: helpColumn.implicitHeight + 36
                anchors.centerIn: parent
                color: "#1a1a1e"
                radius: Math.min(12, theme.radius)

                Column {
                    id: helpColumn
                    width: parent.width - 36
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        textFormat: Text.PlainText
                        color: "white"
                        font.pixelSize: 18
                        font.bold: true
                        text: qsTr("Shortcuts")
                    }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("Space calls or answers") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("H hangs up") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("A answers") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("D shows the dial pad") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("C shows contacts") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("L shows calls") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("Backspace deletes a digit") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("Q quits") }
                    Text { textFormat: Text.PlainText; color: "#d0d0d6"; text: qsTr("? shows this help") }
                }
            }
        }
    }
}
