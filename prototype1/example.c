#include "sys.h"

#define PUB __attribute__((visibility("default"))) /* WASM export */

// example helper functions
static void check_status(sys_ret r, const char* contextmsg);
static void print(const char* str);

void write_file(const char* path, const void* data, usize len) {
  sys_fd fd      = create(path, 0666);   check_status(fd, "create");
  isize n        = write(fd, data, len); check_status(n, "write");
  sys_ret status = close(fd);            check_status(status, "close");

  print("created file "); print(path); print("\n");
}

void read_file(const char* path, void* buf, usize cap) {
  print("read_file begin\n");
  sys_fd fd      = open(path, sys_open_ronly, 0); check_status(fd, "open");
  isize readlen  = read(fd, buf, cap);            check_status(readlen, "read");
  sys_ret status = close(fd);                     check_status(status, "close");

  print("read from file "); print(path); print(": ");
  write(SYS_FD_STDOUT, buf, readlen);
  if (readlen > 0 && ((u8*)buf)[readlen - 1] != '\n')
    print("\n");
}

PUB int main(int argc, const char** argv) {

  print("before sys_sleep\n");
  sys_sleep(0, 200000000); // 200ms
  print("after first sleep\n");
  sys_sleep(0, 200000000);
  print("done sleeping\n");

  const char* path = "hello.txt";
  u8* buf[64];
  read_file("/sys/uname", buf, sizeof(buf));

  const char* message = "Helloj wörld\n";
  write_file(path, message, strlen(message));

  read_file(path, buf, sizeof(buf));

  return 0;
}

// ------------------------------------------------------------------------
// example helper functions

static void print(const char* str) {
  write(SYS_FD_STDOUT, str, strlen(str));
}

static void printerr(const char* str) {
  write(SYS_FD_STDERR, str, strlen(str));
}

static void check_status(sys_ret r, const char* contextmsg) {
  if (r < 0) {
    sys_err err = (sys_err)-r;
    const char* errname = sys_errname(err);
    printerr("error: "); printerr(errname);
    if (contextmsg && strlen(contextmsg)) {
      printerr(" ("); printerr(contextmsg); printerr(")\n");
    } else {
      printerr("\n");
    }
    exit(1);
  }
}

// static u32 fmtuint(char* buf, usize bufsize, u64 v, u32 base) {
//   char rbuf[20]; // 18446744073709551615 (0xFFFFFFFFFFFFFFFF)
//   static const char chars[] =
//     "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
//   if (base > 62)
//     base = 62;
//   char* p = rbuf;
//   do {
//     *p++ = chars[v % base];
//     v /= base;
//   } while (v);
//   u32 len = (u32)(p - rbuf);
//   p--;
//   char* dst = buf;
//   char* end = buf + bufsize;
//   while (rbuf <= p && buf < end) {
//     *dst++ = *p--;
//   }
//   return len;
// }

// static void printint(i64 v, u32 base) {
//   char buf[21];
//   char* wbuf = buf;
//   u64 u = (u64)v;
//   if (v < 0) {
//     (*wbuf++) = '-';
//     u = (u64)-v;
//   }
//   u32 len = fmtuint(wbuf, sizeof(buf) - (usize)(wbuf - buf), u, base);
//   write(SYS_FD_STDOUT, buf, len);
// }
