// Web (Emscripten) adapter: implements the M2Plat port on top of the browser
// canvas, keyboard/pointer events and the wasm virtual filesystem.
//
// Layout of the world this file expects (see web/shell.html):
//   - <canvas id="canvas"> sized to the viewport; we scale it by devicePixelRatio.
//   - the shell prefetches manifest.json files into MEMFS under /data before
//     main() runs (lazy files can't work: every FS syscall is proxied to the
//     main thread under PROXY_TO_PTHREAD, and sync XHR is banned there)
//   - URL query params (m2_game_host, m2_ws_bridge, debug.m2.*, ...) become
//     process env vars via ApplyQueryEnv below
//   - Module.m2ShowKeyboard(v, frac) / Module.m2SetGameControls(v): optional
//     shell hooks for on-screen chrome; no-ops on desktop
#include "../m2platform.h"
#include "../../EterLib/MSApplication.h"
#include "../m2net.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
#include <emscripten/threading.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <vector>
#include <mutex>

// DOM key codes are already Win32-VK-shaped for the control keys the engine
// cares about; DIK codes below come from dinput.h.

namespace
{
	int s_iSurfaceW = 0;
	int s_iSurfaceH = 0;

	std::mutex s_kBlockerLock;
	std::vector<M2Plat::SRect> s_kBlockers;

	void UpdateSurfaceSize()
	{
		double w = 0, h = 0;
		if (emscripten_get_element_css_size("#canvas", &w, &h) != EMSCRIPTEN_RESULT_SUCCESS)
			return;
		const double dpr = emscripten_get_device_pixel_ratio();
		const int pw = (int)(w * dpr + 0.5);
		const int ph = (int)(h * dpr + 0.5);
		if (pw > 0 && ph > 0 && (pw != s_iSurfaceW || ph != s_iSurfaceH))
		{
			s_iSurfaceW = pw;
			s_iSurfaceH = ph;
			emscripten_set_canvas_element_size("#canvas", pw, ph);
		}
	}

	// Push DOM key/mouse events into the shared input queue. The queue hands them
	// to CPythonApplication::OnPortKeyEvent / OnTouchEvent on the game thread
	// (which, under emscripten, is the browser main thread — no locking needed
	// beyond what the queue already does).

	EM_BOOL KeyCb(int type, const EmscriptenKeyboardEvent* e, void*)
	{
		if (type == EMSCRIPTEN_EVENT_KEYPRESS)
		{
			// Text input; keyCode 0 means "character only".
			if (e->charCode)
				CMSApplication::PushKeyEvent(0, 0, (int)e->charCode);
			return EM_TRUE;
		}
		// Ignore repeat so held keys do not spam WM_CHAR-free presses.
		if (e->repeat)
			return EM_TRUE;
		CMSApplication::PushKeyEvent(type == EMSCRIPTEN_EVENT_KEYUP ? 1 : 0, (int)e->keyCode, 0);
		return EM_TRUE;
	}

	void SurfacePoint(const EmscriptenMouseEvent* e, int* x, int* y)
	{
		// canvasX is deprecated/never filled in new emscripten; targetX is
		// clientX - target rect, i.e. canvas-local.
		double cssW = s_iSurfaceW, cssH = s_iSurfaceH;
		emscripten_get_element_css_size("#canvas", &cssW, &cssH);
		*x = cssW > 0 ? (int)(e->targetX * (double)s_iSurfaceW / cssW) : e->targetX;
		*y = cssH > 0 ? (int)(e->targetY * (double)s_iSurfaceH / cssH) : e->targetY;
	}

