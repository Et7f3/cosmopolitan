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
#include "libc/assert.h"
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/internal.h"
#include "libc/calls/struct/iovec.h"
#include "libc/str/str.h"
#include "libc/zip.h"
#include "libc/zipos/zipos.internal.h"

static size_t GetIovSize(const struct iovec *iov, size_t iovlen) {
  size_t i, r;
  for (r = i = 0; i < iovlen; ++i) r += iov[i].iov_len;
  return r;
}

/**
 * Reads data from zip store object.
 *
 * @return [1..size] bytes on success, 0 on EOF, or -1 w/ errno; with
 *     exception of size==0, in which case return zero means no error
 * @asyncsignalsafe
 */
ssize_t __zipos_read(struct ZiposHandle *h, const struct iovec *iov,
                     size_t iovlen, ssize_t opt_offset) {
  size_t i, b, x, y;
  x = y = opt_offset != -1 ? opt_offset : h->pos;
  for (i = 0; i < iovlen && y < h->size; ++i, y += b) {
    b = min(iov[i].iov_len, h->size - y);
    memcpy(iov[i].iov_base, h->mem + y, b);
  }
  if (opt_offset == -1) h->pos = y;
  ZTRACE("__zipos_read(%S, cap=%d, off=%d) -> %d",
         ZIP_CFILE_NAMESIZE(__zipos_get()->map + h->cfile),
         ZIP_CFILE_NAME(__zipos_get()->map + h->cfile), GetIovSize(iov, iovlen),
         x, y - x);
  return y - x;
}
