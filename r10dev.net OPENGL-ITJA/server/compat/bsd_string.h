// strlcpy/strlcat for host libcs that lack them (glibc < 2.38). bionic has both.
#ifndef M2_BSD_STRING_H
#define M2_BSD_STRING_H
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline size_t m2_strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = strlen(src);
	if (size)
	{
		size_t n = len >= size ? size - 1 : len;
		memcpy(dst, src, n);
		dst[n] = '\0';
	}
	return len;
}
static inline size_t m2_strlcat(char *dst, const char *src, size_t size)
{
	size_t dlen = strnlen(dst, size);
	if (dlen == size)
		return size + strlen(src);
	return dlen + m2_strlcpy(dst + dlen, src, size - dlen);
}
#ifdef __cplusplus
}
#endif
#define strlcpy m2_strlcpy
#define strlcat m2_strlcat
#endif