	EM_BOOL MouseCb(int type, const EmscriptenMouseEvent* e, void*)
	{
		int x, y;
		SurfacePoint(e, &x, &y);
		switch (type)
		{
		case EMSCRIPTEN_EVENT_MOUSEDOWN:
			// Engine tracks cursor pos from move events only; a click that
			// arrives with no preceding move would land at stale coords, so
			// push a move first (on Windows a move always precedes the click).
			CMSApplication::PushTouchEvent(2, x, y);
			// Right button = camera rotate, like the desktop client.
			CMSApplication::PushTouchEvent(e->button == 2 ? CMSApplication::TOUCH_RIGHT_DOWN : 0, x, y);
			break;
		case EMSCRIPTEN_EVENT_MOUSEUP:
			CMSApplication::PushTouchEvent(2, x, y);
			CMSApplication::PushTouchEvent(e->button == 2 ? CMSApplication::TOUCH_RIGHT_UP : 1, x, y);
			break;
		case EMSCRIPTEN_EVENT_MOUSEMOVE:
			CMSApplication::PushTouchEvent(2, x, y);
			break;
		default:
			return EM_FALSE;
		}
		return EM_TRUE;
	}

	EM_BOOL WheelCb(int, const EmscriptenWheelEvent* e, void*)
	{
		CMSApplication::PushTouchEvent(CMSApplication::TOUCH_WHEEL, (int)e->deltaY, 0);
		return EM_TRUE;
	}

	EM_BOOL TouchCb(int type, const EmscriptenTouchEvent* e, void*)
	{
		if (!e->numTouches)
			return EM_TRUE;
		const EmscriptenTouchPoint& t = e->touches[0];
		double cssW = s_iSurfaceW, cssH = s_iSurfaceH;
		emscripten_get_element_css_size("#canvas", &cssW, &cssH);
		int x = cssW > 0 ? (int)(t.targetX * (double)s_iSurfaceW / cssW) : t.targetX;
		int y = cssH > 0 ? (int)(t.targetY * (double)s_iSurfaceH / cssH) : t.targetY;
		int action = 2;
		if (type == EMSCRIPTEN_EVENT_TOUCHSTART)
			action = 0;
		else if (type == EMSCRIPTEN_EVENT_TOUCHEND || type == EMSCRIPTEN_EVENT_TOUCHCANCEL)
			action = 1;
		CMSApplication::PushTouchEvent(action, x, y);
		return EM_TRUE;
	}

	EM_BOOL ResizeCb(int, const EmscriptenUiEvent*, void*)
	{
		UpdateSurfaceSize();
		return EM_TRUE;
	}
}

// ---- M2Plat ----

void M2Plat::LogV(ELogLevel level, const char* tag, const char* fmt, va_list args)
{
	static const char* const c_aszLevel[] = { "D", "I", "W", "E" };
	fprintf(stderr, "[%s] %s: ", c_aszLevel[level & 3], tag ? tag : "metin2");
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	fflush(stderr);
}

bool M2Plat::PresentFrame()
{
	// The engine drives its own loop on this worker, so there is no rAF
	// boundary for emscripten to auto-commit at. With a proxied context +
	// OFFSCREEN_FRAMEBUFFER the real context lives on the UI thread — ask
	// it to blit the offscreen framebuffer onto the canvas. Async: if the
	// UI thread falls behind, frames coalesce rather than stall the game.
	UpdateSurfaceSize();
	MAIN_THREAD_ASYNC_EM_ASM({
		if (GL.currentContext && GL.currentContext.defaultFbo)
			GL.blitOffscreenFramebuffer(GL.currentContext);
	});
	// DOM input callbacks land in this thread's mailbox — the engine never
	// yields, so pump it once per frame or input starves.
	emscripten_current_thread_process_queued_calls();
	return true;
}

int M2Plat::SurfaceWidth() { return s_iSurfaceW; }
int M2Plat::SurfaceHeight() { return s_iSurfaceH; }
const char* M2Plat::DataDir() { return "/data"; }
const char* M2Plat::FontFilePath() { return "font.ttf"; }

void M2Plat::SetKeyboardVisible(bool visible, float focusBottomFraction)
{
	// The game runs on a pthread (PROXY_TO_PTHREAD) where Module lacks the
	// page-installed hooks — run on the browser main thread instead.
	MAIN_THREAD_ASYNC_EM_ASM({ if (Module.m2ShowKeyboard) Module.m2ShowKeyboard($0, $1); },
		(int)visible, (double)focusBottomFraction);
}

