#ifndef SYS_UNISTD_H
#define SYS_UNISTD_H

#ifdef __GNUC__

#include <unistd.h>

#else

#include <stdlib.h>
#include <io.h>
#include <process.h> /* for getpid() */
#include <direct.h>  /* for _getcwd(), _chdir() */

#define srandom     srand
#define random      rand

#define R_OK    4
#define W_OK    2
/* #define   X_OK    1 */
#define F_OK    0

#define access      _access
#define dup2        _dup2
#define execve      _execve
#define ftruncate   _chsize
#define unlink      _unlink
#define fileno      _fileno
#define getcwd      _getcwd
#define chdir       _chdir
#define isatty      _isatty
#define lseek       _lseek

#ifdef _WIN64
#define ssize_t     __int64
#else
#define ssize_t     long
#endif

typedef int	pid_t;

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* <sys/types.h> */
typedef __int8            int8_t;
typedef __int16           int16_t;
typedef __int32           int32_t;
typedef __int64           int64_t;
typedef unsigned __int8   uint8_t;
typedef unsigned __int16  uint16_t;
typedef unsigned __int32  uint32_t;
typedef unsigned __int64  uint64_t;

#endif
#endif
