// macOS does not have a stable syscall ABI, instead its libc (libSystem) is
// the stable kernel API interface.
#include <fcntl.h>  // open
#include <unistd.h> // close, read, write
#include <stdlib.h> // exit
#include <time.h>   // nanosleep
#include <sys/errno.h>

#define SYS_NO_SYSLIB
#include "sys.h"
#undef SYS_NO_SYSLIB

#define SYS_API_VERSION       1
#define SYS_SPECIAL_FS_PREFIX "/sys"

#define static_assert _Static_assert

#if __has_attribute(musttail)
  #define MUSTTAIL __attribute__((musttail))
#else
  #define MUSTTAIL
#endif

#define XSTR(s) STR(s)
#define STR(s) #s

#define DEBUG
#ifdef DEBUG
  #include <stdio.h>
  #include <string.h> // strerror
  #define dlog(fmt, ...) fprintf(stderr, "[%s] " fmt "\n", __func__, ##__VA_ARGS__)
#else
  #define dlog(fmt, ...) ((void)0)
#endif


#define MAX(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

#define MIN(a,b) \
  ({__typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
  // turns into CMP + CMOV{L,G} on x86_64
  // turns into CMP + CSEL on arm64

// read and write relies on sys_ret being able to represent isize values
static_assert(sizeof(sys_ret) >= sizeof(isize), "");

extern int errno;

static sys_err sys_err_from_errno(int e) {
  // TODO
  return sys_err_invalid;
}

static struct {
  sys_callback_fn callback;
  void*           userdata;
} g_caller_state = {0};

// ---------------------------------------------------
// sys_syscall op implementations


static sys_ret sys_syscall_init(sys_opcode op, sys_callback_fn f, void* userdata) {
  g_caller_state.callback = f;
  g_caller_state.userdata = userdata;
  return 0;
}


static sys_ret sys_syscall_test(sys_opcode op, isize checkop) {
  if (op > sys_op_write)
    return -sys_err_not_supported;
  return 0;
}


static sys_ret sys_syscall_exit(sys_opcode op, isize status) {
  exit((int)status);
  return 0;
}


static sys_ret open_special_uname(sys_opcode op, const char* path, usize flags, isize mode) {
  #if defined(__i386) || defined(__i386__) || defined(_M_IX86)
    #define UNAME_STR "macos-x86"
  #elif defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
    #define UNAME_STR "macos-x64"
  #elif defined(__arm64__) || defined(__aarch64__)
    #define UNAME_STR "macos-arm64"
  #elif defined(__arm__) || defined(__arm) || defined(__ARM__) || defined(__ARM)
    #define UNAME_STR "macos-arm32"
  #elif defined(__ppc__) || defined(__ppc) || defined(__PPC__)
    #define UNAME_STR "macos-ppc"
  #else
    #error
  #endif
  int fd[2];
  if (pipe(fd) != 0)
    return -sys_err_from_errno(errno);
  const char* s = UNAME_STR " " XSTR(SYS_API_VERSION) "\n";
  int w = write(fd[1], s, strlen(s));
  close(fd[1]);
  if (w < 0) {
    close(fd[0]);
    return -sys_err_from_errno(errno);
  }
  return (sys_ret)fd[0];
}


static sys_ret open_special(sys_opcode op, const char* path, usize flags, isize mode) {
  path = path + strlen(SYS_SPECIAL_FS_PREFIX) + 1; // "/sys/foo/bar" => "foo/bar"
  //dlog("path \"%s\"", path);
  usize pathlen = strlen(path);
  #define ROUTE(matchpath,fun) \
    if (pathlen == strlen(matchpath) && memcmp(path,(matchpath),strlen(matchpath)) == 0) \
      MUSTTAIL return (fun)(op, path, flags, mode)
  ROUTE("uname", open_special_uname);
  #undef ROUTE
  return -sys_err_not_found;
}


static sys_ret sys_syscall_open(sys_opcode op, const char* path, usize flags, isize mode) {
  if (strlen(path) > strlen(SYS_SPECIAL_FS_PREFIX) &&
      memcmp(path, SYS_SPECIAL_FS_PREFIX "/", 5) == 0)
  {
    MUSTTAIL return open_special(op, path, flags, mode);
  }

  static const int oflag_map[3] = {
    [sys_open_ronly] = O_RDONLY,
    [sys_open_wonly] = O_WRONLY,
    [sys_open_rw]    = O_RDWR,
  };
  int oflag = oflag_map[flags & 3]; // first two bits is ro/wo/rw
  if (flags & sys_open_append) oflag |= O_APPEND;
  if (flags & sys_open_create) oflag |= O_CREAT;
  if (flags & sys_open_trunc)  oflag |= O_TRUNC;
  if (flags & sys_open_excl)   oflag |= O_EXCL;

  int fd = open(path, oflag, (mode_t)mode);
  if (fd < 0) {
    // dlog("open failed => %d (errno %d %s)", fd, errno, strerror(errno));
    return -sys_err_from_errno(errno);
  }

  return (sys_ret)fd;
}


static sys_ret sys_syscall_close(sys_opcode op, sys_fd fd) {
  if (close((int)fd) != 0)
    return -sys_err_from_errno(errno);
  return 0;
}


static sys_ret sys_syscall_read(sys_opcode op, sys_fd fd, void* data, usize size) {
  isize n = read((int)fd, data, size);
  if (n < 0)
    return -sys_err_from_errno(errno);
  return (sys_ret)n;
}


static sys_ret sys_syscall_write(sys_opcode op, sys_fd fd, const void* data, usize size) {
  isize n = write((int)fd, data, size);
  if (n < 0)
    return -sys_err_from_errno(errno);
  return (sys_ret)n;
}


static sys_ret sys_syscall_sleep(sys_opcode op, usize seconds, usize nanoseconds) {
  struct timespec rqtp = { .tv_sec = seconds, .tv_nsec = nanoseconds };
  // struct timespec remaining;
  int r = nanosleep(&rqtp, 0/*&remaining*/);
  if (r == 0)
    return 0;
  if (errno == EINTR) // interrupted
    return -sys_err_canceled; // FIXME TODO pass bach "remaining time" to caller
  return -sys_err_invalid;
}


typedef sys_ret (*syscall_fun)(sys_opcode,isize,isize,isize,isize,isize);
#define FORWARD(f) MUSTTAIL return ((syscall_fun)(f))(op,arg1,arg2,arg3,arg4,arg5)

sys_ret sys_syscall(
  sys_opcode op, isize arg1, isize arg2, isize arg3, isize arg4, isize arg5)
{
  //dlog("sys_syscall %u, %ld, %ld, %ld, %ld, %ld", op,arg1,arg2,arg3,arg4,arg5);
  switch ((enum sys_opcode)op) {
    case sys_op_init:  FORWARD(sys_syscall_init);
    case sys_op_test:  FORWARD(sys_syscall_test);
    case sys_op_exit:  FORWARD(sys_syscall_exit);
    case sys_op_open:  FORWARD(sys_syscall_open);
    case sys_op_close: FORWARD(sys_syscall_close);
    case sys_op_read:  FORWARD(sys_syscall_read);
    case sys_op_write: FORWARD(sys_syscall_write);
    case sys_op_sleep: FORWARD(sys_syscall_sleep);
  }
  return -sys_err_sys_op;
}