void M2Plat::SetGameControlsVisible(bool visible)
{
	MAIN_THREAD_ASYNC_EM_ASM({ if (Module.m2SetGameControls) Module.m2SetGameControls($0); }, (int)visible);
}

void M2Plat::SetTouchBlockers(const SRect* rects, int count, int width, int height)
{
	std::lock_guard<std::mutex> lock(s_kBlockerLock);
	s_kBlockers.assign(rects, rects + count);
}

void M2Plat::RestartApp()
{
	// Stash display.cfg first: MEMFS is rebuilt from packs on reload, so the
	// user's ui_scale choice would be lost. local.html restores it at boot.
	// worker location is read-only; reload has to happen on the UI thread
	MAIN_THREAD_ASYNC_EM_ASM({
		try {
			var cfg = FS.readFile('/data/display.cfg', {encoding: 'utf8'});
			localStorage.setItem('m2_display_cfg', cfg);
		} catch (e) {}
		location.reload();
	});
}

int M2Plat::GetEnv(const char* name, char* out, size_t outSize)
{
	const char* v = getenv(name);
	if (!v || !out || !outSize)
		return 0;
	int n = snprintf(out, outSize, "%s", v);
	return n > 0 ? n : 0;
}

int M2Plat::GetDebugProperty(const char* name, char* out, size_t outSize)
{
	if (!out || !outSize)
		return 0;
	out[0] = 0;
	// URL query param, e.g. ?debug.m2.dump=3 — read through the main thread;
	// a worker's own location is the worker script URL, not the page's.
	int written = (int)MAIN_THREAD_EM_ASM_INT({
		var v = new URLSearchParams(location.search).get(UTF8ToString($0));
		if (v === null) return 0;
		stringToUTF8(v, $1, $2);
		return lengthBytesUTF8(v);
	}, name, out, outSize);
	if (written >= (int)outSize)
		out[outSize - 1] = 0;
	return written;
}

int M2Plat::KeyToDIK(int keyCode)
{
	static const unsigned char c_aLetterDIK[26] = {
		DIK_A, DIK_B, DIK_C, DIK_D, DIK_E, DIK_F, DIK_G, DIK_H, DIK_I, DIK_J, DIK_K, DIK_L, DIK_M,
		DIK_N, DIK_O, DIK_P, DIK_Q, DIK_R, DIK_S, DIK_T, DIK_U, DIK_V, DIK_W, DIK_X, DIK_Y, DIK_Z };
	static const unsigned char c_aDigitDIK[10] = {
		DIK_0, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9 };

	// DOM keyCode: A-Z = 65-90, 0-9 = 48-57, F1-F12 = 112-123.
	if (keyCode >= 65 && keyCode <= 90)
		return c_aLetterDIK[keyCode - 65];
	if (keyCode >= 48 && keyCode <= 57)
		return c_aDigitDIK[keyCode - 48];
	if (keyCode >= 112 && keyCode <= 121)
		return DIK_F1 + (keyCode - 112);
	if (keyCode == 122)
		return DIK_F11;
	if (keyCode == 123)
		return DIK_F12;

	switch (keyCode)
	{
	case 27: return DIK_ESCAPE;
	case 38: return DIK_UP;
	case 40: return DIK_DOWN;
	case 37: return DIK_LEFT;
	case 39: return DIK_RIGHT;
	case 9: return DIK_TAB;
	case 32: return DIK_SPACE;
	case 13: return DIK_RETURN;
	case 8: return DIK_BACK;
	case 46: return DIK_DELETE;
	case 16: return DIK_LSHIFT;
	case 17: return DIK_LCONTROL;
	case 18: return DIK_LMENU;
	case 36: return DIK_HOME;
	case 35: return DIK_END;
	case 33: return DIK_PRIOR;
	case 34: return DIK_NEXT;
	}
	return 0;
}

int M2Plat::KeyToVK(int keyCode)
{
	switch (keyCode)
	{
	case 27: return VK_ESCAPE;
	case 9: return VK_TAB;
	case 13: return VK_RETURN;
	case 8: return VK_BACK;
	case 38: return VK_UP;
	case 40: return VK_DOWN;
	case 37: return VK_LEFT;
	case 39: return VK_RIGHT;
	case 46: return VK_DELETE;
	case 36: return VK_HOME;
	case 35: return VK_END;
	}
	return 0;
}

