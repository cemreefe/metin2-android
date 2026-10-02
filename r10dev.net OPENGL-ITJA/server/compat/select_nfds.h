// libthecore's select() backend passes nfds=0 (only valid on Winsock). On
// POSIX the kernel then examines no descriptors and leaves the sets as-is, so
// every fd looks readable and DESC::ProcessOutput never runs. Scan the full
// fd_set instead.
#pragma once
#include <sys/select.h>

static inline int m2_select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
	return select(nfds > 0 ? nfds : FD_SETSIZE, r, w, e, t);
}
#define select m2_select
