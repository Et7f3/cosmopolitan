/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
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
#include "libc/errno.h"
#include "libc/fmt/conv.h"
#include "libc/limits.h"
#include "libc/stdio/stdio.h"
#include "libc/testlib/ezbench.h"
#include "libc/testlib/testlib.h"

TEST(atoi, test) {
  EXPECT_EQ(0, atoi(""));
  EXPECT_EQ(0, atoi("0"));
  EXPECT_EQ(1, atoi("1"));
  EXPECT_EQ(9, atoi("9"));
  EXPECT_EQ(12, atoi("12"));
  EXPECT_EQ(-12, atoi("-12"));
  EXPECT_EQ(31337, atoi("31337"));
  EXPECT_EQ(+0, atoi("+0"));
  EXPECT_EQ(+1, atoi("+1"));
  EXPECT_EQ(+9, atoi("+9"));
  EXPECT_EQ(-0, atoi("-0"));
  EXPECT_EQ(-1, atoi("-1"));
  EXPECT_EQ(-9, atoi("-9"));
  EXPECT_EQ(-31337, atoi("-31337"));
  EXPECT_EQ(INT_MIN, atoi("-2147483648"));
  EXPECT_EQ(INT_MAX, atoi("2147483647"));
  EXPECT_EQ(INT_MIN, atoi("-2147483649"));
  EXPECT_EQ(INT_MAX, atoi("2147483648"));
  EXPECT_EQ(INT_MIN, atoi("-2147483658"));
  EXPECT_EQ(INT_MAX, atoi("2147483657"));
  EXPECT_EQ(123, atoi(" 123"));
  EXPECT_EQ(123, atoi(" \t123"));
  EXPECT_EQ(+123, atoi(" +123"));
  EXPECT_EQ(+123, atoi(" \t+123"));
  EXPECT_EQ(-123, atoi(" -123"));
  EXPECT_EQ(-123, atoi(" \t-123"));
  EXPECT_EQ(0, atoi(" a123"));
  EXPECT_EQ(0, atoi(" \ta123"));
}

TEST(atoi, testWithinLimit_doesntChangeErrno) {
  errno = 7;
  EXPECT_EQ(INT_MAX, atoi("2147483647"));
  EXPECT_EQ(7, errno);
  errno = 7;
  EXPECT_EQ(INT_MIN, atoi("-2147483648"));
  EXPECT_EQ(7, errno);
}

TEST(atoi, testOutsideLimit_saturatesAndSetsErangeErrno) {
  errno = 0;
  EXPECT_EQ(INT_MAX, atoi("2147483648"));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(INT_MIN, atoi("-2147483649"));
  EXPECT_EQ(ERANGE, errno);
}

TEST(atol, test) {
  EXPECT_EQ(0, atol(""));
  EXPECT_EQ(0, atol("0"));
  EXPECT_EQ(1, atol("1"));
  EXPECT_EQ(9, atol("9"));
  EXPECT_EQ(12, atol("12"));
  EXPECT_EQ(-12, atol("-12"));
  EXPECT_EQ(31337, atol("31337"));
  EXPECT_EQ(+0, atol("+0"));
  EXPECT_EQ(+1, atol("+1"));
  EXPECT_EQ(+9, atol("+9"));
  EXPECT_EQ(-0, atol("-0"));
  EXPECT_EQ(-1, atol("-1"));
  EXPECT_EQ(-9, atol("-9"));
  EXPECT_EQ(-31337, atol("-31337"));
  EXPECT_EQ(LONG_MIN, atol("-9223372036854775808"));
  EXPECT_EQ(LONG_MAX, atol("9223372036854775807"));
  EXPECT_EQ(LONG_MIN, atol("-9223372036854775809"));
  EXPECT_EQ(LONG_MAX, atol("9223372036854775808"));
  EXPECT_EQ(LONG_MIN, atol("-9223372036854775818"));
  EXPECT_EQ(LONG_MAX, atol("9223372036854775817"));
  EXPECT_EQ(123, atol(" 123"));
  EXPECT_EQ(123, atol(" \t123"));
  EXPECT_EQ(-123, atol(" -123"));
  EXPECT_EQ(-123, atol(" \t-123"));
  EXPECT_EQ(0, atol(" a123"));
  EXPECT_EQ(0, atol(" \ta123"));
}