// ---- entry point ----

int M2PortMain(int argc, char** argv);

// Copy the shell-provided env vars (from window.M2_ENV / URL query) into the
// process environment so getenv() callers in the core (M2_GAME_HOST etc.) just work.
// URL query params become process env vars: every m2_*/debug.* key lands
// uppercased in getenv(). Workers see location.search too.
static void ApplyQueryEnv()
{
	// must run on the main thread: the worker's location.search is the
	// worker script URL (empty query), the page's query lives on the UI side
	char* blob = (char*)MAIN_THREAD_EM_ASM_PTR({
		var s = "";
		new URLSearchParams(location.search).forEach(function (v, k) {
			s += k.toUpperCase() + "=" + v + "\n";
		});
		var buf = _malloc(s.length + 1);
		stringToUTF8(s, buf, s.length + 1);
		return buf;
	});
	if (!blob)
		return;
	for (char* p = blob; *p; )
	{
		char* nl = strchr(p, '\n');
		if (nl)
			*nl = 0;
		char* eq = strchr(p, '=');
		if (eq)
		{
			*eq = 0;
			setenv(p, eq + 1, 1);
		}
		p = nl ? nl + 1 : p + strlen(p);
	}
	free(blob);
}

int main(int argc, char** argv)
{
	UpdateSurfaceSize();

	// /data = cwd; the shell already prefetched the client files into it
	int iMk = mkdir("/data", 0755);
	fprintf(stderr, "[web] mkdir /data = %d errno=%d\n", iMk, errno);
	int iCd = chdir("/data");
	fprintf(stderr, "[web] chdir /data = %d errno=%d\n", iCd, errno);

	ApplyQueryEnv();

	// WebGL2 context up front — the GLES3 renderer binds directly to it.
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.majorVersion = 2;
	attrs.minorVersion = 0;
	attrs.alpha = EM_FALSE;
	attrs.depth = EM_TRUE;
	attrs.stencil = EM_TRUE;
	attrs.antialias = EM_FALSE;
	attrs.premultipliedAlpha = EM_FALSE;
	attrs.preserveDrawingBuffer = EM_FALSE;
	attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
	attrs.failIfMajorPerformanceCaveat = EM_FALSE;
	// main() runs on a pthread (PROXY_TO_PTHREAD): the canvas lives on the
	// UI thread, so context creation and every GL call get proxied there
	attrs.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_ALWAYS;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
	if (ctx <= 0)
	{
		M2Plat::Log(M2Plat::LOG_ERROR, "web", "WebGL2 context creation failed");
		return 1;
	}
	emscripten_webgl_make_context_current(ctx);

	fprintf(stderr, "[web] registering event callbacks\n");
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, KeyCb);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, KeyCb);
	emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, KeyCb);
	emscripten_set_mousedown_callback("#canvas", NULL, EM_TRUE, MouseCb);
	emscripten_set_mouseup_callback("#canvas", NULL, EM_TRUE, MouseCb);
	emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, MouseCb);
	emscripten_set_wheel_callback("#canvas", NULL, EM_TRUE, WheelCb);
	emscripten_set_touchstart_callback("#canvas", NULL, EM_TRUE, TouchCb);
	emscripten_set_touchend_callback("#canvas", NULL, EM_TRUE, TouchCb);
	emscripten_set_touchmove_callback("#canvas", NULL, EM_TRUE, TouchCb);
	emscripten_set_touchcancel_callback("#canvas", NULL, EM_TRUE, TouchCb);
	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, ResizeCb);

	// python2 needs its library path up front: the staged pyc set lives in
	// /data/lib, loose scripts in /data
	setenv("PYTHONPATH", "/data/lib:/data", 1);

	fprintf(stderr, "[web] entering M2PortMain\n");
	char szProgram[] = "metin2";
	char* av[] = { szProgram, NULL };
	M2PortMain(1, av);
	return 0;
}
