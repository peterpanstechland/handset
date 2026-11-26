// Pure helpers for the mobile-data bar widget. Operator names and command
// errors are untrusted, so plain() strips markup before any text is shown.

var ICON_SIM_OFF = "\udb81\udca9"
var ICON_SIGNAL_OFF = "\udb81\udf83"
var ICON_NONE = "\udb82\udcbf"
var ICON_BARS = ["\udb82\udcbc", "\udb82\udcbd", "\udb82\udcbe"]

function plain(value, max) {
  var limit = max > 0 ? max : 80
  var text = String(value == null ? "" : value)
    .replace(/[\u0000-\u001f\u007f]/g, " ")
    .replace(/[<>&%]/g, "")
    .replace(/\s+/g, " ")
    .trim()
  if (text === "--") return ""
  if (text.length > limit) text = text.slice(0, limit)
  return text
}

// Empty means the saved default. Anything a shell could treat specially is
// rejected so the name is only ever passed as one nmcli argument.
function connectionName(value) {
  var name = String(value == null ? "" : value).trim()
  if (name === "") name = "4g"
  if (!/^[A-Za-z0-9][A-Za-z0-9 ._:@+-]{0,63}$/.test(name)) return ""
  return name
}

function connectionIsActive(raw) {
  var line = String(raw || "").trim().split(/\r?\n/)[0].trim().toLowerCase()
  return line === "activated"
}

function parseModem(raw) {
  var missing = { present: false, operatorName: "", signal: -1 }
  var parsed
  try {
    parsed = JSON.parse(String(raw || ""))
  } catch (e) {
    return missing
  }
  var modem = parsed && parsed.modem
  if (!modem || !modem.generic) return missing
  var gpp = modem["3gpp"] || {}
  var quality = modem.generic["signal-quality"] || {}
  var signal = parseInt(quality.value, 10)
  if (!isFinite(signal)) signal = -1
  else signal = Math.max(0, Math.min(100, signal))
  return {
    present: true,
    operatorName: plain(gpp["operator-name"], 40),
    signal: signal
  }
}

function whenLabel(value) {
  var text = plain(value, 40)
  var match = /^(\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2})/.exec(text)
  if (!match) return text
  return match[2] + "-" + match[3] + " " + match[4] + ":" + match[5]
}

// ModemManager's SMS JSON is untrusted. Keep a short, plain list of received
// texts and drop anything whose id is not a modem index.
function parseSmsReport(raw) {
  var none = { ok: false, messages: [] }
  var parsed
  try {
    parsed = JSON.parse(String(raw || ""))
  } catch (e) {
    return none
  }
  if (!parsed || parsed.ok !== true || !Array.isArray(parsed.messages)) return none
  var messages = []
  var seen = {}
  for (var i = 0; i < parsed.messages.length && messages.length < 12; i++) {
    var item = parsed.messages[i]
    if (!item) continue
    var id = String(item.id == null ? "" : item.id)
    if (!/^[0-9]{1,6}$/.test(id) || seen[id]) continue
    var state = String(item.state || "")
    if (state !== "received" && state !== "receiving") continue
    seen[id] = true
    messages.push({
      id: id,
      from: plain(item.number, 24),
      text: plain(item.text, 420),
      when: whenLabel(item.timestamp),
      received: state === "received"
    })
  }
  return { ok: true, messages: messages }
}

// One argument for the notification body. A colon keeps it from being a flag
// such as --exec, and the pieces are already plain().
function notificationBody(from, text) {
  var who = from ? String(from) : "Unknown"
  var body = who + ": " + (text ? String(text) : "")
  if (body.length > 180) body = body.slice(0, 180)
  return body
}

// A number we will place in one mmcli argument. Anything else is refused.
function dialNumber(value) {
  var text = String(value == null ? "" : value).replace(/[\s-]/g, "")
  if (!/^\+?[0-9*#]{2,20}$/.test(text)) return ""
  return text
}

function callToken(value) {
  var text = String(value == null ? "" : value).trim().toLowerCase()
  var match = /^[a-z]+/.exec(text)
  return match ? match[0] : ""
}

function callIdFrom(raw) {
  var match = /\/Call\/([0-9]{1,6})\b/.exec(String(raw || ""))
  return match ? match[1] : ""
}

var LIVE_CALL = { dialing: true, ringing: true, active: true, held: true, waiting: true }

// Voice state from the modem is untrusted. Keep one live call, preferring
// an incoming ring over anything we placed ourselves.
function parseCallReport(raw) {
  var none = { ok: false, live: null }
  var parsed
  try {
    parsed = JSON.parse(String(raw || ""))
  } catch (e) {
    return none
  }
  if (!parsed || parsed.ok !== true || !Array.isArray(parsed.calls)) return none
  var incoming = null
  var other = null
  var seen = {}
  for (var i = 0; i < parsed.calls.length && i < 8; i++) {
    var item = parsed.calls[i]
    if (!item) continue
    var id = String(item.id == null ? "" : item.id)
    if (!/^[0-9]{1,6}$/.test(id) || seen[id]) continue
    seen[id] = true
    var state = callToken(item.state)
    if (!LIVE_CALL[state]) continue
    var direction = callToken(item.direction)
    var call = {
      id: id,
      number: plain(item.number, 24),
      state: state,
      incoming: direction === "incoming"
    }
    if (call.incoming && state === "ringing") incoming = call
    else if (!other) other = call
  }
  return { ok: true, live: incoming || other }
}

function iconFor(present, dataOn, signal) {
  if (!present) return ICON_SIM_OFF
  if (!dataOn) return ICON_SIGNAL_OFF
  if (!(signal >= 34)) return signal >= 1 ? ICON_BARS[0] : ICON_NONE
  if (signal < 67) return ICON_BARS[1]
  return ICON_BARS[2]
}

if (typeof module === "object" && module.exports) {
  module.exports = {
    plain: plain,
    connectionName: connectionName,
    connectionIsActive: connectionIsActive,
    parseModem: parseModem,
    parseSmsReport: parseSmsReport,
    notificationBody: notificationBody,
    dialNumber: dialNumber,
    callIdFrom: callIdFrom,
    parseCallReport: parseCallReport,
    iconFor: iconFor,
    ICON_SIM_OFF: ICON_SIM_OFF,
    ICON_SIGNAL_OFF: ICON_SIGNAL_OFF,
    ICON_NONE: ICON_NONE,
    ICON_BARS: ICON_BARS
  }
}
