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
#include "libc/assert.h"
#include "libc/bits/bits.h"
#include "libc/calls/calls.h"
#include "libc/dce.h"
#include "libc/elf/def.h"
#include "libc/elf/elf.h"
#include "libc/errno.h"
#include "libc/limits.h"
#include "libc/macros.internal.h"
#include "libc/runtime/internal.h"
#include "libc/runtime/runtime.h"
#include "libc/runtime/symbols.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

/**
 * Maps debuggable binary into memory and indexes symbol addresses.
 *
 * @return object freeable with CloseSymbolTable(), or NULL w/ errno
 */
struct SymbolTable *OpenSymbolTable(const char *filename) {
  int fd;
  void *map;
  long *stp;
  struct stat st;
  size_t n, m, tsz;
  unsigned i, j, x;
  const Elf64_Ehdr *elf;
  const char *name_base;
  struct SymbolTable *t;
  const Elf64_Sym *symtab, *sym;
  ptrdiff_t names_offset, name_base_offset, stp_offset;
  map = MAP_FAILED;
  if ((fd = open(filename, O_RDONLY)) == -1) return 0;
  if (fstat(fd, &st) == -1) goto SystemError;
  if (st.st_size > INT_MAX) goto RaiseE2big;
  if (st.st_size < 64) goto RaiseEnoexec;
  elf = map = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map == MAP_FAILED) goto SystemError;
  if (READ32LE(map) != READ32LE("\177ELF")) goto RaiseEnoexec;
  if (!(name_base = GetElfStrs(map, st.st_size, &m))) goto RaiseEnobufs;
  if (!(symtab = GetElfSymbolTable(map, st.st_size, &n))) goto RaiseEnobufs;
  tsz = 0;
  tsz += sizeof(struct SymbolTable);
  tsz += sizeof(struct Symbol) * n;
  names_offset = tsz;
  tsz += sizeof(unsigned) * n;
  name_base_offset = tsz;
  tsz += m;
  tsz = ROUNDUP(tsz, FRAMESIZE);
  stp_offset = tsz;
  tsz += sizeof(const Elf64_Sym *) * n;
  tsz = ROUNDUP(tsz, FRAMESIZE);
  t = mmap(0, tsz, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (t == MAP_FAILED) goto SystemError;
  t->mapsize = tsz;
  t->names = (unsigned *)((char *)t + names_offset);
  t->name_base = (char *)((char *)t + name_base_offset);
  GetElfVirtualAddressRange(elf, st.st_size, &t->addr_base, &t->addr_end);
  memcpy(t->name_base, name_base, m);
  --t->addr_end;
  stp = (long *)((char *)t + stp_offset);
  for (m = i = 0; i < n; ++i) {
    sym = symtab + i;
    if (!(sym->st_size > 0 && (ELF64_ST_TYPE(sym->st_info) == STT_FUNC ||
                               ELF64_ST_TYPE(sym->st_info) == STT_OBJECT))) {
      continue;
    }
    if (sym->st_value > t->addr_end) continue;
    if (sym->st_value < t->addr_base) continue;
    x = sym->st_value - t->addr_base;
    stp[m++] = (unsigned long)x << 32 | i;
  }
  longsort(stp, m);
  for (j = i = 0; i < m; ++i) {
    sym = symtab + (stp[i] & 0x7fffffff);
    x = stp[i] >> 32;
    if (j && x == t->symbols[j - 1].x) --j;
    if (j && t->symbols[j - 1].y >= x) t->symbols[j - 1].y = x - 1;
    t->names[j] = sym->st_name;
    t->symbols[j].x = x;
    if (sym->st_size) {
      t->symbols[j].y = x + sym->st_size - 1;
    } else {
      t->symbols[j].y = t->addr_end - t->addr_base;
    }
    ++j;
  }
  t->count = j;
  munmap(stp, ROUNDUP(sizeof(const Elf64_Sym *) * n, FRAMESIZE));
  munmap(map, st.st_size);
  close(fd);
  return t;
RaiseE2big:
  errno = E2BIG;
  goto SystemError;
RaiseEnobufs:
  errno = ENOBUFS;
  goto SystemError;
RaiseEnoexec:
  errno = ENOEXEC;
SystemError:
  if (map != MAP_FAILED) {
    munmap(map, st.st_size);
  }
  close(fd);
  return 0;
}
