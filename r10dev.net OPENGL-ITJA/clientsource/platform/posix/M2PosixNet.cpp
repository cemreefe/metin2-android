// POSIX (BSD socket) implementation of the M2Net port. Used by the Android
// adapter and any desktop adapter; the web adapter has its own WebSocket impl.
#include "../m2net.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace M2Net
{
	int Connect(const char* host, int port)
	{
		struct sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = AF_INET;
		sa.sin_port = htons((uint16_t)port);
		sa.sin_addr.s_addr = inet_addr(host);
		if (sa.sin_addr.s_addr == INADDR_NONE)
		{
			struct hostent* he = gethostbyname(host);
			if (!he)
				return -1;
			memcpy(&sa.sin_addr, he->h_addr, sizeof(sa.sin_addr));
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0)
			return -1;

		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);

		if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) != 0 && errno != EINPROGRESS)
		{
			close(fd);
			return -1;
		}
		return fd;
	}

	bool ConnectDone(int handle, int* errCode)
	{
		fd_set w;
		FD_ZERO(&w);
		FD_SET(handle, &w);
		struct timeval tv = { 0, 0 };
		if (select(handle + 1, NULL, &w, NULL, &tv) <= 0)
			return false;
		int err = 0;
		socklen_t len = sizeof(err);
		getsockopt(handle, SOL_SOCKET, SO_ERROR, &err, &len);
		if (errCode)
			*errCode = err;
		return true;
	}

	int Poll(int handle)
	{
		fd_set r, w;
		FD_ZERO(&r);
		FD_ZERO(&w);
		FD_SET(handle, &r);
		FD_SET(handle, &w);
		struct timeval tv = { 0, 0 };
		if (select(handle + 1, &r, &w, NULL, &tv) < 0)
			return NET_POLL_ERROR;
		return (FD_ISSET(handle, &r) ? NET_POLL_READABLE : 0)
			| (FD_ISSET(handle, &w) ? NET_POLL_WRITABLE : 0);
	}

	int Send(int handle, const void* buf, int len)
	{
		int r = send(handle, (const char*)buf, len, MSG_NOSIGNAL);
		if (r >= 0)
			return r;
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? NET_WOULD_BLOCK : NET_ERROR;
	}

	int Recv(int handle, void* buf, int cap)
	{
		int r = recv(handle, (char*)buf, cap, 0);
		if (r > 0)
			return r;
		if (r == 0)
			return NET_CLOSED;
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? NET_WOULD_BLOCK : NET_ERROR;
	}

	void Close(int handle)
	{
		close(handle);
	}
}
