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
#include "libc/bits/safemacros.internal.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/fmt.h"
#include "libc/fmt/itoa.h"
#include "libc/macros.internal.h"
#include "libc/nt/enum/formatmessageflags.h"
#include "libc/nt/process.h"
#include "libc/nt/runtime.h"
#include "libc/str/str.h"

struct Error {
  int x;
  int s;
};

extern const struct Error kErrorNames[];
extern const struct Error kErrorNamesLong[];

static const char *GetErrorName(long x) {
  int i;
  if (x) {
    for (i = 0; kErrorNames[i].x; ++i) {
      if (x == *(const long *)((uintptr_t)kErrorNames + kErrorNames[i].x)) {
        return (const char *)((uintptr_t)kErrorNames + kErrorNames[i].s);
      }
    }
  }
  return "EUNKNOWN";
}

static const char *GetErrorNameLong(long x) {
  int i;
  if (x) {
    for (i = 0; kErrorNamesLong[i].x; ++i) {
      if (x ==
          *(const long *)((uintptr_t)kErrorNamesLong + kErrorNamesLong[i].x)) {
        return (const char *)((uintptr_t)kErrorNamesLong +
                              kErrorNamesLong[i].s);
      }
    }
  }
  return "EUNKNOWN[No error information]";
}

/**
 * Converts errno value to string.
 * @return 0 on success, or error code
 */
int strerror_r(int err, char *buf, size_t size) {
  char *p;
  const char *s;
  err &= 0xFFFF;
  if (IsTiny()) {
    s = GetErrorName(err);
  } else {
    s = GetErrorNameLong(err);
  }
  p = buf;
  if (strlen(s) + 1 + 5 + 1 + 1 <= size) {
    p = stpcpy(p, s);
    *p++ = '[';
    p += uint64toarray_radix10(err, p);
    *p++ = ']';
  }
  if (p - buf < size) {
    *p++ = '\0';
  }
  return 0;
}
