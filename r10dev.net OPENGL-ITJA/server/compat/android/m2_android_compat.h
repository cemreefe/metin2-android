// Force-included into every Android server TU.
#pragma once
#if defined(__ANDROID__) && __ANDROID_API__ < 24 && defined(__cplusplus)
#include <dlfcn.h>
#include <ifaddrs.h>
#include <stddef.h>

// getifaddrs() only exists in bionic from API 24. Resolve it at runtime and
// otherwise report "no interfaces": the launcher always passes -I 127.0.0.1.
static inline int getifaddrs(struct ifaddrs **ifap)
{
    typedef int (*fn_t)(struct ifaddrs **);
    fn_t fn = (fn_t)dlsym(RTLD_DEFAULT, "getifaddrs");
    if (fn)
        return fn(ifap);
    *ifap = NULL;
    return 0;
}

static inline void freeifaddrs(struct ifaddrs *ifa)
{
    typedef void (*fn_t)(struct ifaddrs *);
    fn_t fn = (fn_t)dlsym(RTLD_DEFAULT, "freeifaddrs");
    if (fn && ifa)
        fn(ifa);
}
#endif
