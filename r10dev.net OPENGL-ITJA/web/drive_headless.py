#!/usr/bin/env python3
"""Load the web client in headless Chrome via CDP, collect console logs and
periodic screenshots. Usage: drive.py <url> <secs> <shotdir> [actions.json]

actions.json is an optional list of timed input events, e.g.
  [{"t": 30, "op": "click", "x": 750, "y": 495},
   {"t": 32, "op": "type", "text": "test"},
   {"t": 33, "op": "key", "key": "Tab"}]
Ops: click, type (dispatches char keys), key (raw key name), eval (JS expr)."""
import asyncio, base64, json, os, subprocess, sys, time, urllib.request

CHROME = "google-chrome"
PORT = 9223

async def main():
    url = sys.argv[1]
    secs = float(sys.argv[2])
    shotdir = sys.argv[3] if len(sys.argv) > 3 else "/tmp/m2shots"
    actions = []
    if len(sys.argv) > 4:
        actions = json.load(open(sys.argv[4]))
        actions.sort(key=lambda a: a.get("t", 0))
    os.makedirs(shotdir, exist_ok=True)

    proc = subprocess.Popen([
        CHROME, "--headless=new", "--no-sandbox", "--disable-gpu",
        "--use-gl=swiftshader", "--enable-unsafe-swiftshader",
        f"--remote-debugging-port={PORT}",
        "--user-data-dir=/home/ubuntu/m2webtest/cdp-profile",
        "--window-size=1280,800", "about:blank",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                tabs = json.load(urllib.request.urlopen(f"http://localhost:{PORT}/json"))
                if tabs:
                    break
            except Exception:
                pass
            await asyncio.sleep(0.2)
        ws_url = tabs[0]["webSocketDebuggerUrl"]

        import websockets
        logs = []
        mid = 0
        pending = {}
        async def send(ws, method, params=None):
            nonlocal mid
            mid += 1
            await ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
            return mid

        async with websockets.connect(ws_url, max_size=64*1024*1024) as ws:
            await send(ws, "Runtime.enable")
            await send(ws, "Page.enable")
            await send(ws, "Log.enable")
            await send(ws, "Page.navigate", {"url": url})
            start = time.time()
            end = start + secs
            next_shot = time.time()
            nshot = 0
            async def run_action(a):
                op = a["op"]
                if op == "click":
                    for kind in ("mousePressed", "mouseReleased"):
                        await send(ws, "Input.dispatchMouseEvent", {
                            "type": kind, "x": a["x"], "y": a["y"],
                            "button": "left", "clickCount": 1})
                elif op == "type":
                    for ch in a["text"]:
                        await send(ws, "Input.dispatchKeyEvent", {
                            "type": "char", "text": ch})
                elif op == "key":
                    for kind in ("keyDown", "keyUp"):
                        await send(ws, "Input.dispatchKeyEvent", {
                            "type": "rawKeyDown" if kind == "keyDown" else "keyUp",
                            "key": a["key"], "code": a["key"]})
                elif op == "eval":
                    await send(ws, "Runtime.evaluate", {"expression": a["expr"]})
                logs.append(f"[action t={a.get('t')}] {op} done")
            while time.time() < end:
                now = time.time()
                while actions and now - start >= actions[0].get("t", 0):
                    await run_action(actions.pop(0))
                if time.time() >= next_shot:
                    sid = await send(ws, "Page.captureScreenshot")
                    pending[sid] = f"{shotdir}/shot_{nshot:03d}.png"
                    nshot += 1
                    next_shot = time.time() + 10
                try:
                    msg = json.loads(await asyncio.wait_for(ws.recv(), 0.5))
                except asyncio.TimeoutError:
                    continue
                m = msg.get("method")
                if m == "Runtime.consoleAPICalled":
                    args = [a.get("value", a.get("description", "")) for a in msg["params"]["args"]]
                    logs.append(f"[console.{msg['params']['type']}] " + " ".join(str(a) for a in args))
                elif m == "Runtime.exceptionThrown":
                    d = msg["params"]["exceptionDetails"]
                    logs.append(f"[exception] {d.get('text','')} {d.get('exception',{}).get('description','')[:2000]}")
                elif m == "Log.entryAdded":
                    e = msg["params"]["entry"]
                    logs.append(f"[log.{e['level']}] {e['text'][:1000]}")
                elif "id" in msg:
                    name = pending.pop(msg["id"], None)
                    if name:
                        if "error" in msg:
                            logs.append(f"[cdp.error] {name}: {msg['error']}")
                        else:
                            with open(name, "wb") as f:
                                f.write(base64.b64decode(msg["result"]["data"]))
            # dump syserr.txt from MEMFS (file ops proxy to main thread FS)
            try:
                eval_id = await send(ws, "Runtime.evaluate", {
                    "expression": "(() => { try { const d = FS.readFile('/data/syserr.txt'); return new TextDecoder().decode(d); } catch(e) { return 'no syserr: ' + e; } })()",
                    "returnByValue": True,
                })
                for _ in range(40):
                    msg = json.loads(await asyncio.wait_for(ws.recv(), 0.5))
                    if msg.get("id") == eval_id:
                        txt = msg["result"]["result"].get("value", "")
                        logs.append("==== syserr.txt ====\n" + txt[-12000:])
                        break
            except Exception as ex:
                logs.append(f"syserr dump failed: {ex}")
        print("\n".join(logs[-400:]))
    finally:
        proc.terminate()

asyncio.run(main())
