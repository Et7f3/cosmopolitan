/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/bits/bits.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/bsr.h"
#include "libc/nexgen32e/tinystrlen.internal.h"
#include "libc/rand/rand.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"

char u8[] = "utf-8 ☻";
char16_t u16[] = u"utf16 ☻";
wchar_t u32[] = L"utf32 ☻";

size_t strlen_pure(const char *s) {
  size_t n = 0;
  while (*s++) ++n;
  return n;
}

TEST(strlen, usageExample_c11) {
  _Alignas(16) char ugh[] = "eeeeeeeeeeeeeee\017";
  EXPECT_EQ(1, strlen_pure(ugh + 15));
  EXPECT_EQ(6 + 3, strlen(u8));
  EXPECT_EQ(7, strlen16(u16));
  EXPECT_EQ(7, wcslen(u32));
}

TEST(strlen, usageExample_c99) {
  EXPECT_EQ(6 + 3, strlen(u8));
  EXPECT_EQ(7, strlen16(u16));
  EXPECT_EQ(7, wcslen(u32));
}

TEST(strlen, whatMemoryLooksLike) {
  EXPECT_BINEQ(u"utf-8 Γÿ╗ ", u8); /* ← thompson-pike encoded */
  EXPECT_BINEQ(u"u t f 1 6   ;&  ", u16);
  EXPECT_BINEQ(u"u   t   f   3   2       ;&      ", u32);
}

TEST(strlen, test_const) {
  const char buf[] = "hellothere";
  ASSERT_EQ(0, strlen(""));
  ASSERT_EQ(0, strnlen("e", 0));
  ASSERT_EQ(10, strlen(buf));
}

TEST(strlen, test_nonconst) {
  char buf[256];
  unsigned i;
  for (i = 0; i < 255; ++i) buf[i] = i + 1;
  buf[i] = '\0';
  ASSERT_EQ(255, strlen(buf));
}

TEST(strnlen, testconst) {
  ASSERT_EQ(0, strnlen_s(NULL, 3));
  ASSERT_EQ(0, strnlen("", 3));
  ASSERT_EQ(0, strnlen("a", 0));
  ASSERT_EQ(3, strnlen("123", 3));
  ASSERT_EQ(2, strnlen("123", 2));
  ASSERT_EQ(3, strnlen("123", 4));
}

TEST(strlen, testnonconst) {
  /* this test case is a great example of why we need:
       "m"(*(char(*)[0x7fffffff])StR)
     rather than:
       "m"(*StR) */
  char buf[256];
  unsigned i;
  for (i = 0; i < 250; ++i) buf[i] = i + 1;
  buf[i] = '\0';
  ASSERT_EQ(250, strlen(buf));
}

TEST(strnlen_s, null_ReturnsZero) {
  ASSERT_EQ(0, strnlen_s(NULL, 3));
}

TEST(strnlen, nulNotFound_ReturnsSize) {
  int sizes[] = {1, 2, 7, 8, 15, 16, 31, 32, 33};
  for (unsigned i = 0; i < ARRAYLEN(sizes); ++i) {
    char *buf = malloc(sizes[i]);
    memset(buf, ' ', sizes[i]);
    ASSERT_EQ(sizes[i], strnlen(buf, sizes[i]), "%d", sizes[i]);
    free(buf);
  }
}

TEST(strnlen_s, nulNotFound_ReturnsZero) {
  char buf[3] = {1, 2, 3};
  ASSERT_EQ(0, strnlen_s(buf, 3));
}

TEST(tinystrlen, test) {
  ASSERT_EQ(0, tinystrlen(""));
  ASSERT_EQ(1, tinystrlen("a"));
  ASSERT_EQ(3, tinystrlen("123"));
}

TEST(tinywcslen, test) {
  ASSERT_EQ(0, tinywcslen(L""));
  ASSERT_EQ(1, tinywcslen(L"a"));
  ASSERT_EQ(3, tinywcslen(L"123"));
}

TEST(tinywcsnlen, test) {
  EXPECT_EQ(0, tinywcsnlen(L"", 3));
  EXPECT_EQ(0, tinywcsnlen(L"a", 0));
  EXPECT_EQ(3, tinywcsnlen(L"123", 3));
  EXPECT_EQ(2, tinywcsnlen(L"123", 2));
  EXPECT_EQ(3, tinywcsnlen(L"123", 4));
}

