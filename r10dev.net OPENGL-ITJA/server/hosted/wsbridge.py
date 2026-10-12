#!/usr/bin/env python3
"""ws->tcp bridge for the metin2 web client, production variant.

Same wire protocol as web/serve.py's /ws endpoint, but:
- ws only (no static file serving; the site lives on GitHub Pages)
- target allowlist via M2_ALLOWED_TARGETS (comma-separated host:port) so the
  public endpoint can't be used as an open TCP relay into the LAN
- /healthz for tunnel/uptime checks

    python3 wsbridge.py --port 8000
"""
import argparse
import base64
import functools
import hashlib
import http.server
import json
import os
import re
import socket
import sqlite3
import struct
import threading
import time
import urllib.parse

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
ALLOWED = set(
    t.strip()
    for t in os.environ.get(
        "M2_ALLOWED_TARGETS", "127.0.0.1:11000,127.0.0.1:11011"
    ).split(",")
    if t.strip()
)

M2_SQLITE_DIR = os.environ.get("M2_SQLITE_DIR", "")
REGISTER_DB = os.path.join(M2_SQLITE_DIR, "account.sqlite3") if M2_SQLITE_DIR else ""

# crude per-IP registration throttle: at most N per window
RATE_N = int(os.environ.get("M2_REGISTER_RATE", "5"))
RATE_WINDOW = 600  # seconds
_rate = {}  # ip -> [timestamps]
LOGIN_RE = re.compile(r"^[A-Za-z0-9_]{4,16}$")


def mysql_password(pw):
    return "*" + hashlib.sha1(hashlib.sha1(pw.encode()).digest()).hexdigest().upper()


def rate_ok(ip):
    now = time.time()
    hits = [t for t in _rate.get(ip, []) if now - t < RATE_WINDOW]
    if len(hits) >= RATE_N:
        _rate[ip] = hits
        return False
    hits.append(now)
    _rate[ip] = hits
    return True


def register_account(login, password, social_id=""):
    """Insert a row into the account db the game db process reads.

    Returns (ok, error). Never logs the password.
    """
    if not LOGIN_RE.match(login):
        return False, "username must be 4-16 letters, digits or _"
    if not (6 <= len(password) <= 64):
        return False, "password must be 6-64 characters"
    if social_id and not re.match(r"^[0-9]{7}$", social_id):
        return False, "deletion code must be 7 digits"
    if not REGISTER_DB or not os.path.exists(REGISTER_DB):
        return False, "registration unavailable"
    try:
        con = sqlite3.connect(REGISTER_DB, timeout=5)
        try:
            con.execute(
                "INSERT INTO account (login, password, social_id, create_time, status, ip) "
                "VALUES (?, ?, ?, ?, 'OK', '')",
                (login, mysql_password(password), social_id,
                 time.strftime("%Y-%m-%d %H:%M:%S")),
            )
            con.commit()
        finally:
            con.close()
    except sqlite3.IntegrityError:
        return False, "that username is taken"
    except sqlite3.Error:
        return False, "registration failed, try again later"
    return True, ""


def ws_accept(key):
    return base64.b64encode(
        hashlib.sha1((key + WS_GUID).encode()).digest()
    ).decode()


def ws_recv_frame(conn):
    hdr = conn.recv(2)
    if len(hdr) < 2:
        return None
    op = hdr[0] & 0x0F
    masked = hdr[1] & 0x80
    ln = hdr[1] & 0x7F
    if ln == 126:
        ln = struct.unpack(">H", conn.recv(2))[0]
    elif ln == 127:
        ln = struct.unpack(">Q", conn.recv(8))[0]
    mask = conn.recv(4) if masked else b""
    payload = b""
    while len(payload) < ln:
        chunk = conn.recv(ln - len(payload))
        if not chunk:
            return None
        payload += chunk
    if mask:
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    return op, payload


