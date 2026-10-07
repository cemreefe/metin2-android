/*
 * SharedArrayBuffer loopback TCP — wasm side, shared by every module.
 *
 * SAB layout (all offsets in bytes):
 *   [0]  magic 'M2LB' (0x424C324D)
 *   [4]  spinlock
 *   [8]  unused
 *   [32] listener table: 64 entries * 96B = 6144B
 *        per entry (i32): state(0|1), port, pendHead, pendTail, pendQ[16]
 *   [8192] conn table: 32 slots * CONN_BLOCK
 *        CONN_BLOCK = 32B hdr + 2*RING_CAP data
 *        hdr (i32): state, closedA, closedB, headA, tailA, headB, tailB, rsvd
 *        dataA[RING_CAP] = A(connector)->B, dataB[RING_CAP] = B->A
 *
 * head/tail are monotonic u32 counters; position in ring = head % CAP.
 * Every op is synchronous Atomics/memcpy — no postMessage or DOM events —
 * so a wasm thread that never yields can't starve the transport.
 */

#include "m2lb.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- JS ops (all synchronous; control region guarded by a spinlock) ---- */

// Lazily builds this JS context's SAB views. Readiness is tracked on
// Module._m2lb — deliberately NOT a wasm-side flag: pthreads of one module
// share linear memory but each has its own JS Module object, so a wasm
// static would make one thread's init look like everyone's.
EM_JS(int, m2lb_js_ready, (), {
	if (Module._m2lb) return 1;
	// The SAB can reach us three ways: Module['m2lbSab'] on the module's own
	// JS context, wasmMemory.m2lbSab inside emcc pthreads (the memory object
	// is posted to workers with our expando attached), or self.m2lbSab where
	// a boot script set it directly.
	var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) ||
		(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) ||
		(typeof self !== 'undefined' && self.m2lbSab);
	if (!sab) return 0;
	var s = {
		i32: new Int32Array(sab),
		u8: new Uint8Array(sab),
		LOCK: 1,
		LIST_OFF: 8,		// i32 index (byte 32)
		LIST_MAX: 64,
		LIST_STRIDE: 24,	// i32s per listener
		PEND_MAX: 16,
		CONN_BASE: 8192,	// byte offset of conn table
		CONN_MAX: 32,
		RING_CAP: 131072,	// bytes per direction
		HDR: 32
	};
	s.CONN_BLOCK = s.HDR + 2 * s.RING_CAP;
	Module._m2lb = s;
	return 1;
});

EM_JS(int, m2lb_js_listen, (int port), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	while (Atomics.compareExchange(i32, s.LOCK, 0, 1) !== 0) {}
	var ret = -1;
	for (var i = 0; i < s.LIST_MAX; i++) {
		var b = s.LIST_OFF + i * s.LIST_STRIDE;
		if (i32[b] === 1 && i32[b + 1] === port) { ret = -5; break; }
	}
	if (ret === -1) for (var i = 0; i < s.LIST_MAX; i++) {
		var b = s.LIST_OFF + i * s.LIST_STRIDE;
		if (i32[b] === 0) {
			i32[b] = 1; i32[b + 1] = port; i32[b + 2] = 0; i32[b + 3] = 0;
			ret = i; break;
		}
	}
	Atomics.store(i32, s.LOCK, 0);
	return ret;
});

EM_JS(void, m2lb_js_unlisten, (int lid), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	while (Atomics.compareExchange(i32, s.LOCK, 0, 1) !== 0) {}
	i32[s.LIST_OFF + lid * s.LIST_STRIDE] = 0;
	Atomics.store(i32, s.LOCK, 0);
});

EM_JS(int, m2lb_js_conn, (int port), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	while (Atomics.compareExchange(i32, s.LOCK, 0, 1) !== 0) {}
	var lid = -1;
	for (var i = 0; i < s.LIST_MAX; i++) {
		var b = s.LIST_OFF + i * s.LIST_STRIDE;
		if (i32[b] === 1 && i32[b + 1] === port) { lid = i; break; }
	}
	if (lid < 0) { Atomics.store(i32, s.LOCK, 0); return -2; }
	var lb = s.LIST_OFF + lid * s.LIST_STRIDE;
	var head = i32[lb + 2], tail = i32[lb + 3];
	if (tail - head >= s.PEND_MAX) { Atomics.store(i32, s.LOCK, 0); return -3; }
	var cid = -1;
	for (var i = 0; i < s.CONN_MAX; i++) {
		var cb = (s.CONN_BASE + i * s.CONN_BLOCK) >> 2;
		if (i32[cb] === 0) {
			i32[cb] = 1;
			for (var j = 1; j < 7; j++) i32[cb + j] = 0;
			cid = i; break;
		}
	}
	if (cid < 0) { Atomics.store(i32, s.LOCK, 0); return -4; }
	i32[lb + 4 + (tail % s.PEND_MAX)] = cid;
	i32[lb + 3] = tail + 1;
	Atomics.store(i32, s.LOCK, 0);
	return cid;
});

