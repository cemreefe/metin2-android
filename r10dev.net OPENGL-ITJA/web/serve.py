#!/usr/bin/env python3
"""Dev server for the web build: static files + the ws->tcp game bridge.

Three things python -m http.server won't give you:

- COOP/COEP headers. The client builds with -sUSE_PTHREADS (real worker
  threads for the file/area loaders), which requires SharedArrayBuffer,
  which requires cross-origin isolation.
- Range requests. Unused by the current prefetch shell but kept for
  range-capable tooling.
- /ws?target=<host>:<port> — WebSocket upgrade that pipes bytes to a TCP
  game server. The wasm client can't open raw TCP, so M2Net wraps every
  connection in a ws to this same-origin endpoint (see M2WebNet.cpp).

    python3 serve.py --root . --port 8000
"""
import argparse
import base64
import functools
import hashlib
import http.server
import re
import os
import socket
import struct
import threading
import urllib.parse

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_accept(key):
    return base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()


def ws_recv_frame(conn):
    """Read one client->server frame; returns (opcode, payload) or None."""
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
    """Send one unmasked binary frame server->client."""
    ln = len(payload)
    if ln < 126:
        hdr = struct.pack("BB", 0x82, ln)
    elif ln < 65536:
        hdr = struct.pack("BBH", 0x82, 126, ln)
    else:
        hdr = struct.pack("BBQ", 0x82, 127, ln)
    conn.sendall(hdr + payload)


def bridge_ws_to_tcp(conn, target):
    """Pump an established ws connection <-> tcp target until either dies."""
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
            if op == 8:  # close
                break
            if op == 9:  # ping -> pong
                conn.sendall(struct.pack("BB", 0x8A, len(payload)) + payload)
                continue
            if op in (1, 2):  # text/binary
                tcp.sendall(payload)
    except OSError:
        pass
    finally:
        tcp.close()
        t.join(timeout=2)


class RangeHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Accept-Ranges", "bytes")
        super().end_headers()

    def do_GET(self):
        if self.path.startswith("/ws?") or self.path == "/ws":
            return self.do_ws()
        rng = self.headers.get("Range")
        m = re.match(r"bytes=(\d+)-(\d*)$", rng or "")
        if not m:
            return super().do_GET()
        path = self.translate_path(self.path)
        if not os.path.isfile(path):
            self.send_error(404)
            return
        size = os.path.getsize(path)
        start = int(m.group(1))
        end = int(m.group(2)) if m.group(2) else size - 1
        end = min(end, size - 1)
        if start > end:
            self.send_error(416)
            return
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(end - start + 1))
        self.end_headers()
        with open(path, "rb") as f:
            f.seek(start)
            self.wfile.write(f.read(end - start + 1))

    def do_ws(self):
        if self.headers.get("Upgrade", "").lower() != "websocket":
            self.send_error(400, "expected websocket upgrade")
            return
        key = self.headers.get("Sec-WebSocket-Key", "")
        qs = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
        target = (qs.get("target") or [None])[0]
        if not key or not target:
            self.send_error(400, "missing key or target")
            return
        self.send_response(101, "Switching Protocols")
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", ws_accept(key))
        self.end_headers()
        self.close_connection = True
        bridge_ws_to_tcp(self.connection, target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()
    handler = functools.partial(RangeHandler, directory=args.root)
    with http.server.ThreadingHTTPServer(("0.0.0.0", args.port), handler) as srv:
        print(f"serving {os.path.abspath(args.root)} on http://0.0.0.0:{args.port}")
        srv.serve_forever()


if __name__ == "__main__":
    main()
