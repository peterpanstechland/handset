#!/usr/bin/env python3
"""Print received SMS from ModemManager as one JSON object. Takes no arguments."""

import json
import re
import subprocess
import sys

MODEM = "0"
MAX_FETCH = 30
MAX_SHOWN = 12
MAX_TEXT = 700
INDEX = re.compile(r"/SMS/(\d+)\b")


def run(args):
    try:
        done = subprocess.run(args, capture_output=True, text=True, timeout=8)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, "", str(exc)
    return done.returncode, done.stdout or "", done.stderr or ""


def main():
    code, out, err = run(["mmcli", "-m", MODEM, "--messaging-list-sms"])
    combined = out + "\n" + err
    if code != 0 and "No sms" not in combined and "No SMS" not in combined:
        json.dump({"ok": False, "messages": []}, sys.stdout)
        return

    indexes = []
    for match in INDEX.finditer(combined):
        number = match.group(1)
        if number not in indexes:
            indexes.append(number)
        if len(indexes) >= MAX_FETCH:
            break

    messages = []
    for number in indexes:
        status, payload, _error = run(["mmcli", "-m", MODEM, "-s", number, "-J"])
        if status != 0 or not payload:
            continue
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError:
            continue
        sms = parsed.get("sms") if isinstance(parsed, dict) else None
        if not isinstance(sms, dict):
            continue
        content = sms.get("content") if isinstance(sms.get("content"), dict) else {}
        props = sms.get("properties") if isinstance(sms.get("properties"), dict) else {}
        state = str(props.get("state") or "")
        if state not in ("received", "receiving"):
            continue
        text = str(content.get("text") or "")
        if len(text) > MAX_TEXT:
            text = text[:MAX_TEXT]
        messages.append({
            "id": number,
            "number": str(content.get("number") or ""),
            "text": text,
            "timestamp": str(props.get("timestamp") or ""),
            "state": state,
            "storage": str(props.get("storage") or ""),
        })

    messages.sort(key=lambda item: (item["timestamp"], int(item["id"])), reverse=True)
    json.dump({"ok": True, "messages": messages[:MAX_SHOWN]}, sys.stdout)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        json.dump({"ok": False, "messages": []}, sys.stdout)
