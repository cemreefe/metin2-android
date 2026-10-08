#!/usr/bin/env python3
"""ws->tcp bridge for the web client.

Browsers can't open raw TCP sockets, so the wasm client wraps every
connection in a WebSocket to this bridge:

    ws://<bridge>/ws?target=<host>:<port>

The bridge dials host:port and pipes bytes both ways. Binary ws frames map
to TCP chunks and vice versa.

    python3 ws_bridge.py --bind 0.0.0.0:8080 [--allow 127.0.0.1,::1]

--allow limits which game hosts the bridge will dial (comma-separated);
omit it to allow anything (fine for local dev, risky if exposed).
"""
import argparse
import asyncio
import sys

try:
    import websockets
except ImportError:
    sys.exit("pip install websockets")


async def pump_ws_to_tcp(ws, writer):
    try:
        async for msg in ws:
            if isinstance(msg, str):
                continue
            writer.write(msg)
            await writer.drain()
    except websockets.ConnectionClosed:
        pass
    finally:
        writer.close()


async def pump_tcp_to_ws(ws, reader):
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            await ws.send(data)
    except websockets.ConnectionClosed:
        pass


async def handle(ws, allow):
    target = None
    for part in ws.path.split("?", 1)[-1].split("&"):
        if part.startswith("target="):
            target = part[7:]
    if not target or ":" not in target:
        await ws.close(4400, "missing ?target=host:port")
        return
    host, _, port_s = target.rpartition(":")
    try:
        port = int(port_s)
    except ValueError:
        await ws.close(4400, "bad port")
        return
    if allow is not None and host not in allow:
        await ws.close(4403, "host not allowed")
        return
    try:
        reader, writer = await asyncio.open_connection(host, port)
    except OSError as e:
        await ws.close(4502, str(e))
        return
    print(f"{ws.remote_address} -> {host}:{port}")
    try:
        await asyncio.gather(pump_ws_to_tcp(ws, writer), pump_tcp_to_ws(ws, reader))
    finally:
        writer.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", default="127.0.0.1:8080")
    ap.add_argument("--allow", default=None,
                    help="comma-separated host allowlist; default allows any")
    args = ap.parse_args()
    host, _, port = args.bind.rpartition(":")
    allow = set(args.allow.split(",")) if args.allow else None

    async def run():
        async with websockets.serve(lambda ws: handle(ws, allow), host, int(port),
                                    max_size=None):
            await asyncio.Future()  # forever

    print(f"ws_bridge listening on {args.bind}")
    asyncio.run(run())


if __name__ == "__main__":
    main()
