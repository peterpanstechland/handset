import QtQuick
import Quickshell
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "Model.js" as Model

Panel {
  id: root
  moduleName: "handset.cellular"
  ipcTarget: "handset.cellular"
  manageIpc: false

  readonly property string connectionName: Model.connectionName(setting("connection", "4g"))
  readonly property color foreground: bar ? bar.foreground : Commons.Color.foreground
  readonly property string fontFamily: bar ? bar.fontFamily : Style.font.family
  readonly property bool headerHasCursor: cursorActive
  property bool cursorActive: false

  readonly property string toggleHint: cell.dataOn ? strings.t("cellular.turnOff") : strings.t("cellular.turnOn")
  readonly property string heroMeta: {
    if (connectionName === "") return strings.t("cellular.invalidName")
    if (cell.busy) return cell.turningOn ? strings.t("cellular.turningOn") : strings.t("cellular.turningOff")
    if (cell.profileMissing) return strings.t("cellular.noProfile")
    if (!cell.modemPresent) return strings.t("cellular.noModem")
    if (!cell.dataOn) {
      if (cell.operatorName !== "") return strings.t("cellular.operatorOff").arg(cell.operatorName)
      return strings.t("cellular.off")
    }
    if (cell.operatorName !== "" && cell.signal >= 0)
      return strings.t("cellular.operatorSignal").arg(cell.operatorName).arg(Commons.I18n.formatNumber(cell.signal))
    if (cell.operatorName !== "") return cell.operatorName
    return strings.t("cellular.on")
  }
  readonly property string errorText: cell.lastError === "" ? "" : strings.t("cellular.failed")
  readonly property string callStateLabel: {
    var call = cell.liveCall
    if (!call) return ""
    if (call.incoming && call.state === "ringing") return strings.t("cellular.callRinging")
    if (call.state === "dialing") return strings.t("cellular.callDialing")
    if (call.state === "ringing") return strings.t("cellular.callOutgoing")
    if (call.state === "active") return strings.t("cellular.callActive")
    if (call.state === "held" || call.state === "waiting") return strings.t("cellular.callHeld")
    return ""
  }
  readonly property string callParty: {
    var call = cell.liveCall
    if (!call) return ""
    return call.number !== "" ? call.number : strings.t("cellular.unknownSender")
  }

  function toggleData() {
    cell.toggleData()
  }

  function placeCall(number) {
    cell.dial(number)
  }

  Commons.Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  Service {
    id: cell
    connectionName: root.connectionName
  }

  onOpenedChanged: if (opened) {
    cursorActive = false
    cell.refresh()
    cell.refreshSms()
    cell.refreshCalls()
    Qt.callLater(function() { if (panel.loadedContent) panel.loadedContent.forceActiveFocus() })
  }

  Connections {
    target: cell
    function onMessagesArrived(arrived) {
      var count = Math.min(arrived.length, 3)
      for (var i = 0; i < count; i++) {
        var item = arrived[i]
        var who = item.from !== "" ? item.from : strings.t("cellular.unknownSender")
        Util.execArgv([
          "omarchy-notification-send",
          "--app-name", "Mobile data",
          "-u", "normal",
          "-t", "20000",
          strings.t("cellular.smsTitle"),
          Model.notificationBody(who, item.text)
        ])
      }
    }
    function onCallArrived(call) {
      root.open()
      Util.execArgv(["handset"])
      var who = call.number !== "" ? call.number : strings.t("cellular.unknownSender")
      Util.execArgv([
        "omarchy-notification-send",
        "--app-name", "Mobile data",
        "-u", "normal",
        "-t", "60000",
        strings.t("cellular.callIncomingTitle"),
        Model.notificationBody(who, "")
      ])
    }
  }

  ShellIpc {
    target: root.ipcTarget
    function open() { root.open(); return "ok" }
    function close() { root.close(); return "ok" }
    function show() { root.open(); return "ok" }
    function hide() { root.close(); return "ok" }
    function toggle() { root.toggle(); return "ok" }
    function toggleData() { root.toggleData(); return "ok" }
  }

  // The slot uses this item's implicit size. A child button does not
  // contribute unless it fills the item, so pin the slot to one icon.
  implicitWidth: Style.bar.iconSlot
  implicitHeight: bar && bar.barSize > 0 ? bar.barSize : Style.bar.sizeHorizontal

  // Cellular icon-font glyphs draw above the icon row and the bar clips
  // them. "4G" is ordinary text, centered in the slot like the clock.
  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "4G"
    fontSize: Style.bar.iconFont
    horizontalMargin: 2
    foreground: cell.dataOn ? root.barForeground : Qt.darker(root.barForeground, 1.45)
    tooltipText: root.toggleHint
    onPressed: function(b) {
      if (b === Qt.RightButton) root.toggleData()
      else root.toggle()
    }
  }

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: panel.loadedContent
    contentWidth: panel.fittedContentWidth(Style.space(340))
    contentHeight: panel.fittedContentHeight(panel.loadedContentHeight)

    content: PanelKeyCatcher {
      id: keyCatcher
      implicitHeight: column.implicitHeight
      anchors.fill: parent
      blocked: numberField.activeFocus
      onMoveRequested: function(dx, dy) { root.cursorActive = true }
      onActivateRequested: root.toggleData()
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }
      onTextKey: function(t) {
        if (t === "d" || t === "D") root.toggleData()
      }

      Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Style.space(8)

        Item {
          width: parent.width
          implicitHeight: Math.max(heroIcon.implicitHeight, heroLabels.implicitHeight, powerSwitch.implicitHeight)

          Text {
            id: heroIcon
            textFormat: Text.PlainText
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: cell.icon
            color: root.foreground
            font.family: root.fontFamily
            font.pixelSize: Style.font.display
            opacity: cell.dataOn ? 1.0 : 0.5
          }

          ToggleSwitch {
            id: powerSwitch
            checked: cell.dataOn
            busy: cell.busy
            enabled: root.connectionName !== ""
            hasCursor: root.headerHasCursor
            foreground: root.foreground
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            onHovered: function(on) { if (on) root.cursorActive = true }
            onToggled: root.toggleData()

            PanelToolTip {
              visible: powerSwitch.containsMouse
              text: root.toggleHint
              fontFamily: root.fontFamily
            }
          }

          Column {
            id: heroLabels
            anchors.left: heroIcon.right
            anchors.leftMargin: Style.space(14)
            anchors.right: parent.right
            anchors.rightMargin: powerSwitch.width + Style.space(12)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.space(2)

            Text {
              textFormat: Text.PlainText
              text: strings.t("cellular.title")
              color: root.foreground
              font.family: root.fontFamily
              font.pixelSize: Style.font.title
              font.bold: true
              elide: Text.ElideRight
              width: parent.width
            }

            Text {
              textFormat: Text.PlainText
              text: root.heroMeta
              color: Qt.darker(root.foreground, 1.4)
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
              font.bold: true
              font.letterSpacing: 1.1
              elide: Text.ElideRight
              width: parent.width
            }
          }
        }

        Text {
          visible: root.errorText !== ""
          textFormat: Text.PlainText
          text: root.errorText
          color: root.bar ? root.bar.urgent : Commons.Color.urgent
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.WordWrap
          width: parent.width
        }

        PanelSeparator { foreground: root.foreground }

        Text {
          textFormat: Text.PlainText
          text: strings.t("cellular.callSection")
          color: Qt.darker(root.foreground, 1.25)
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          font.bold: true
          font.letterSpacing: 1.1
          width: parent.width
        }

        Button {
          text: strings.t("cellular.phone")
          bordered: true
          foreground: root.foreground
          fontFamily: root.fontFamily
          onClicked: Util.execArgv(["handset"])
        }

        Column {
          visible: cell.liveCall !== null
          width: parent.width
          spacing: Style.space(6)

          Text {
            textFormat: Text.PlainText
            text: root.callParty
            color: root.foreground
            font.family: root.fontFamily
            font.pixelSize: Style.font.title
            font.bold: true
            elide: Text.ElideRight
            width: parent.width
          }

          Text {
            textFormat: Text.PlainText
            text: root.callStateLabel
            color: cell.liveCall && cell.liveCall.incoming && cell.liveCall.state === "ringing"
              ? (root.bar ? root.bar.urgent : Commons.Color.urgent)
              : Qt.darker(root.foreground, 1.25)
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            font.bold: true
            font.letterSpacing: 1.1
            width: parent.width
          }

          Row {
            spacing: Style.space(8)

            Button {
              visible: cell.liveCall && cell.liveCall.incoming && cell.liveCall.state === "ringing"
              text: strings.t("cellular.answer")
              bordered: true
              enabled: !cell.callBusy
              foreground: root.foreground
              fontFamily: root.fontFamily
              onClicked: cell.acceptCall()
            }

            Button {
              text: strings.t("cellular.hangup")
              bordered: true
              enabled: !cell.callBusy
              foreground: root.foreground
              fontFamily: root.fontFamily
              onClicked: cell.hangupCall()
            }
          }
        }

        Column {
          visible: cell.liveCall === null
          width: parent.width
          spacing: Style.space(8)

          Row {
            width: parent.width
            spacing: Style.space(8)

            TextField {
              id: numberField
              width: Math.max(Style.space(80), parent.width - callButton.implicitWidth - parent.spacing)
              placeholderText: strings.t("cellular.numberHint")
              foreground: root.foreground
              font.family: root.fontFamily
              inputMethodHints: Qt.ImhDialableCharactersOnly
              validator: RegularExpressionValidator { regularExpression: /^\+?[0-9*#]{0,20}$/ }
              onAccepted: root.placeCall(text)
            }

            Button {
              id: callButton
              text: cell.callBusy ? strings.t("cellular.callDialing") : strings.t("cellular.call")
              bordered: true
              enabled: !cell.callBusy && Model.dialNumber(numberField.text) !== ""
              foreground: root.foreground
              fontFamily: root.fontFamily
              onClicked: root.placeCall(numberField.text)
            }
          }
        }

        Text {
          visible: cell.callFailed
          textFormat: Text.PlainText
          text: strings.t("cellular.callFailed")
          color: root.bar ? root.bar.urgent : Commons.Color.urgent
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.WordWrap
          width: parent.width
        }

        Text {
          textFormat: Text.PlainText
          text: strings.t("cellular.headset")
          color: Qt.darker(root.foreground, 1.4)
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.WordWrap
          width: parent.width
        }

        PanelSeparator { foreground: root.foreground }

        Text {
          textFormat: Text.PlainText
          text: strings.t("cellular.texts")
          color: Qt.darker(root.foreground, 1.25)
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          font.bold: true
          font.letterSpacing: 1.1
          width: parent.width
        }

        Text {
          visible: cell.messages.length === 0
          textFormat: Text.PlainText
          text: cell.smsFailed ? strings.t("cellular.smsFailed") : strings.t("cellular.noTexts")
          color: root.foreground
          font.family: root.fontFamily
          font.pixelSize: Style.font.body
          wrapMode: Text.WordWrap
          width: parent.width
        }

        Repeater {
          model: cell.messages
          delegate: Column {
            width: column.width
            spacing: Style.space(2)

            Text {
              textFormat: Text.PlainText
              width: parent.width
              elide: Text.ElideRight
              text: (modelData.from !== "" ? modelData.from : strings.t("cellular.unknownSender"))
                + (modelData.when !== "" ? "  " + modelData.when : "")
              color: Qt.darker(root.foreground, 1.25)
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
              font.bold: true
            }

            Text {
              visible: modelData.text !== ""
              textFormat: Text.PlainText
              width: parent.width
              wrapMode: Text.WordWrap
              maximumLineCount: 4
              elide: Text.ElideRight
              text: modelData.text
              color: root.foreground
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
            }
          }
        }
      }
    }
  }
}