EM_JS(int, m2lb_js_accept, (int lid), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	while (Atomics.compareExchange(i32, s.LOCK, 0, 1) !== 0) {}
	var lb = s.LIST_OFF + lid * s.LIST_STRIDE;
	var head = i32[lb + 2], tail = i32[lb + 3];
	var cid = -1;
	if (head !== tail) {
		cid = i32[lb + 4 + (head % s.PEND_MAX)];
		i32[lb + 2] = head + 1;
	}
	Atomics.store(i32, s.LOCK, 0);
	return cid;
});

EM_JS(int, m2lb_js_send, (int cid, int isA, int bufPtr, int len), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32, u8 = s.u8;
	var cb = (s.CONN_BASE + cid * s.CONN_BLOCK) >> 2;
	var headIdx = isA ? cb + 3 : cb + 5;
	var tailIdx = isA ? cb + 4 : cb + 6;
	var dataOff = s.CONN_BASE + cid * s.CONN_BLOCK + s.HDR + (isA ? 0 : s.RING_CAP);
	var head = i32[headIdx] >>> 0, tail = i32[tailIdx] >>> 0;
	var free = s.RING_CAP - (head - tail);
	if (free <= 0) return 0;
	var n = Math.min(len, free);
	var pos = head % s.RING_CAP;
	var first = Math.min(n, s.RING_CAP - pos);
	u8.set(HEAPU8.subarray(bufPtr, bufPtr + first), dataOff + pos);
	if (n > first) u8.set(HEAPU8.subarray(bufPtr + first, bufPtr + n), dataOff);
	Atomics.store(i32, headIdx, head + n);
	return n;
});

EM_JS(int, m2lb_js_recv, (int cid, int isA, int bufPtr, int max), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32, u8 = s.u8;
	var cb = (s.CONN_BASE + cid * s.CONN_BLOCK) >> 2;
	var headIdx = isA ? cb + 5 : cb + 3;	// inbound ring
	var tailIdx = isA ? cb + 6 : cb + 4;
	var dataOff = s.CONN_BASE + cid * s.CONN_BLOCK + s.HDR + (isA ? s.RING_CAP : 0);
	var head = i32[headIdx] >>> 0, tail = i32[tailIdx] >>> 0;
	var avail = head - tail;
	var peerClosed = i32[isA ? cb + 2 : cb + 1] !== 0;
	if (avail === 0) return peerClosed ? 0 : -1;
	var n = Math.min(max, avail);
	var pos = tail % s.RING_CAP;
	var first = Math.min(n, s.RING_CAP - pos);
	HEAPU8.set(u8.subarray(dataOff + pos, dataOff + pos + first), bufPtr);
	if (n > first) HEAPU8.set(u8.subarray(dataOff, dataOff + (n - first)), bufPtr + first);
	Atomics.store(i32, tailIdx, tail + n);
	return n;
});

EM_JS(int, m2lb_js_poll, (int cid, int isA), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	var cb = (s.CONN_BASE + cid * s.CONN_BLOCK) >> 2;
	var head = i32[isA ? cb + 5 : cb + 3] >>> 0;
	var tail = i32[isA ? cb + 6 : cb + 4] >>> 0;
	var peerClosed = i32[isA ? cb + 2 : cb + 1] !== 0;
	var oHead = i32[isA ? cb + 3 : cb + 5] >>> 0;
	var oTail = i32[isA ? cb + 4 : cb + 6] >>> 0;
	var st = 0;
	if (head - tail > 0 || peerClosed) st |= 1;
	if (s.RING_CAP - (oHead - oTail) > 0 || peerClosed) st |= 2;
	return st;
});

EM_JS(void, m2lb_js_heartbeat, (), {
	var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) ||
		(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) ||
		(typeof self !== 'undefined' && self.m2lbSab);
	if (!sab) return;
	var i32 = new Int32Array(sab);
	var hb = (8192 + 31 * (32 + 2 * 131072)) >> 2;	// unused conn slot 31 hdr
	var slot = (self.m2dbgSlot || 0) & 3;
	i32[hb] = (i32[hb] + (1 << (slot * 8))) | 0;
});

