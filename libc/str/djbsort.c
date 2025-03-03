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
#include "libc/alg/alg.h"
#include "libc/dce.h"
#include "libc/intrin/asan.internal.h"
#include "libc/nexgen32e/bsr.h"
#include "libc/nexgen32e/nexgen32e.h"
#include "libc/nexgen32e/x86feature.h"

void djbsort_avx2(int32_t *, long);

static noinline void intsort(int *x, size_t n, size_t t) {
  int a, b, c;
  size_t i, p, q;
  for (p = t; p > 0; p >>= 1) {
    for (i = 0; i < n - p; ++i) {
      if (!(i & p)) {
        a = x[i + 0];
        b = x[i + p];
        if (a > b) c = a, a = b, b = c;
        x[i + 0] = a;
        x[i + p] = b;
      }
    }
    for (q = t; q > p; q >>= 1) {
      for (i = 0; i < n - q; ++i) {
        if (!(i & p)) {
          a = x[i + p];
          b = x[i + q];
          if (a > b) c = a, a = b, b = c;
          x[i + p] = a;
          x[i + q] = b;
        }
      }
    }
  }
}

/**
 * D.J. Bernstein's outrageously fast integer sorting algorithm.
 */
void djbsort(int32_t *a, size_t n) {
  size_t m;
  if (IsAsan()) {
    if (__builtin_mul_overflow(n, 4, &m)) m = -1;
    __asan_check(a, m);
  }
  if (n > 1) {
    if (X86_HAVE(AVX2)) {
      djbsort_avx2(a, n);
    } else {
      intsort(a, n, 1ul << bsrl(n - 1));
    }
  }
}
