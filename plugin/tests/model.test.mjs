import assert from "node:assert/strict"
import { createRequire } from "node:module"

const require = createRequire(import.meta.url)
const Model = require("../Model.js")

assert.equal(Model.connectionName(""), "4g")
assert.equal(Model.connectionName("4g"), "4g")
assert.equal(Model.connectionName("  home-data  "), "home-data")
assert.equal(Model.connectionName("4g; rm -rf"), "")
assert.equal(Model.connectionName("$(id)"), "")
assert.equal(Model.connectionName("<img>"), "")

assert.equal(Model.connectionIsActive("activated\n"), true)
assert.equal(Model.connectionIsActive("disconnected"), false)
assert.equal(Model.connectionIsActive(""), false)

const markup = JSON.stringify({
  modem: {
    generic: { "signal-quality": { value: "47" } },
    "3gpp": { "operator-name": "<img src=x>CHN-CT" }
  }
})
const modem = Model.parseModem(markup)
assert.equal(modem.present, true)
assert.equal(modem.signal, 47)
assert.equal(modem.operatorName.includes("<"), false)
assert.equal(modem.operatorName.includes(">"), false)
assert.equal(modem.operatorName.includes("CHN-CT"), true)

assert.equal(Model.parseModem("not json").present, false)
assert.equal(Model.parseModem("{}").present, false)

assert.equal(Model.iconFor(false, false, 0), Model.ICON_SIM_OFF)
assert.equal(Model.iconFor(true, false, 80), Model.ICON_SIGNAL_OFF)
assert.equal(Model.iconFor(true, true, 0), Model.ICON_NONE)
assert.equal(Model.iconFor(true, true, 20), Model.ICON_BARS[0])
assert.equal(Model.iconFor(true, true, 47), Model.ICON_BARS[1])
assert.equal(Model.iconFor(true, true, 90), Model.ICON_BARS[2])

assert.equal(Model.plain("a\nb\t<img>&", 80), "a b img")

const sms = Model.parseSmsReport(JSON.stringify({
  ok: true,
  messages: [
    {
      id: "3",
      number: "+8613800138000",
      text: "<img src=x onerror=alert(1)>hello",
      timestamp: "2026-11-25T23:15:00+08",
      state: "received"
    },
    { id: "3", number: "dup", text: "nope", timestamp: "", state: "received" },
    { id: "x", number: "bad", text: "nope", timestamp: "", state: "received" },
    { id: "4", number: "--exec", text: "--exec", timestamp: "not-a-date", state: "sent" },
    { id: "5", number: "", text: "partial <b>x</b>", timestamp: "--", state: "receiving" }
  ]
}))
assert.equal(sms.ok, true)
assert.equal(sms.messages.length, 2)
assert.equal(sms.messages[0].id, "3")
assert.equal(sms.messages[0].from, "+8613800138000")
assert.equal(sms.messages[0].text.includes("<"), false)
assert.equal(sms.messages[0].text.includes("hello"), true)
assert.equal(sms.messages[0].when, "11-25 23:15")
assert.equal(sms.messages[0].received, true)
assert.equal(sms.messages[1].received, false)
assert.equal(sms.messages[1].from, "")
assert.equal(Model.parseSmsReport("nope").ok, false)
assert.equal(Model.parseSmsReport(JSON.stringify({ ok: false, messages: [] })).ok, false)

const note = Model.notificationBody(sms.messages[0].from, sms.messages[0].text)
assert.equal(note.startsWith("+8613800138000: "), true)
assert.equal(note.includes("<"), false)
assert.notEqual(Model.notificationBody("--exec", "--exec"), "--exec")
assert.notEqual(Model.notificationBody("", ""), "--exec")

assert.equal(Model.dialNumber(" 10086 "), "10086")
assert.equal(Model.dialNumber("+86 138-0013-8000"), "+8613800138000")
assert.equal(Model.dialNumber("*#100#"), "*#100#")
assert.equal(Model.dialNumber("1"), "")
assert.equal(Model.dialNumber("123;rm"), "")
assert.equal(Model.dialNumber("$(id)"), "")
assert.equal(Model.dialNumber("<img>"), "")

assert.equal(Model.callIdFrom("Successfully created\n/org/freedesktop/ModemManager1/Call/4\n"), "4")
assert.equal(Model.callIdFrom("no path"), "")
assert.equal(Model.callIdFrom("/Call/notanumber"), "")

const calls = Model.parseCallReport(JSON.stringify({
  ok: true,
  calls: [
    { id: "1", number: "<img src=x>10086", state: "active", direction: "outgoing" },
    { id: "2", number: "--exec", state: "ringing-in", direction: "incoming" },
    { id: "2", number: "dup", state: "ringing", direction: "incoming" },
    { id: "x", number: "9", state: "ringing", direction: "incoming" },
    { id: "3", number: "110", state: "terminated", direction: "incoming" }
  ]
}))
assert.equal(calls.ok, true)
assert.equal(calls.live.id, "2")
assert.equal(calls.live.incoming, true)
assert.equal(calls.live.state, "ringing")
assert.equal(calls.live.number.includes("<"), false)
assert.equal(calls.live.number.includes("--exec"), true)
assert.equal(Model.parseCallReport("nope").ok, false)
assert.equal(Model.parseCallReport(JSON.stringify({ ok: true, calls: [] })).live, null)

console.log("ok")
