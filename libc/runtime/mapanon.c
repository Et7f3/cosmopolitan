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
#include "libc/bits/weaken.h"
#include "libc/calls/calls.h"
#include "libc/errno.h"
#include "libc/nexgen32e/bsr.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

/**
 * Helper function for allocating anonymous mapping.
 *
 * This function is equivalent to:
 *
 *     mmap(NULL, mapsize, PROT_READ | PROT_WRITE,
 *          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
 *
 * Except it offers a small saving on code size.
 */
void *mapanon(size_t size) {
  void *m;
  if (!size || bsrl(size) >= 40) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  m = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (m == MAP_FAILED) {
    if (weaken(__oom_hook)) {
      weaken(__oom_hook)(size);
    }
  }
  return m;
}
