/* Test-only LD_PRELOAD fault injector, compiled on disposable Ubuntu CI.
 * It affects exactly DC24H_BETA09_FSYNC_TARGET; no production dependency. */
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
    if (target && *target) {
        char proc[64], path[4096];
        snprintf(proc, sizeof(proc), "/proc/self/fd/%d", fd);
        ssize_t count = readlink(proc, path, sizeof(path) - 1);
        if (count > 0) {
            path[count] = '\0';
            if (strcmp(path, target) == 0) { errno = EIO; return -1; }
        }
    }
    return actual(fd);
}