TEST(tinystrlen16, test) {
  ASSERT_EQ(0, tinystrlen16(u""));
  ASSERT_EQ(1, tinystrlen16(u"a"));
  ASSERT_EQ(3, tinystrlen16(u"123"));
}

TEST(tinystrnlen16, test) {
  EXPECT_EQ(0, tinystrnlen16(u"", 3));
  EXPECT_EQ(0, tinystrnlen16(u"a", 0));
  EXPECT_EQ(3, tinystrnlen16(u"123", 3));
  EXPECT_EQ(2, tinystrnlen16(u"123", 2));
  EXPECT_EQ(3, tinystrnlen16(u"123", 4));
}

TEST(strlen, fuzz) {
  char *b;
  size_t n, n1, n2;
  for (n = 2; n < 1026; ++n) {
    b = rngset(calloc(1, n), n - 1, rand64, -1);
    n1 = strlen(b);
    n2 = strlen_pure(b);
    ASSERT_EQ(n1, n2, "%#.*s", n, b);
    n1 = strlen(b + 1);
    n2 = strlen_pure(b + 1);
    ASSERT_EQ(n1, n2);
    free(b);
  }
}

BENCH(strlen, bench) {
  extern size_t strlen_(const char *) asm("strlen");
  extern size_t strlen_pure_(const char *) asm("strlen_pure");
  static char b[2048];
  static char c[512];
  static char d[256];
  memset(b, -1, sizeof(b) - 1);
  memset(c, -1, sizeof(c) - 1);
  memset(d, -1, sizeof(d) - 1);
  EZBENCH2("strlen_sse  1", donothing, strlen_(""));
  EZBENCH2("strlen_swar 1", donothing, strlen_pure_(""));
  EZBENCH2("strlen_sse  2", donothing, strlen_("1"));
  EZBENCH2("strlen_swar 2", donothing, strlen_pure_("1"));
  EZBENCH2("strlen_sse  3", donothing, strlen_("11"));
  EZBENCH2("strlen_swar 3", donothing, strlen_pure_("11"));
  EZBENCH2("strlen_sse  4", donothing, strlen_("113"));
  EZBENCH2("strlen_swar 4", donothing, strlen_pure_("113"));
  EZBENCH2("strlen_sse  7", donothing, strlen_("123456"));
  EZBENCH2("strlen_swar 7", donothing, strlen_pure_("123456"));
  EZBENCH2("strlen_sse  8", donothing, strlen_("1234567"));
  EZBENCH2("strlen_swar 8", donothing, strlen_pure_("1234567"));
  EZBENCH2("strlen_sse  9", donothing, strlen_("12345678"));
  EZBENCH2("strlen_swar 9", donothing, strlen_pure_("12345678"));
  EZBENCH2("strlen_sse  11", donothing, strlen_("12345678aa"));
  EZBENCH2("strlen_swar 11", donothing, strlen_pure_("12345678aa"));
  EZBENCH2("strlen_sse  13", donothing, strlen_("12345678aabb"));
  EZBENCH2("strlen_swar 13", donothing, strlen_pure_("12345678aabb"));
  EZBENCH2("strlen_sse  16", donothing, strlen_("123456781234567"));
  EZBENCH2("strlen_swar 16", donothing, strlen_pure_("123456781234567"));
  EZBENCH2("strlen_sse  17", donothing, strlen_("123456781234567e"));
  EZBENCH2("strlen_swar 17", donothing, strlen_pure_("123456781234567e"));
  EZBENCH2("strlen_sse  34", donothing,
           strlen_("123456781234567e123456781234567ee"));
  EZBENCH2("strlen_swar 34", donothing,
           strlen_pure_("123456781234567e123456781234567ee"));
  EZBENCH2("strlen_sse  68", donothing,
           strlen_("123456781234567e123456781234567ee123456781234567e1234567812"
                   "34567eee"));
  EZBENCH2("strlen_swar 68", donothing,
           strlen_pure_("123456781234567e123456781234567ee123456781234567e12345"
                        "6781234567eee"));
  EZBENCH2("strlen_sse  256", donothing, strlen_(d));
  EZBENCH2("strlen_swar 256", donothing, strlen_pure_(d));
  EZBENCH2("strlen_sse  512", donothing, strlen_(c));
  EZBENCH2("strlen_swar 512", donothing, strlen_pure_(c));
  EZBENCH2("strlen_sse  2048", donothing, strlen_(b));
  EZBENCH2("strlen_swar 2048", donothing, strlen_pure_(b));
}
