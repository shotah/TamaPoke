#pragma once
// Tiny host runner. No libc headers — SteamOS gcc has none.
// write(2) is provided by the installed glibc at link time.

extern "C" long write(int fd, const void *buf, unsigned long n);

static int gFails = 0;
static int gRuns = 0;

static void mt_write(const char *s) {
  unsigned long n = 0;
  while (s[n]) n++;
  write(1, s, n);
}

static void mt_fail(const char *name, const char *why) {
  gFails++;
  mt_write("FAIL  ");
  mt_write(name);
  mt_write(": ");
  mt_write(why);
  mt_write("\n");
}

static bool mt_streq(const char *a, const char *b) {
  if (!a || !b) return a == b;
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

#define RUN_TEST(fn)                                                           \
  do {                                                                         \
    gRuns++;                                                                   \
    fn();                                                                      \
  } while (0)

#define TEST_ASSERT_TRUE(cond)                                                 \
  do {                                                                         \
    if (!(cond)) mt_fail(__func__, #cond " is false");                         \
  } while (0)

#define TEST_ASSERT_FALSE(cond)                                                \
  do {                                                                         \
    if (cond) mt_fail(__func__, #cond " is true");                             \
  } while (0)

#define TEST_ASSERT_EQUAL(exp, act)                                            \
  do {                                                                         \
    if ((exp) != (act)) mt_fail(__func__, #exp " != " #act);                   \
  } while (0)

#define TEST_ASSERT_NOT_EQUAL(a, b)                                            \
  do {                                                                         \
    if ((a) == (b)) mt_fail(__func__, #a " == " #b);                           \
  } while (0)

#define TEST_ASSERT_LESS_OR_EQUAL(lim, v)                                      \
  do {                                                                         \
    if ((v) > (lim)) mt_fail(__func__, #v " > " #lim);                         \
  } while (0)

#define TEST_ASSERT_LESS_THAN(lim, v)                                          \
  do {                                                                         \
    if ((v) >= (lim)) mt_fail(__func__, #v " >= " #lim);                       \
  } while (0)

#define TEST_ASSERT_EQUAL_STRING(exp, act)                                     \
  do {                                                                         \
    if (!mt_streq((exp), (act))) mt_fail(__func__, #exp " != " #act);          \
  } while (0)

static int mt_end() {
  if (gFails) {
    mt_write("FAILED\n");
    return 1;
  }
  mt_write("ok  ");
  // print run count as a single digit range we care about (tests << 10)
  char buf[8];
  int n = gRuns, i = 0;
  if (n == 0) buf[i++] = '0';
  else {
    char tmp[8];
    int t = 0;
    while (n) {
      tmp[t++] = char('0' + (n % 10));
      n /= 10;
    }
    while (t) buf[i++] = tmp[--t];
  }
  buf[i] = 0;
  mt_write(buf);
  mt_write(" C++ tests\n");
  return 0;
}