EM_JS(void, m2lb_js_hb0, (int nfds), {
	var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) ||
		(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) ||
		(typeof self !== 'undefined' && self.m2lbSab);
	if (!sab) return;
	var i32 = new Int32Array(sab);
	var hb = (8192 + 24 * (32 + 2 * 131072)) >> 2;	// conn slot 24 hdr
	var slot = (self.m2dbgSlot || 0) & 3;
	i32[hb] = (i32[hb] + (1 << (slot * 8 + (nfds > 0 ? 4 : 0)))) | 0;
});

EM_JS(void, m2lb_js_pend_seen, (), {
	var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) ||
		(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) ||
		(typeof self !== 'undefined' && self.m2lbSab);
	if (!sab) return;
	var i32 = new Int32Array(sab);
	var hb = (8192 + 30 * (32 + 2 * 131072)) >> 2;
	var slot = (self.m2dbgSlot || 0) & 3;
	i32[hb] = (i32[hb] + (1 << (slot * 8))) | 0;
});

EM_JS(int, m2lb_js_listener_pending, (int lid), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	var lb = s.LIST_OFF + lid * s.LIST_STRIDE;
	return i32[lb + 3] - i32[lb + 2];
});

EM_JS(void, m2lb_js_close, (int cid, int isA), {
	var s = Module._m2lb; if (!s) return -9; var i32 = s.i32;
	var cb = (s.CONN_BASE + cid * s.CONN_BLOCK) >> 2;
	while (Atomics.compareExchange(i32, s.LOCK, 0, 1) !== 0) {}
	i32[isA ? cb + 1 : cb + 2] = 1;
	if (i32[cb + 1] && i32[cb + 2]) i32[cb] = 0;
	Atomics.store(i32, s.LOCK, 0);
});

#define M2MARK(fid, fdx) \
	EM_ASM({ \
		var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) || \
			(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) || \
			(typeof self !== 'undefined' && self.m2lbSab); \
		if (!sab) return; \
		var i32 = new Int32Array(sab); \
		var base = (8192 + (16 - ((self.m2dbgSlot||0) & 3)) * (32 + 2 * 131072)) >> 2; \
		i32[base] = 0xF0000000 | (($1 & 0xffff) << 8) | $0; \
	}, fid, fdx)

#define M2MARKC(cv) \
	EM_ASM({ \
		var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) || \
			(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) || \
			(typeof self !== 'undefined' && self.m2lbSab); \
		if (!sab) return; \
		var i32 = new Int32Array(sab); \
		i32[(8192 + (9 - ((self.m2dbgSlot||0) & 3)) * (32 + 2 * 131072)) >> 2] = 0xC0000000 | ($0 & 0xffff); \
	}, cv)

#define M2MARK0() \
	EM_ASM({ \
		var sab = (typeof Module !== 'undefined' && Module['m2lbSab']) || \
			(typeof wasmMemory !== 'undefined' && wasmMemory && wasmMemory.m2lbSab) || \
			(typeof self !== 'undefined' && self.m2lbSab); \
		if (!sab) return; \
		new Int32Array(sab)[(8192 + (16 - ((self.m2dbgSlot||0) & 3)) * (32 + 2 * 131072)) >> 2] = 0; \
	})

/* ---- public raw API --------------------------------------------------- */

int m2lb_present(void)
{
	return m2lb_js_ready();
}

static void m2lb_ensure()
{
	m2lb_js_ready();	// lazy per-context init (each pthread has its own Module)
}

int m2lb_listen(int port)   { m2lb_ensure(); return m2lb_js_listen(port); }
void m2lb_unlisten(int lid) { m2lb_ensure(); m2lb_js_unlisten(lid); }
int m2lb_conn(int port)     { m2lb_ensure(); return m2lb_js_conn(port); }
int m2lb_accept(int lid)    { m2lb_ensure(); return m2lb_js_accept(lid); }
int m2lb_send(int c, int a, const char* b, int l) { m2lb_ensure(); return m2lb_js_send(c, a, (int)(uintptr_t)b, l); }
int m2lb_recv(int c, int a, char* b, int l)       { m2lb_ensure(); return m2lb_js_recv(c, a, (int)(uintptr_t)b, l); }
int m2lb_state(int c)       { return 0; }	// superseded by m2lp_select internals
void m2lb_close(int c, int a) { m2lb_ensure(); m2lb_js_close(c, a); }

