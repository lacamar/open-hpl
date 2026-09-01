#!/usr/bin/env python3
"""
Client for the in-engine headless control server (see
HPL2/core/include/system/HeadlessControl.h). Talks newline-delimited JSON
over a Unix domain socket to drive/monitor a running game process, instead
of attaching gdb by hand (see PORTING_NOTES.md for the pain that caused).

The server only exists in a process launched with OPENHPL_HEADLESS_SOCKET
set to a socket path - it's a no-op otherwise, so this never affects normal
play.

As a library:

    from hpl_control import HplControl
    with HplControl("/tmp/openhpl-test.sock") as hpl:
        print(hpl.send({"cmd": "state"}))
        hpl.send({"cmd": "teleport", "x": 1, "y": 2, "z": 3})
        hpl.send({"cmd": "run_script", "line": 'AddTinderboxes(1, "Player")'})
        hpl.send({"cmd": "screenshot", "path": "/tmp/check.bmp"})
        hpl.send({"cmd": "quit"})

As a CLI (one command, flat key=value args become JSON fields; values that
parse as a number/true/false are sent as that type, everything else as a
string):

    scripts/hpl_control.py --socket /tmp/openhpl-test.sock state
    scripts/hpl_control.py --socket /tmp/openhpl-test.sock teleport x=1 y=2 z=3
    scripts/hpl_control.py --socket /tmp/openhpl-test.sock run_script line='AddTinderboxes(1, "Player")'
"""

import argparse
import json
import socket
import sys


class HplControlError(RuntimeError):
    pass


class HplControl:
    def __init__(self, socket_path, timeout=10.0):
        self.socket_path = socket_path
        self.timeout = timeout
        self._sock = None
        self._buf = b""

    def connect(self):
        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.settimeout(self.timeout)
        self._sock.connect(self.socket_path)
        return self

    def close(self):
        if self._sock is not None:
            self._sock.close()
            self._sock = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def send(self, request):
        """Send one command dict, return the parsed response dict.
        Raises HplControlError if the response has "ok": false."""
        if self._sock is None:
            raise HplControlError("not connected - use as a context manager or call connect() first")

        line = json.dumps(request) + "\n"
        self._sock.sendall(line.encode("utf-8"))

        while b"\n" not in self._buf:
            chunk = self._sock.recv(65536)
            if not chunk:
                raise HplControlError("connection closed before a response line arrived")
            self._buf += chunk

        line, _, self._buf = self._buf.partition(b"\n")
        response = json.loads(line.decode("utf-8"))

        if not response.get("ok", False):
            raise HplControlError(response.get("error", "unknown error"))
        return response


def _coerce_value(raw):
    if raw == "true":
        return True
    if raw == "false":
        return False
    try:
        if "." in raw or "e" in raw or "E" in raw:
            return float(raw)
        return int(raw)
    except ValueError:
        return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--socket", required=True, help="path to the OPENHPL_HEADLESS_SOCKET socket")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("cmd", help="command name, e.g. state / teleport / run_script / screenshot / quit")
    parser.add_argument("fields", nargs="*", help="key=value pairs added as request fields")
    args = parser.parse_args()

    request = {"cmd": args.cmd}
    for field in args.fields:
        if "=" not in field:
            parser.error(f"expected key=value, got: {field!r}")
        key, _, raw_val = field.partition("=")
        request[key] = _coerce_value(raw_val)

    try:
        with HplControl(args.socket, timeout=args.timeout) as hpl:
            response = hpl.send(request)
    except (OSError, HplControlError) as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)

    print(json.dumps(response, indent=2))


if __name__ == "__main__":
    main()