TEST(strtol, test) {
  EXPECT_EQ(0, strtol("", 0, 10));
  EXPECT_EQ(0, strtol("0", 0, 10));
  EXPECT_EQ(1, strtol("1", 0, 10));
  EXPECT_EQ(9, strtol("9", 0, 10));
  EXPECT_EQ(12, strtol("12", 0, 10));
  EXPECT_EQ(-12, strtol("-12", 0, 10));
  EXPECT_EQ(31337, strtol("31337", 0, 10));
  EXPECT_EQ(0, strtol("-0", 0, 10));
  EXPECT_EQ(-1, strtol("-1", 0, 10));
  EXPECT_EQ(-9, strtol("-9", 0, 10));
  EXPECT_EQ(-31337, strtol("-31337", 0, 10));
  EXPECT_EQ(LONG_MIN, strtol("-9223372036854775808", 0, 10));
  EXPECT_EQ(LONG_MAX, strtol("9223372036854775807", 0, 10));
  EXPECT_EQ(LONG_MIN, strtol("-9223372036854775809", 0, 10));
  EXPECT_EQ(LONG_MAX, strtol("9223372036854775808", 0, 10));
  EXPECT_EQ(LONG_MIN, strtol("-9223372036854775818", 0, 10));
  EXPECT_EQ(LONG_MAX, strtol("9223372036854775817", 0, 10));
  EXPECT_EQ(123, strtol(" 123", 0, 10));
  EXPECT_EQ(123, strtol(" \t123", 0, 10));
  EXPECT_EQ(-123, strtol(" -123", 0, 10));
  EXPECT_EQ(-123, strtol(" \t-123", 0, 10));
  EXPECT_EQ(0, strtol(" a123", 0, 10));
  EXPECT_EQ(0, strtol(" \ta123", 0, 10));
}

TEST(strtol, testBinary) {
  char *e;
  ASSERT_EQ(85, strtol("0b1010101", 0, 2));
  ASSERT_EQ(85, strtol("0b1010101", 0, 0));
  ASSERT_EQ(85, strtol("0b1010101", &e, 0));
  ASSERT_EQ(0, *e);
}

