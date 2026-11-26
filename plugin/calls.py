#!/usr/bin/env python3
"""Print ModemManager voice calls as one JSON object. Takes no arguments."""

import json
import re
import subprocess
import sys

MODEM = "0"
MAX_CALLS = 8
INDEX = re.compile(r"/Call/(\d+)\b")


def run(args):
    try:
        done = subprocess.run(args, capture_output=True, text=True, timeout=8)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 1, "", str(exc)
    return done.returncode, done.stdout or "", done.stderr or ""


def token(value):
    text = str(value or "").strip().lower()
    piece = text.split()[0] if text else ""
    return piece.split("-")[0]


def main():
    code, out, err = run(["mmcli", "-m", MODEM, "--voice-list-calls"])
    combined = out + "\n" + err
    if code != 0 and "No calls" not in combined and "No voice" not in combined:
        json.dump({"ok": False, "calls": []}, sys.stdout)
        return

    indexes = []
    for match in INDEX.finditer(combined):
        number = match.group(1)
        if number not in indexes:
            indexes.append(number)
        if len(indexes) >= MAX_CALLS:
            break

    calls = []
    for number in indexes:
        status, payload, _error = run(["mmcli", "-o", number, "-J"])
        if status != 0 or not payload:
            continue
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError:
            continue
        call = parsed.get("call") if isinstance(parsed, dict) else None
        if not isinstance(call, dict):
            continue
        props = call.get("properties") if isinstance(call.get("properties"), dict) else {}
        calls.append({
            "id": number,
            "number": str(props.get("number") or ""),
            "state": token(props.get("state")),
            "direction": token(props.get("direction")),
        })

    json.dump({"ok": True, "calls": calls}, sys.stdout)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        json.dump({"ok": False, "calls": []}, sys.stdout)