/* ---- POSIX-ish facade over fake fds ----------------------------------- */

#include <fcntl.h>

#define M2LP_MAX_FD 384		/* fake fds are M2LP_FD_BASE+i */

enum { M2LP_FREE = 0, M2LP_TCP, M2LP_UDP, M2LP_LISTENER, M2LP_CONN_A, M2LP_CONN_B };

struct M2lpFd {
	int kind;
	int port;
	int lid;
	int conn;
	int flags;	/* bit0: blocking */
};

static M2lpFd s_fds[M2LP_MAX_FD];

static int m2lp_alloc(void)
{
	for (int i = 0; i < M2LP_MAX_FD; i++)
		if (s_fds[i].kind == M2LP_FREE) return i;
	return -1;
}

static M2lpFd* m2lp_get(int fd)
{
	int i = fd - M2LP_FD_BASE;
	if (i < 0 || i >= M2LP_MAX_FD || s_fds[i].kind == M2LP_FREE) return NULL;
	return &s_fds[i];
}

int m2lp_socket(int type)
{
	int i = m2lp_alloc();
	if (i < 0) { errno = EMFILE; return -1; }
	memset(&s_fds[i], 0, sizeof(s_fds[i]));
	s_fds[i].kind = (type == SOCK_DGRAM) ? M2LP_UDP : M2LP_TCP;
	fprintf(stderr, "[m2lp] socket -> fd=%d kind=%d\n", M2LP_FD_BASE + i, s_fds[i].kind);
	return M2LP_FD_BASE + i;
}

int m2lp_bind(int fd, int port)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) { errno = EBADF; return -1; }
	s->port = port;
	return 0;
}

/* Every m2lp_* entry that touches the SAB goes through m2lb_ensure() first. */

int m2lp_listen(int fd)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s || s->kind != M2LP_TCP) { errno = EINVAL; return -1; }
	M2MARK(0x16, fd);
	m2lb_ensure();
	int lid = m2lb_js_listen(s->port);
	if (lid < 0) { errno = EADDRINUSE; return -1; }
	s->kind = M2LP_LISTENER;
	s->lid = lid;
	return 0;
}

int m2lp_accept(int fd, struct sockaddr_in* peer)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s || s->kind != M2LP_LISTENER) { errno = EINVAL; return -1; }
	M2MARK(0x12, fd);
	m2lb_ensure();
	int cid = m2lb_js_accept(s->lid);
	if (cid < 0) { errno = EAGAIN; return -1; }
	int i = m2lp_alloc();
	if (i < 0) { errno = EMFILE; return -1; }
	memset(&s_fds[i], 0, sizeof(s_fds[i]));
	s_fds[i].kind = M2LP_CONN_B;
	s_fds[i].conn = cid;
	if (peer) {
		peer->sin_family = AF_INET;
		peer->sin_addr.s_addr = htonl(0x7f000001);
	}
	return M2LP_FD_BASE + i;
}

int m2lp_connect(int fd, int port)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) { errno = EBADF; fprintf(stderr, "[m2lp] connect EBADF fd=%d port=%d slot_kind=%d s_fds=%p\n", fd, port, (fd - M2LP_FD_BASE >= 0 && fd - M2LP_FD_BASE < M2LP_MAX_FD) ? s_fds[fd - M2LP_FD_BASE].kind : -99, (void*)s_fds); return -1; }
	/* Retry while no listener exists — startup ordering between the
	 * db/game/client workers is not deterministic. Matches the old
	 * blocking connect's 10s timeout. */
	M2MARK(0x15, fd);
	m2lb_ensure();
	for (int tries = 0; tries < 1000; tries++) {
		int cid = m2lb_js_conn(port);
		if (cid >= 0) {
			s->kind = M2LP_CONN_A;
			s->conn = cid;
			return 0;
		}
		if (cid == -3 || cid == -4) { errno = ECONNREFUSED; return -1; }
		usleep(10000);
	}
	errno = ETIMEDOUT;
	return -1;
}

