#pragma once
/*
 * In-page loopback TCP for the wasm build. Every module (client, db, game
 * cores) shares one SharedArrayBuffer that holds a listener registry and a
 * fixed table of connection slots, each with two ring buffers. All ops are
 * synchronous Atomics/memcpy — no postMessage, no DOM events — so nothing
 * can starve behind a busy wasm thread.
 *
 * Two layers:
 *   m2lb_*  raw connection-slot ops (connect by port, send, recv, close)
 *   m2lp_*  POSIX-ish socket facade over fake fds (server code calls this
 *           through #defines injected by m2dev-server-src-web.patch)
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Raw slot ops (JS side). Return values:
 *   m2lb_listen   -> listener id >=0, <0 on failure
 *   m2lb_conn     -> conn id >=0, -2 no listener on that port, -3 accept
 *                    queue full, -4 conn table full
 *   m2lb_accept   -> conn id >=0, -1 if nothing pending
 *   m2lb_send     -> bytes written (may be less than len; 0 = no room)
 *   m2lb_recv     -> bytes read, 0 = EOF (peer closed, drained), -1 = empty
 *   m2lb_state    -> bit0 readable, bit1 writable, bit2 peer-closed
 */
int m2lb_present(void);
int m2lb_listen(int port);
void m2lb_unlisten(int lid);
int m2lb_conn(int port);
int m2lb_accept(int lid);
int m2lb_send(int conn, int is_a, const char* buf, int len);
int m2lb_recv(int conn, int is_a, char* buf, int len);
int m2lb_state(int conn);
void m2lb_close(int conn, int is_a);

/* POSIX-ish facade for the patched server sources. */
#define M2LP_FD_BASE 400
int m2lp_socket(int type);	/* SOCK_STREAM / SOCK_DGRAM */
int m2lp_bind(int fd, int port);
int m2lp_listen(int fd);
int m2lp_accept(int fd, struct sockaddr_in* peer);
int m2lp_connect(int fd, int port);	/* host ignored: routing is by port */
ssize_t m2lp_recv(int fd, void* buf, size_t len);
ssize_t m2lp_send(int fd, const void* buf, size_t len);
ssize_t m2lp_recvfrom(int fd, void* buf, size_t len, int flags, void* from, void* fromlen);
int m2lp_close(int fd);
int m2lp_fcntl(int fd, int cmd, int arg);
int m2lp_select(int nfds, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv);
int m2lp_peek_readable(int fd);	/* nonzero: inbound bytes or pending accept */	/* debug progress marker -> SAB cell 22 */

#ifdef __cplusplus
}
#endif