TEST(strtol, testOutsideLimit) {
  errno = 0;
  EXPECT_EQ(0x7fffffffffffffff, strtol("0x8000000000000000", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000000, strtol("-0x8000000000000001", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000001, strtol("-9223372036854775807", NULL, 0));
  EXPECT_EQ(0, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000000, strtol("-9223372036854775808", NULL, 0));
  EXPECT_EQ(0, errno);
}

TEST(strtoul, ipAddress) {
  char *e;
  const char *p = "1.2.3.4";
  EXPECT_EQ(1, strtoul(p, &e, 0));
  EXPECT_EQ(1, e - p);
}

TEST(strtoul, weirdComma) {
  char *e;
  const char *p = ",2";
  EXPECT_EQ(0, strtoul(p, &e, 0));
  EXPECT_EQ(0, e - p);
}

TEST(strtoul, outsideLimit_doesModulus) {
  EXPECT_EQ(/* python -c 'print(((2**123-1)/123)%(2**64))' */
            9298358801382050408ull,
            strtoul(/* python -c 'print((2**123-1)/123)' */
                    "86453853384384772221385825058884200", 0, 10));
}

TEST(strtol, testHex) {
  EXPECT_EQ(0, strtol("", 0, 16));
  EXPECT_EQ(0, strtol("0", 0, 16));
  EXPECT_EQ(1, strtol("1", 0, 16));
  EXPECT_EQ(9, strtol("9", 0, 16));
  EXPECT_EQ(18, strtol("12", 0, 16));
  EXPECT_EQ(-18, strtol("-12", 0, 16));
  EXPECT_EQ(201527, strtol("31337", 0, 16));
  EXPECT_EQ(15, strtol("f", 0, 16));
  EXPECT_EQ(3735928559, strtol("deadbeef", 0, 16));
  EXPECT_EQ(0, strtol("-0", 0, 16));
  EXPECT_EQ(-1, strtol("-1", 0, 16));
  EXPECT_EQ(-9, strtol("-9", 0, 16));
  EXPECT_EQ(-201527, strtol("-31337", 0, 16));
  EXPECT_EQ(-3735928559, strtol("-DEADBEEF", 0, 16));
  EXPECT_EQ(LONG_MIN, strtol("-8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MAX, strtol("7fffffffffffffff", 0, 16));
  EXPECT_EQ(LONG_MIN, strtol("-8000000000000001", 0, 16));
  EXPECT_EQ(LONG_MAX, strtol("8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MIN, strtol("-0x8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MAX, strtol("0x7fffffffffffffff", 0, 16));
  EXPECT_EQ(LONG_MIN, strtol("-0x800000000000000a", 0, 16));
  EXPECT_EQ(LONG_MAX, strtol("0x8000000000000009", 0, 16));
  EXPECT_EQ(291, strtol(" 123", 0, 16));
  EXPECT_EQ(291, strtol(" \t123", 0, 16));
  EXPECT_EQ(-291, strtol(" -123", 0, 16));
  EXPECT_EQ(-291, strtol(" \t-123", 0, 16));
  EXPECT_EQ(0, strtol(" z123", 0, 16));
  EXPECT_EQ(0, strtol(" \tz123", 0, 16));
}

TEST(strtol, testOctal) {
  EXPECT_EQ(0, strtol("", 0, 8));
  EXPECT_EQ(0, strtol("0", 0, 8));
  EXPECT_EQ(1, strtol("1", 0, 8));
  EXPECT_EQ(7, strtol("7", 0, 8));
  EXPECT_EQ(10, strtol("12", 0, 8));
  EXPECT_EQ(-10, strtol("-12", 0, 8));
  EXPECT_EQ(13023, strtol("31337", 0, 8));
  EXPECT_EQ(0, strtol("-0", 0, 8));
  EXPECT_EQ(-1, strtol("-1", 0, 8));
  EXPECT_EQ(-7, strtol("-7", 0, 8));
  EXPECT_EQ(-13023, strtol("-31337", 0, 8));
  EXPECT_EQ(LONG_MIN, strtol("-1000000000000000000000", 0, 8));
  EXPECT_EQ(LONG_MAX, strtol("777777777777777777777", 0, 8));
  EXPECT_EQ(LONG_MIN, strtol("-01000000000000000000000", 0, 0));
  EXPECT_EQ(LONG_MAX, strtol("0777777777777777777777", 0, 0));
  EXPECT_EQ(1152921504606846975, strtol("077777777777777777777", 0, 0));
  EXPECT_EQ(-144115188075855872, strtol("-010000000000000000000", 0, 0));
}

TEST(strtol, testBase36) {
  EXPECT_EQ(35, strtol("z", 0, 36));
  EXPECT_EQ(5089507, strtol("31337", 0, 36));
  EXPECT_EQ(29234652, strtol("hello", 0, 36));
  EXPECT_EQ(29234652, strtol("HELLO", 0, 36));
}

TEST(strtol, testEndPtr) {
  char *e;
  const char *p;
  p = "1";
  ASSERT_EQ(1, strtol(p, &e, 2));
  ASSERT_EQ(1, e - p);
  p = "";
  ASSERT_EQ(0, strtol(p, &e, 2));
  ASSERT_EQ(0, e - p);
}

TEST(wcstol, testEndPtr) {
  wchar_t *e;
  const wchar_t *p;
  p = L"1";
  ASSERT_EQ(1, wcstol(p, &e, 2));
  ASSERT_EQ(1, e - p);
  p = L"";
  ASSERT_EQ(0, wcstol(p, &e, 2));
  ASSERT_EQ(0, e - p);
}

TEST(strtoul, testEndPtr) {
  char *e;
  const char *p;
  p = "1";
  ASSERT_EQ(1, strtoul(p, &e, 2));
  ASSERT_EQ(1, e - p);
  p = "";
  ASSERT_EQ(0, strtoul(p, &e, 2));
  ASSERT_EQ(0, e - p);
}

TEST(wcstoul, testEndPtr) {
  wchar_t *e;
  const wchar_t *p;
  p = L"1";
  ASSERT_EQ(1, wcstoul(p, &e, 2));
  ASSERT_EQ(1, e - p);
  p = L"";
  ASSERT_EQ(0, wcstoul(p, &e, 2));
  ASSERT_EQ(0, e - p);
}

TEST(strtoimax, testZero) {
  EXPECT_EQ(0, strtoimax("0", NULL, 0));
}

TEST(strtoimax, testDecimal) {
  EXPECT_EQ(-123, strtoimax("-123", NULL, 0));
}

TEST(strtoimax, testHex) {
  EXPECT_EQ(-255, strtoimax("-0xff", NULL, 0));
  EXPECT_EQ(255, strtoimax("0xff", NULL, 16));
}

TEST(strtoimax, testOctal) {
  EXPECT_EQ(-123, strtoimax("-0173", NULL, 0));
  EXPECT_EQ(123, strtoimax("0173", NULL, 8));
}

TEST(strtoimax, testBinary) {
  EXPECT_EQ(-42, strtoimax("-0b101010", NULL, 0));
  EXPECT_EQ(42, strtoimax("0b101010", NULL, 2));
  ASSERT_EQ(85, strtoimax("0b1010101", 0, 2));
  ASSERT_EQ(85, strtoimax("0b1010101", 0, 0));
}

TEST(strtoimax, testEndPtr) {
  char *e;
  const char *p = "1";
  ASSERT_EQ(1, strtoimax(p, &e, 2));
  ASSERT_EQ(1, e - p);
}

TEST(strtoimax, testLimits) {
  EXPECT_EQ(
      ((uintmax_t)0xffffffffffffffff) << 64 | (uintmax_t)0xffffffffffffffff,
      strtoimax("-1", NULL, 0));
  EXPECT_EQ(
      ((uintmax_t)0x7fffffffffffffff) << 64 | (uintmax_t)0xffffffffffffffff,
      strtoimax("0x7fffffffffffffffffffffffffffffff", NULL, 0));
}

TEST(strtoimax, testOutsideLimit) {
  errno = 0;
  EXPECT_EQ(
      ((uintmax_t)0x7fffffffffffffff) << 64 | (uintmax_t)0xffffffffffffffff,
      strtoimax("0x80000000000000000000000000000000", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(((uintmax_t)0x8000000000000000) << 64 | 0x0000000000000000,
            strtoimax("-0x80000000000000000000000000000001", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
}

TEST(strtoul, neghex) {
  errno = 0;
  ASSERT_EQ(-16, (long)strtoul("0xfffffffffffffff0", NULL, 0));
  EXPECT_EQ(0, errno);
}

TEST(strtoumax, testZero) {
  EXPECT_EQ(UINTMAX_MIN, strtoumax("0", NULL, 0));
}
TEST(strtoumax, testDecimal) {
  EXPECT_EQ(123, strtoumax("123", NULL, 0));
  EXPECT_EQ(-123, strtoumax("-123", NULL, 0));
}
TEST(strtoumax, testHex) {
  EXPECT_EQ(255, strtoumax("0xff", NULL, 0));
  EXPECT_EQ(255, strtoumax("0xff", NULL, 16));
}
TEST(strtoumax, testOctal) {
  EXPECT_EQ(123, strtoumax("0173", NULL, 0));
  EXPECT_EQ(123, strtoumax("0173", NULL, 8));
}
TEST(strtoumax, testBinary) {
  EXPECT_EQ(42, strtoumax("0b101010", NULL, 0));
  EXPECT_EQ(42, strtoumax("0b101010", NULL, 2));
}

TEST(strtoumax, testMaximum) {
  EXPECT_EQ(UINTMAX_MAX,
            strtoumax("340282366920938463463374607431768211455", NULL, 0));
  EXPECT_EQ(UINTMAX_MAX,
            strtoumax("0xffffffffffffffffffffffffffffffff", NULL, 0));
}

TEST(strtoumax, testTwosBane) {
  EXPECT_EQ(((uintmax_t)0x8000000000000000) << 64 | 0x0000000000000000,
            strtoumax("0x80000000000000000000000000000000", NULL, 0));
}

TEST(wcstol, test) {
  EXPECT_EQ(0, wcstol(L"", 0, 10));
  EXPECT_EQ(0, wcstol(L"0", 0, 10));
  EXPECT_EQ(1, wcstol(L"1", 0, 10));
  EXPECT_EQ(9, wcstol(L"9", 0, 10));
  EXPECT_EQ(12, wcstol(L"12", 0, 10));
  EXPECT_EQ(-12, wcstol(L"-12", 0, 10));
  EXPECT_EQ(31337, wcstol(L"31337", 0, 10));
  EXPECT_EQ(0, wcstol(L"-0", 0, 10));
  EXPECT_EQ(-1, wcstol(L"-1", 0, 10));
  EXPECT_EQ(-9, wcstol(L"-9", 0, 10));
  EXPECT_EQ(-31337, wcstol(L"-31337", 0, 10));
  EXPECT_EQ(LONG_MIN, wcstol(L"-9223372036854775808", 0, 10));
  EXPECT_EQ(LONG_MAX, wcstol(L"9223372036854775807", 0, 10));
  EXPECT_EQ(LONG_MIN, wcstol(L"-9223372036854775809", 0, 10));
  EXPECT_EQ(LONG_MAX, wcstol(L"9223372036854775808", 0, 10));
  EXPECT_EQ(LONG_MIN, wcstol(L"-9223372036854775818", 0, 10));
  EXPECT_EQ(LONG_MAX, wcstol(L"9223372036854775817", 0, 10));
  EXPECT_EQ(123, wcstol(L" 123", 0, 10));
  EXPECT_EQ(123, wcstol(L" \t123", 0, 10));
  EXPECT_EQ(-123, wcstol(L" -123", 0, 10));
  EXPECT_EQ(-123, wcstol(L" \t-123", 0, 10));
  EXPECT_EQ(0, wcstol(L" a123", 0, 10));
  EXPECT_EQ(0, wcstol(L" \ta123", 0, 10));
}

TEST(wcstol, testOutsideLimit) {
  errno = 0;
  EXPECT_EQ(0x7fffffffffffffff, wcstol(L"0x8000000000000000", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000000, wcstol(L"-0x8000000000000001", NULL, 0));
  EXPECT_EQ(ERANGE, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000001, wcstol(L"-9223372036854775807", NULL, 0));
  EXPECT_EQ(0, errno);
  errno = 0;
  EXPECT_EQ(0x8000000000000000, wcstol(L"-9223372036854775808", NULL, 0));
  EXPECT_EQ(0, errno);
}

TEST(wcstol, testHex) {
  EXPECT_EQ(0, wcstol(L"", 0, 16));
  EXPECT_EQ(0, wcstol(L"0", 0, 16));
  EXPECT_EQ(1, wcstol(L"1", 0, 16));
  EXPECT_EQ(9, wcstol(L"9", 0, 16));
  EXPECT_EQ(18, wcstol(L"12", 0, 16));
  EXPECT_EQ(-18, wcstol(L"-12", 0, 16));
  EXPECT_EQ(201527, wcstol(L"31337", 0, 16));
  EXPECT_EQ(15, wcstol(L"f", 0, 16));
  EXPECT_EQ(3735928559, wcstol(L"deadbeef", 0, 16));
  EXPECT_EQ(0, wcstol(L"-0", 0, 16));
  EXPECT_EQ(-1, wcstol(L"-1", 0, 16));
  EXPECT_EQ(-9, wcstol(L"-9", 0, 16));
  EXPECT_EQ(-201527, wcstol(L"-31337", 0, 16));
  EXPECT_EQ(-3735928559, wcstol(L"-DEADBEEF", 0, 16));
  EXPECT_EQ(LONG_MIN, wcstol(L"-8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MAX, wcstol(L"7fffffffffffffff", 0, 16));
  EXPECT_EQ(LONG_MIN, wcstol(L"-8000000000000001", 0, 16));
  EXPECT_EQ(LONG_MAX, wcstol(L"8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MIN, wcstol(L"-0x8000000000000000", 0, 16));
  EXPECT_EQ(LONG_MAX, wcstol(L"0x7fffffffffffffff", 0, 16));
  EXPECT_EQ(LONG_MIN, wcstol(L"-0x800000000000000a", 0, 16));
  EXPECT_EQ(LONG_MAX, wcstol(L"0x8000000000000009", 0, 16));
  EXPECT_EQ(291, wcstol(L" 123", 0, 16));
  EXPECT_EQ(291, wcstol(L" \t123", 0, 16));
  EXPECT_EQ(-291, wcstol(L" -123", 0, 16));
  EXPECT_EQ(-291, wcstol(L" \t-123", 0, 16));
  EXPECT_EQ(0, wcstol(L" z123", 0, 16));
  EXPECT_EQ(0, wcstol(L" \tz123", 0, 16));
}

TEST(wcstol, testBase36) {
  EXPECT_EQ(35, wcstol(L"z", 0, 36));
  EXPECT_EQ(5089507, wcstol(L"31337", 0, 36));
  EXPECT_EQ(29234652, wcstol(L"hello", 0, 36));
  EXPECT_EQ(29234652, wcstol(L"HELLO", 0, 36));
}

TEST(strtol, testWormsMeat) {
  ASSERT_EQ(0x4e00, strtol("0x4e00", 0, 0));
}

TEST(strtol, testIBM) {
  char *e;
  ASSERT_EQ(1, strtol("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtol("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtol("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(strtoul, testIBM) {
  char *e;
  ASSERT_EQ(1, strtoul("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtoul("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtoul("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(strtoll, testIBM) {
  char *e;
  ASSERT_EQ(1, strtoll("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtoll("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtoll("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(strtoull, testIBM) {
  char *e;
  ASSERT_EQ(1, strtoull("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtoull("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtoull("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(strtoimax, testIBM) {
  char *e;
  ASSERT_EQ(1, strtoimax("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtoimax("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtoimax("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(strtoumax, testIBM) {
  char *e;
  ASSERT_EQ(1, strtoumax("1e-", &e, 10));
  ASSERT_STREQ("e-", e);
  ASSERT_EQ(0, strtoumax("-", &e, 10));
  ASSERT_STREQ("-", e);
  ASSERT_EQ(0, strtoumax("0f", &e, 10));
  ASSERT_STREQ("f", e);
}

TEST(wcstol, testIBM) {
  wchar_t *e;
  ASSERT_EQ(1, wcstol(L"1e-", &e, 10));
  ASSERT_STREQ(L"e-", e);
  ASSERT_EQ(0, wcstol(L"-", &e, 10));
  ASSERT_STREQ(L"-", e);
  ASSERT_EQ(0, wcstol(L"0f", &e, 10));
  ASSERT_STREQ(L"f", e);
}

TEST(wcstoul, testIBM) {
  wchar_t *e;
  ASSERT_EQ(1, wcstoul(L"1e-", &e, 10));
  ASSERT_STREQ(L"e-", e);
  ASSERT_EQ(0, wcstoul(L"-", &e, 10));
  ASSERT_STREQ(L"-", e);
  ASSERT_EQ(0, wcstoul(L"0f", &e, 10));
  ASSERT_STREQ(L"f", e);
}

TEST(wcstoimax, testIBM) {
  wchar_t *e;
  ASSERT_EQ(1, wcstoimax(L"1e-", &e, 10));
  ASSERT_STREQ(L"e-", e);
  ASSERT_EQ(0, wcstoimax(L"-", &e, 10));
  ASSERT_STREQ(L"-", e);
  ASSERT_EQ(0, wcstoimax(L"0f", &e, 10));
  ASSERT_STREQ(L"f", e);
}

TEST(wcstoumax, testIBM) {
  wchar_t *e;
  ASSERT_EQ(1, wcstoumax(L"1e-", &e, 10));
  ASSERT_STREQ(L"e-", e);
  ASSERT_EQ(0, wcstoumax(L"-", &e, 10));
  ASSERT_STREQ(L"-", e);
  ASSERT_EQ(0, wcstoumax(L"0f", &e, 10));
  ASSERT_STREQ(L"f", e);
}

BENCH(atoi, bench) {
  EZBENCH2("atoi 10⁸", donothing, EXPROPRIATE(atoi(VEIL("r", "100000000"))));
  EZBENCH2("strtol 10⁸", donothing,
           EXPROPRIATE(strtol(VEIL("r", "100000000"), 0, 10)));
  EZBENCH2("strtoul 10⁸", donothing,
           EXPROPRIATE(strtol(VEIL("r", "100000000"), 0, 10)));
  EZBENCH2("wcstol 10⁸", donothing,
           EXPROPRIATE(wcstol(VEIL("r", L"100000000"), 0, 10)));
  EZBENCH2("wcstoul 10⁸", donothing,
           EXPROPRIATE(wcstol(VEIL("r", L"100000000"), 0, 10)));
  EZBENCH2("strtoimax 10⁸", donothing,
           EXPROPRIATE(strtoimax(VEIL("r", "100000000"), 0, 10)));
  EZBENCH2("strtoumax 10⁸", donothing,
           EXPROPRIATE(strtoimax(VEIL("r", "100000000"), 0, 10)));
  EZBENCH2("wcstoimax 10⁸", donothing,
           EXPROPRIATE(wcstoimax(VEIL("r", L"100000000"), 0, 10)));
  EZBENCH2("wcstoumax 10⁸", donothing,
           EXPROPRIATE(wcstoimax(VEIL("r", L"100000000"), 0, 10)));
}
