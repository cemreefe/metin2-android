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
import os
import socket
import struct
import threading
import urllib.parse

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
ALLOWED = set(
    t.strip()
    for t in os.environ.get(
        "M2_ALLOWED_TARGETS", "127.0.0.1:11000,127.0.0.1:11011"
    ).split(",")
    if t.strip()
)


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

    def do_GET(self):
        if self.path == "/healthz":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"ok")
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
