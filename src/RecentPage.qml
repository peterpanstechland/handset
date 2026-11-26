import QtQuick
import QtQuick.Layouts

Item {
    id: root
    signal placeCall(string number)

    function recentLabel(row) {
        var incoming = row.direction === "incoming"
        var direction = incoming ? qsTr("Incoming") : qsTr("Outgoing")
        var outcome = ""
        if (row.state === "terminated")
            outcome = incoming && !row.answered ? qsTr("Missed") : qsTr("Ended")
        else if (incoming && row.state === "ringing")
            outcome = qsTr("Ringing")
        else if (row.state === "dialing")
            outcome = qsTr("Calling")
        else if (row.state === "ringing")
            outcome = qsTr("Ringing")
        else if (row.state === "active")
            outcome = qsTr("On the call")
        else if (row.state === "held" || row.state === "waiting")
            outcome = qsTr("On hold")
        return direction + " · " + outcome
    }

    function whenLabel(iso) {
        var unused = translator.language
        var when = new Date(iso)
        if (isNaN(when.getTime()))
            return ""
        return when.toLocaleString(Qt.locale(), Locale.ShortFormat)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Text {
            visible: backend.recents.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: "#8a8a92"
            text: qsTr("No calls yet. A call you make or receive shows up here.")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: backend.recents

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
                            text: {
                                var named = backend.nameFor(modelData.number)
                                if (named !== "") return named
                                if (modelData.number !== "") return modelData.number
                                return qsTr("Unknown")
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: modelData.direction === "incoming" && modelData.state === "terminated" && !modelData.answered
                                   ? "#e06a62" : "#b8b8c0"
                            font.pixelSize: 13
                            text: root.whenLabel(modelData.at) === ""
                                  ? root.recentLabel(modelData)
                                  : root.recentLabel(modelData) + " · " + root.whenLabel(modelData.at)
                        }
                    }

                    KeyButton {
                        label: qsTr("Call")
                        visible: modelData.number !== ""
                        enabled: !backend.busy && !backend.hasLiveCall
                        accent: true
                        implicitWidth: 88
                        onActivated: root.placeCall(modelData.number)
                    }
                }
            }
        }
    }
}