ssize_t m2lp_recv(int fd, void* buf, size_t len)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) { errno = EBADF; return -1; }
	if (s->kind == M2LP_UDP) { errno = EAGAIN; return -1; }
	int isA = (s->kind == M2LP_CONN_A);
	M2MARK(0x13, fd);
	m2lb_ensure();
	M2MARKC(s->conn);
	int m2rc = 0;
	for (;;) {
		int n = m2lb_js_recv(s->conn, isA, (int)(uintptr_t)buf, (int)len);
		if (n != -1 || !(s->flags & 1)) return n == -1 ? (errno = EAGAIN, -1) : n;
		if (!(++m2rc & 0x3ff))
			EM_ASM({
				var sab = (Module['m2lbSab']) || (wasmMemory && wasmMemory.m2lbSab) || self.m2lbSab;
				if (sab) new Int32Array(sab)[(8192 + (5 - ((self.m2dbgSlot||0) & 3)) * (32 + 2 * 131072)) >> 2] = 0xE0000000 | ($0 & 0xffffff);
			}, m2rc);
		usleep(200);
	}
}

ssize_t m2lp_send(int fd, const void* buf, size_t len)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) { errno = EBADF; return -1; }
	if (s->kind != M2LP_CONN_A && s->kind != M2LP_CONN_B) { errno = ENOTCONN; return -1; }
	M2MARK(0x14, fd);
	m2lb_ensure();
	M2MARKC(s->conn);
	int n = m2lb_js_send(s->conn, s->kind == M2LP_CONN_A, (int)(uintptr_t)buf, (int)len);
	if (n == 0) { errno = EAGAIN; return -1; }
	return n;
}

ssize_t m2lp_recvfrom(int fd, void* buf, size_t len, int flags, void* from, void* fromlen)
{
	(void)fd; (void)buf; (void)len; (void)flags; (void)from; (void)fromlen;
	errno = EAGAIN;
	return -1;
}

int m2lp_peek_readable(int fd)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) return 0;
	m2lb_ensure();
	if (s->kind == M2LP_LISTENER)
		return m2lb_js_listener_pending(s->lid) > 0;
	if (s->kind == M2LP_CONN_A || s->kind == M2LP_CONN_B)
		return m2lb_js_poll(s->conn, s->kind == M2LP_CONN_A) & 1;
	return 0;
}

int m2lp_close(int fd)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) return 0;
	m2lb_ensure();
	if (s->kind == M2LP_LISTENER) m2lb_js_unlisten(s->lid);
	else if (s->kind == M2LP_CONN_A || s->kind == M2LP_CONN_B)
		m2lb_js_close(s->conn, s->kind == M2LP_CONN_A);
	s->kind = M2LP_FREE;
	return 0;
}

int m2lp_fcntl(int fd, int cmd, int arg)
{
	M2lpFd* s = m2lp_get(fd);
	if (!s) { errno = EBADF; return -1; }
	if (cmd == F_SETFL)
		s->flags = (arg & O_NONBLOCK) ? (s->flags & ~1) : (s->flags | 1);
	return 0;
}

int m2lp_select(int nfds, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv)
{
	M2MARK(0x11, nfds);
	m2lb_ensure();
	m2lb_js_heartbeat();
	m2lb_js_hb0(nfds);
	struct timeval start, now;
	gettimeofday(&start, NULL);
	long budget_us = tv ? tv->tv_sec * 1000000L + tv->tv_usec : 0;

	for (;;) {
		int ready = 0;
		fd_set rr, ww;
		FD_ZERO(&rr); FD_ZERO(&ww);
		for (int fd = 0; fd < nfds && fd < FD_SETSIZE; fd++) {
			M2lpFd* s = m2lp_get(fd);
			if (!s) continue;
			int st = 0;
			if (s->kind == M2LP_LISTENER) {
				int p = m2lb_js_listener_pending(s->lid);
				if (p > 0) m2lb_js_pend_seen();
				st = p > 0 ? 1 : 0;
			}
			else if (s->kind == M2LP_CONN_A || s->kind == M2LP_CONN_B)
				st = m2lb_js_poll(s->conn, s->kind == M2LP_CONN_A);
			if (r && FD_ISSET(fd, r) && (st & 1)) { FD_SET(fd, &rr); ready++; }
			if (w && FD_ISSET(fd, w) && (st & 2)) { FD_SET(fd, &ww); ready++; }
		}
		if (ready || budget_us <= 0) {
			if (r) *r = rr;
			if (w) *w = ww;
			if (e) FD_ZERO(e);
			return ready;
		}
		usleep(200);
		gettimeofday(&now, NULL);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000000L + (now.tv_usec - start.tv_usec);
		if (elapsed >= budget_us) {
			if (r) *r = rr;
			if (w) *w = ww;
			if (e) FD_ZERO(e);
			return 0;
		}
	}
}

#endif /* __EMSCRIPTEN__ */