def ws_send_frame(conn, payload):
    ln = len(payload)
    if ln < 126:
        hdr = struct.pack(">BB", 0x82, ln)
    elif ln < 65536:
        hdr = struct.pack(">BBH", 0x82, 126, ln)
    else:
        hdr = struct.pack(">BBQ", 0x82, 127, ln)
    conn.sendall(hdr + payload)


def bridge_ws_to_tcp(conn, target):
    host, _, port = target.rpartition(":")
    try:
        tcp = socket.create_connection((host, int(port)), timeout=10)
    except OSError:
        print(f"[wsbridge] tcp connect FAILED {target}", flush=True)
        return
    tcp.setblocking(True)
    print(f"[wsbridge] tcp connected {target}", flush=True)

    def tcp_to_ws():
        try:
            while True:
                data = tcp.recv(65536)
                if not data:
                    break
                ws_send_frame(conn, data)
        except OSError:
            pass
        try:
            conn.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

    t = threading.Thread(target=tcp_to_ws, daemon=True)
    t.start()
    try:
        while True:
            frame = ws_recv_frame(conn)
            if frame is None:
                break
            op, payload = frame
            if op == 8:
                break
            if op == 9:
                conn.sendall(struct.pack("BB", 0x8A, len(payload)) + payload)
                continue
            if op in (1, 2):
                tcp.sendall(payload)
    except OSError:
        pass
    finally:
        tcp.close()
        t.join(timeout=2)


class BridgeHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print("[wsbridge] " + fmt % args, flush=True)

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_POST(self):
        if self.path != "/register":
            return self.send_error(404)
        ip = self.client_address[0]
        if not rate_ok(ip):
            return self._json(429, {"ok": False, "error": "too many attempts, wait a bit"})
        try:
            n = min(int(self.headers.get("Content-Length", "0")), 4096)
        except ValueError:
            n = 0
        try:
            body = json.loads(self.rfile.read(n) or b"{}")
        except (ValueError, UnicodeDecodeError):
            return self._json(400, {"ok": False, "error": "bad request"})
        ok, err = register_account(
            str(body.get("login", "")),
            str(body.get("password", "")),
            str(body.get("social_id", "")),
        )
        if ok:
            print(f"[wsbridge] registered account {body.get('login')} from {ip}", flush=True)
            return self._json(200, {"ok": True})
        code = 409 if "taken" in err else 400
        return self._json(code, {"ok": False, "error": err})

    def do_GET(self):
        if self.path == "/healthz":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(b"ok")
            return
        if self.path == "/status":
            def alive(host_port):
                h, p = host_port.rsplit(":", 1)
                try:
                    s = socket.create_connection((h, int(p)), timeout=1)
                    s.close()
                    return True
                except OSError:
                    return False
            self._json(200, {
                "realm": "Dutluk",
                "auth": alive("127.0.0.1:11000"),
                "chan1": alive("127.0.0.1:11011"),
            })
            return
        if not (self.path.startswith("/ws?") or self.path == "/ws"):
            return self.send_error(404)
        if self.headers.get("Upgrade", "").lower() != "websocket":
            return self.send_error(400, "expected websocket upgrade")
        key = self.headers.get("Sec-WebSocket-Key", "")
        qs = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        target = (qs.get("target") or [None])[0]
        if not key or not target:
            return self.send_error(400, "missing key or target")
        if target not in ALLOWED:
            print(f"[wsbridge] target DENIED {target}", flush=True)
            return self.send_error(403, "target not allowed")
        self.send_response(101, "Switching Protocols")
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", ws_accept(key))
        self.end_headers()
        self.close_connection = True
        bridge_ws_to_tcp(self.connection, target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()
    with http.server.ThreadingHTTPServer(("0.0.0.0", args.port), BridgeHandler) as srv:
        print(f"[wsbridge] listening on :{args.port}, targets: {sorted(ALLOWED)}", flush=True)
        srv.serve_forever()


if __name__ == "__main__":
    main()
