import QtQuick
import Quickshell
import Quickshell.Io
import "Model.js" as Model

Item {
  id: root

  property string connectionName: "4g"

  property bool profileMissing: false
  property bool modemPresent: false
  property string operatorName: ""
  property int signal: -1
  property bool liveOn: false
  property string lastError: ""
  property var messages: []
  property bool smsFailed: false
  property var _seenSms: ({})
  property bool _smsSeeded: false
  property var liveCall: null
  property bool callFailed: false
  property string _callPhase: ""
  property string _callId: ""
  property var _notifiedRings: ({})
  readonly property bool callBusy: callProcess.running

  signal messagesArrived(var arrived)
  signal callArrived(var call)

  // -1 follows the modem. 0 or 1 is the position the switch asked for while
  // NetworkManager is still catching up, so the knob moves on the click.
  property int _desired: -1
  property bool _turnOn: false
  property bool _queued: false
  property bool _gaveUp: false
  readonly property bool dataOn: _desired === -1 ? liveOn : _desired === 1
  readonly property bool turningOn: _desired === 1
  readonly property bool busy: modifyProcess.running || actionProcess.running
  readonly property string icon: Model.iconFor(modemPresent, dataOn, signal)

  readonly property string smsScript: {
    var url = Qt.resolvedUrl("sms.py").toString()
    if (url.indexOf("file://") === 0) url = url.slice("file://".length)
    return decodeURIComponent(url)
  }

  readonly property string callsScript: {
    var url = Qt.resolvedUrl("calls.py").toString()
    if (url.indexOf("file://") === 0) url = url.slice("file://".length)
    return decodeURIComponent(url)
  }

  function refresh() {
    if (connectionName === "") {
      profileMissing = true
      return
    }
    if (stateProcess.running || modemProcess.running) {
      _queued = true
      return
    }
    _queued = false
    stateProcess.command = ["nmcli", "-g", "GENERAL.STATE", "connection", "show", connectionName]
    stateProcess.running = true
  }

  function toggleData() {
    if (busy || connectionName === "") return
    setData(!dataOn)
  }

  function setData(on) {
    if (busy || connectionName === "") return
    _desired = on ? 1 : 0
    _turnOn = on
    lastError = ""
    modifyProcess.command = ["nmcli", "connection", "modify", connectionName, "connection.autoconnect", on ? "yes" : "no"]
    modifyProcess.running = true
  }

  function refreshSms() {
    if (smsProcess.running) return
    smsProcess.command = ["python3", smsScript]
    smsProcess.running = true
  }

  function refreshCalls() {
    if (callListProcess.running) return
    callListProcess.command = ["python3", callsScript]
    callListProcess.running = true
  }

  function applyCalls(raw) {
    var report = Model.parseCallReport(raw)
    if (!report.ok) return
    liveCall = report.live
    var ring = report.live && report.live.incoming && report.live.state === "ringing" ? report.live : null
    if (!ring) {
      _notifiedRings = ({})
      return
    }
    if (_notifiedRings[ring.id]) return
    var next = ({})
    next[ring.id] = true
    _notifiedRings = next
    callArrived(ring)
  }

  function dial(number) {
    var safe = Model.dialNumber(number)
    if (safe === "" || callBusy) return false
    callFailed = false
    _callPhase = "create"
    callProcess.command = ["mmcli", "-m", "0", "--voice-create-call=number=" + safe]
    callProcess.running = true
    return true
  }

  function acceptCall() {
    var id = liveCall && liveCall.id ? String(liveCall.id) : ""
    if (!/^[0-9]{1,6}$/.test(id) || callBusy) return
    callFailed = false
    _callPhase = "accept"
    callProcess.command = ["mmcli", "-o", id, "--accept"]
    callProcess.running = true
  }

  function hangupCall() {
    var id = liveCall && liveCall.id ? String(liveCall.id) : ""
    if (!/^[0-9]{1,6}$/.test(id) || callBusy) return
    callFailed = false
    _callPhase = "hangup"
    callProcess.command = ["mmcli", "-o", id, "--hangup"]
    callProcess.running = true
  }

  function finishCallStep(phase, code, text) {
    if (phase === "create") {
      var id = Model.callIdFrom(text)
      if (code !== 0 || !/^[0-9]{1,6}$/.test(id)) {
        callFailed = true
        _callPhase = ""
        _callId = ""
        refreshCalls()
        return
      }
      _callId = id
      _callPhase = "start"
      callProcess.command = ["mmcli", "-o", id, "--start"]
      callProcess.running = true
      return
    }
    if (phase === "start" && code !== 0 && /^[0-9]{1,6}$/.test(_callId)) {
      callFailed = true
      var failedId = _callId
      _callId = ""
      _callPhase = "delete"
      callProcess.command = ["mmcli", "-m", "0", "--voice-delete-call=" + failedId]
      callProcess.running = true
      return
    }
    _callPhase = ""
    _callId = ""
    if (code !== 0 && phase !== "delete") callFailed = true
    refreshCalls()
  }

  function applySms(raw) {
    var report = Model.parseSmsReport(raw)
    if (!report.ok) {
      smsFailed = true
      return
    }
    smsFailed = false
    var arrived = []
    var nextSeen = ({})
    for (var i = 0; i < report.messages.length; i++) {
      var item = report.messages[i]
      nextSeen[item.id] = true
      if (_smsSeeded && !_seenSms[item.id] && item.received)
        arrived.push(item)
    }
    _seenSms = nextSeen
    _smsSeeded = true
    messages = report.messages
    if (arrived.length) messagesArrived(arrived)
  }

  function finishAction(ok, detail) {
    actionTimeout.stop()
    if (!ok) {
      _desired = -1
      lastError = Model.plain(detail, 160)
      if (lastError === "") lastError = "failed"
    } else {
      lastError = ""
    }
    refresh()
  }

  onConnectionNameChanged: refresh()
  Component.onCompleted: {
    refresh()
    refreshSms()
    refreshCalls()
  }

  Timer {
    interval: 8000
    running: true
    repeat: true
    onTriggered: if (!root.busy) root.refresh()
  }

  Timer {
    interval: 15000
    running: true
    repeat: true
    triggeredOnStart: false
    onTriggered: root.refreshSms()
  }

  Timer {
    interval: 2500
    running: true
    repeat: true
    triggeredOnStart: false
    onTriggered: root.refreshCalls()
  }

  Timer {
    id: actionTimeout
    interval: 45000
    onTriggered: {
      if (!actionProcess.running) return
      root._gaveUp = true
      actionProcess.running = false
      Qt.callLater(function() {
        if (!root._gaveUp) return
        root._gaveUp = false
        root.finishAction(false, "timed out")
      })
    }
  }

  Process {
    id: stateProcess
    stdout: StdioCollector { id: stateOut; waitForEnd: true }
    stderr: StdioCollector { id: stateErr; waitForEnd: true }
    onExited: function(code) {
      root.profileMissing = code !== 0
      root.liveOn = code === 0 && Model.connectionIsActive(stateOut.text)
      if (root._desired !== -1 && root.liveOn === (root._desired === 1) && !root.busy)
        root._desired = -1
      modemProcess.command = ["mmcli", "-m", "0", "-J"]
      modemProcess.running = true
    }
  }

  Process {
    id: modemProcess
    stdout: StdioCollector { id: modemOut; waitForEnd: true }
    stderr: StdioCollector { id: modemErr; waitForEnd: true }
    onExited: function(code) {
      var modem = code === 0 ? Model.parseModem(modemOut.text) : { present: false, operatorName: "", signal: -1 }
      root.modemPresent = modem.present
      root.operatorName = modem.operatorName
      root.signal = modem.signal
      if (root._queued && !root.busy) root.refresh()
    }
  }

  Process {
    id: modifyProcess
    stdout: StdioCollector { id: modifyOut; waitForEnd: true }
    stderr: StdioCollector { id: modifyErr; waitForEnd: true }
    onExited: function(code) {
      if (code !== 0) {
        root.finishAction(false, modifyErr.text || modifyOut.text)
        return
      }
      actionTimeout.restart()
      actionProcess.command = ["nmcli", "connection", root._turnOn ? "up" : "down", root.connectionName]
      actionProcess.running = true
    }
  }

  Process {
    id: smsProcess
    stdout: StdioCollector { id: smsOut; waitForEnd: true }
    stderr: StdioCollector { id: smsErr; waitForEnd: true }
    onExited: function(code) {
      if (code !== 0) {
        root.smsFailed = true
        return
      }
      root.applySms(smsOut.text)
    }
  }

  Process {
    id: callListProcess
    stdout: StdioCollector { id: callListOut; waitForEnd: true }
    stderr: StdioCollector { id: callListErr; waitForEnd: true }
    onExited: function(code) {
      if (code !== 0) return
      root.applyCalls(callListOut.text)
    }
  }

  Process {
    id: callProcess
    stdout: StdioCollector { id: callOut; waitForEnd: true }
    stderr: StdioCollector { id: callErr; waitForEnd: true }
    onExited: function(code) {
      var phase = root._callPhase
      var text = callOut.text
      Qt.callLater(function() { root.finishCallStep(phase, code, text) })
    }
  }

  Process {
    id: actionProcess
    stdout: StdioCollector { id: actionOut; waitForEnd: true }
    stderr: StdioCollector { id: actionErr; waitForEnd: true }
    onExited: function(code) {
      var gaveUp = root._gaveUp
      root._gaveUp = false
      root.finishAction(!gaveUp && code === 0, gaveUp ? "timed out" : (actionErr.text || actionOut.text))
    }
  }
}
