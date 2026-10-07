// Mobile-style HUD for the web port: floating movement stick + soft-keyboard
// bridge. Mirrors Android's JoystickView: touch/drag anywhere inside the pad
// plants the stick and walks with arrow keys (camera-relative, same as
// keyboard). The engine calls Module.m2SetGameControls(visible) when the HUD
// should be on screen (game phase only) and Module.m2ShowKeyboard(v, frac)
// when an edit line wants the OS keyboard.
"use strict";

(function () {
  const KEYS = { up: 38, down: 40, left: 37, right: 39 };

  function press(keyCode, down) {
    const ev = new KeyboardEvent(down ? "keydown" : "keyup", {
      keyCode: keyCode, which: keyCode, bubbles: true, cancelable: true,
    });
    // keyCode is read-only; define it for runtimes that ignore the init dict.
    try { Object.defineProperty(ev, "keyCode", { get: () => keyCode }); } catch (e) {}
    window.dispatchEvent(ev);
  }

  function makeJoystick() {
    const pad = document.createElement("div");
    pad.id = "m2joy";
    pad.style.cssText = [
      "position:fixed", "left:24px", "bottom:40px", "width:170px", "height:150px",
      "z-index:30", "display:none", "touch-action:none", "user-select:none",
      "-webkit-user-select:none",
    ].join(";");
    const ring = document.createElement("div");
    const knob = document.createElement("div");
    ring.style.cssText = "position:absolute;border-radius:50%;border:2px solid rgba(255,255,255,.38);display:none";
    knob.style.cssText = "position:absolute;border-radius:50%;background:rgba(224,192,128,.63);border:2px solid rgba(255,255,255,.5)";
    pad.appendChild(ring); pad.appendChild(knob);
    document.body.appendChild(pad);

    const R = 48, KR = 22; // css px, matching Android 48dp/22dp
    let pid = null, ox = 0, oy = 0;
    const held = { up: false, down: false, left: false, right: false };

    function setDirs(u, d, l, r) {
      const want = { up: u, down: d, left: l, right: r };
      for (const k in want)
        if (want[k] !== held[k]) { held[k] = want[k]; press(KEYS[k], want[k]); }
    }
    function release() {
      if (pid === null) return;
      pid = null;
      setDirs(false, false, false, false);
      ring.style.display = "none";
      knob.style.left = (85 - KR) + "px";
      knob.style.top = (75 - KR) + "px";
      knob.style.display = "block";
    }
    pad.addEventListener("pointerdown", (e) => {
      if (pid !== null) return;
      pid = e.pointerId;
      try { pad.setPointerCapture(pid); } catch (err) {}
      const r = pad.getBoundingClientRect();
      ox = e.clientX - r.left; oy = e.clientY - r.top;
      ring.style.cssText += "";
      ring.style.display = "block";
      ring.style.width = ring.style.height = (2 * R) + "px";
      ring.style.left = (ox - R) + "px"; ring.style.top = (oy - R) + "px";
      knob.style.width = knob.style.height = (2 * KR) + "px";
      knob.style.display = "block";
      knob.style.left = (ox - KR) + "px"; knob.style.top = (oy - KR) + "px";
      e.preventDefault();
    });
    pad.addEventListener("pointermove", (e) => {
      if (e.pointerId !== pid) return;
      const r = pad.getBoundingClientRect();
      let dx = e.clientX - r.left - ox, dy = e.clientY - r.top - oy;
      const len = Math.hypot(dx, dy);
      if (len > R) { dx *= R / len; dy *= R / len; }
      knob.style.left = (ox + dx - KR) + "px";
      knob.style.top = (oy + dy - KR) + "px";
      const dead = R * 0.25, diag = 0.38 * Math.min(len, R);
      setDirs(len > dead && -dy > diag, len > dead && dy > diag,
              len > dead && -dx > diag, len > dead && dx > diag);
      e.preventDefault();
    });
    for (const t of ["pointerup", "pointercancel", "lostpointercapture"])
      pad.addEventListener(t, (e) => { if (e.pointerId === pid || t === "lostpointercapture") release(); });
    release();
    return { el: pad, release: release };
  }

  let joy = null;
  window.m2HudInit = function (Module) {
    if (!joy) joy = makeJoystick();
    Module.m2SetGameControls = function (v) {
      joy.el.style.display = v ? "block" : "none";
      if (!v) joy.release();
    };
    // Soft keyboard: hidden input that pulls the OS keyboard up on mobile.
    let ime = document.getElementById("m2ime");
    if (!ime) {
      ime = document.createElement("input");
      ime.id = "m2ime";
      ime.setAttribute("autocomplete", "off");
      ime.style.cssText = "position:fixed;left:-200px;top:0;width:10px;height:10px;opacity:.01";
      document.body.appendChild(ime);
      // Char input takes two DOM paths when this input is focused: a
      // physical keyboard fires keypress (the engine's window listener runs
      // in the capture phase, so it already has the char) and then 'input'.
      // Mobile soft keyboards skip keypress (keyCode 229) and only fire
      // 'input'. Forward the inserted delta only when no keypress carried
      // it, so each character reaches the engine exactly once.
      let imePrev = "";
      let sawKeypress = false;
      ime.addEventListener("keypress", () => { sawKeypress = true; });
      ime.addEventListener("input", () => {
        const v = ime.value;
        let i = 0;
        while (i < imePrev.length && i < v.length && imePrev.charCodeAt(i) === v.charCodeAt(i)) i++;
        const added = v.length >= imePrev.length ? v.slice(i) : "";
        imePrev = v;
        if (v.length > 256) { ime.value = ""; imePrev = ""; }
        if (sawKeypress) { sawKeypress = false; return; }
        for (const ch of added)
          window.dispatchEvent(new KeyboardEvent("keypress", { charCode: ch.charCodeAt(0), bubbles: true }));
      });
      ime.addEventListener("blur", () => { ime.value = ""; imePrev = ""; });
    }
    Module.m2ShowKeyboard = function (visible, frac) {
      if (visible) { ime.style.top = (frac * window.innerHeight) + "px"; ime.focus(); }
      else ime.blur();
    };
  };
})();
