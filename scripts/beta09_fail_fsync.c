/* Test-only LD_PRELOAD fault injector, compiled on disposable Ubuntu CI.
 * It affects an exact target or the separately selected partial-file prefix;
 * no production dependency. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int fsync(int fd) {
    static int (*actual)(int) = NULL;
    if (!actual) actual = (int (*)(int))dlsym(RTLD_NEXT, "fsync");
    if (!actual) { errno = ENOSYS; return -1; }
    const char *target = getenv("DC24H_BETA09_FSYNC_TARGET");
    const char *prefix = getenv("DC24H_BETA09_FSYNC_TARGET_PREFIX");
    if ((target && *target) || (prefix && *prefix)) {
        char proc[64], path[4096];
        snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
        ssize_t count = readlink(proc, path, sizeof(path) - 1);
        if (count > 0) {
            path[count] = '\0';
            if ((target && *target && strcmp(path, target) == 0) ||
                (prefix && *prefix && strncmp(path, prefix, strlen(prefix)) == 0)) {
                errno = EIO;
                return -1;
            }
        }
    }
    return actual(fd);
}
