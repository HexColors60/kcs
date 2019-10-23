#include <kcc.h>
#if !AMALGAMATION
# define INTERNAL
# define EXTERNAL extern
#endif
#include "linker.h"
#include <lacc/array.h>

#include <sys/types.h>
#include <wait.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <xunistd.h>

static array_of(char *) ld_args;

static void add_option(const char *opt)
{
#if !defined(KCC_WINDOWS)
    size_t len;
    char *buf;

    len = strlen(opt) + 1;
    buf = calloc(len, sizeof(*buf));
    strcpy(buf, opt);
    array_push_back(&ld_args, buf);
#endif
}

INTERNAL void clear_linker_args(void)
{
#if !defined(KCC_WINDOWS)
    int i;
    char *ptr;

    for (i = 0; i < array_len(&ld_args); ++i) {
        ptr = array_get(&ld_args, i);
        free(ptr);
    }

    array_clear(&ld_args);
#endif
}

static void init_linker(void)
{
#if !defined(KCC_WINDOWS)
    add_option("/usr/bin/ld");
    add_option("--eh-frame-hdr");
    add_option("-e");
#if __OpenBSD__
    add_option("__start");
    add_option("-dynamic-linker");
    add_option("/usr/libexec/ld.so");
    add_option("/usr/lib/crt0.o");
    add_option("/usr/lib/crtbegin.o");
    add_option("/usr/lib/crtend.o");
#else
    add_option("_start");
    add_option("-dynamic-linker");
    add_option("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2");
    add_option("/usr/lib/x86_64-linux-gnu/crt1.o");
    add_option("/usr/lib/x86_64-linux-gnu/crti.o");
    add_option("/usr/lib/x86_64-linux-gnu/crtn.o");
#endif
    add_option("-L/usr/local/lib");
    add_option("-L/usr/lib");
#endif
}

INTERNAL int add_linker_arg(const char *opt)
{
#if !defined(KCC_WINDOWS)
    if (!array_len(&ld_args)) {
        init_linker();
    }

    add_option(opt);
#endif
    return 0;
}

INTERNAL int invoke_linker(void)
{
#if !defined(KCC_WINDOWS)
    char **argv;
    int status, ret;
    pid_t pid;

    add_option("-lc");
    array_push_back(&ld_args, (char *) NULL);
    argv = &array_get(&ld_args, 0);
    switch ((pid = fork())) {
    case 0:
        execvp(argv[0], argv);
        _exit(0);
    case -1:
        fprintf(stderr, "%s\n", "Failed to start linker process.");
        ret = 1;
        break;
    default:
        waitpid(pid, &status, 0);
        ret = WEXITSTATUS(status);
        break;
    }

    return ret;
#endif
    return 1;
}
