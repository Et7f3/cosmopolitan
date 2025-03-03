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
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/fmt/conv.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/runtime/gc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/x/x.h"
#include "libc/zip.h"
#include "tool/decode/lib/asmcodegen.h"
#include "tool/decode/lib/disassemblehex.h"
#include "tool/decode/lib/zipnames.h"

char *FormatDosDate(uint16_t dosdate) {
  return xasprintf("%04u-%02u-%02u", ((dosdate >> 9) & 0b1111111) + 1980,
                   (dosdate >> 5) & 0b1111, dosdate & 0b11111);
}

char *FormatDosTime(uint16_t dostime) {
  return xasprintf("%02u:%02u:%02u", (dostime >> 11) & 0b11111,
                   (dostime >> 5) & 0b111111, (dostime << 1) & 0b111110);
}

void ShowGeneralFlag(uint16_t generalflag) {
  puts("\
/	              ┌─utf8\n\
/	              │    ┌─strong encryption\n\
/	              │    │┌─compressed patch data\n\
/	              │    ││ ┌─crc and size go after file content\n\
/	              │    ││ │┌─{normal,max,fast,superfast}\n\
/	              │    ││ ││ ┌─encrypted\n\
/	          rrrr│uuuu││r│├┐│");
  show(".short", format(b1, "0b%016b", generalflag), "generalflag");
}

void ShowCompressionMethod(uint16_t compressmethod) {
  show(".short",
       firstnonnull(findnamebyid(kZipCompressionNames, compressmethod),
                    format(b1, "%hu", compressmethod)),
       "compressionmethod");
}

void ShowTimestamp(uint16_t time, uint16_t date) {
  show(".short", format(b1, "%#04hx", time),
       gc(xasprintf("%s (%s)", "lastmodifiedtime", gc(FormatDosTime(time)))));
  show(".short", format(b1, "%#04hx", date),
       gc(xasprintf("%s (%s)", "lastmodifieddate", gc(FormatDosDate(date)))));
}

void ShowNtfs(uint8_t *ntfs, size_t n) {
  struct timespec mtime, atime, ctime;
  mtime = WindowsTimeToTimeSpec(READ64LE(ntfs + 8));
  atime = WindowsTimeToTimeSpec(READ64LE(ntfs + 16));
  ctime = WindowsTimeToTimeSpec(READ64LE(ntfs + 24));
  show(".long", gc(xasprintf("%d", READ32LE(ntfs))), "ntfs reserved");
  show(".short", gc(xasprintf("0x%04x", READ16LE(ntfs + 4))),
       "ntfs attribute tag value #1");
  show(".short", gc(xasprintf("%hu", READ16LE(ntfs + 6))),
       "ntfs attribute tag size");
  show(".quad", gc(xasprintf("%lu", READ64LE(ntfs + 8))),
       gc(xasprintf("%s (%s)", "ntfs last modified time",
                    gc(xiso8601(&mtime)))));
  show(".quad", gc(xasprintf("%lu", READ64LE(ntfs + 16))),
       gc(xasprintf("%s (%s)", "ntfs last access time", gc(xiso8601(&atime)))));
  show(".quad", gc(xasprintf("%lu", READ64LE(ntfs + 24))),
       gc(xasprintf("%s (%s)", "ntfs creation time", gc(xiso8601(&ctime)))));
}

void ShowExtendedTimestamp(uint8_t *p, size_t n, bool islocal) {
  int flag;
  if (n) {
    --n;
    flag = *p++;
    show(".byte", gc(xasprintf("0b%03hhb", flag)), "fields present in local");
    if ((flag & 1) && n >= 4) {
      show(".long", gc(xasprintf("%u", READ32LE(p))),
           gc(xasprintf("%s (%s)", "last modified",
                        gc(xiso8601(&(struct timespec){READ32LE(p)})))));
      p += 4;
      n -= 4;
    }
    flag >>= 1;
    if (islocal) {
      if ((flag & 1) && n >= 4) {
        show(".long", gc(xasprintf("%u", READ32LE(p))),
             gc(xasprintf("%s (%s)", "access time",
                          gc(xiso8601(&(struct timespec){READ32LE(p)})))));
        p += 4;
        n -= 4;
      }
      flag >>= 1;
      if ((flag & 1) && n >= 4) {
        show(".long", gc(xasprintf("%u", READ32LE(p))),
             gc(xasprintf("%s (%s)", "creation time",
                          gc(xiso8601(&(struct timespec){READ32LE(p)})))));
        p += 4;
        n -= 4;
      }
    }
  }
}

void ShowZip64(uint8_t *p, size_t n, bool islocal) {
  if (n >= 8) {
    show(".quad", gc(xasprintf("%lu", READ64LE(p))),
         gc(xasprintf("uncompressed size (%,ld)", READ64LE(p))));
  }
  if (n >= 16) {
    show(".quad", gc(xasprintf("%lu", READ64LE(p + 8))),
         gc(xasprintf("compressed size (%,ld)", READ64LE(p + 8))));
  }
  if (n >= 24) {
    show(".quad", gc(xasprintf("%lu", READ64LE(p + 16))),
         gc(xasprintf("lfile hdr offset (%,ld)", READ64LE(p + 16))));
  }
  if (n >= 28) {
    show(".long", gc(xasprintf("%u", READ32LE(p + 24))), "disk number");
  }
}

void ShowInfoZipNewUnixExtra(uint8_t *p, size_t n, bool islocal) {
  if (p[0] == 1 && p[1] == 4 && p[6] == 4) {
    show(".byte", "1", "version");
    show(".byte", "4", "uid length");
    show(".long", gc(xasprintf("%u", READ32LE(p + 2))), "uid");
    show(".byte", "4", "gid length");
    show(".long", gc(xasprintf("%u", READ32LE(p + 7))), "gid");
  } else {
    disassemblehex(p, n, stdout);
  }
}

void ShowExtra(uint8_t *extra, bool islocal) {
  switch (ZIP_EXTRA_HEADERID(extra)) {
    case kZipExtraNtfs:
      ShowNtfs(ZIP_EXTRA_CONTENT(extra), ZIP_EXTRA_CONTENTSIZE(extra));
      break;
    case kZipExtraExtendedTimestamp:
      ShowExtendedTimestamp(ZIP_EXTRA_CONTENT(extra),
                            ZIP_EXTRA_CONTENTSIZE(extra), islocal);
      break;
    case kZipExtraZip64:
      ShowZip64(ZIP_EXTRA_CONTENT(extra), ZIP_EXTRA_CONTENTSIZE(extra),
                islocal);
      break;
    case kZipExtraInfoZipNewUnixExtra:
      ShowInfoZipNewUnixExtra(ZIP_EXTRA_CONTENT(extra),
                              ZIP_EXTRA_CONTENTSIZE(extra), islocal);
      break;
    default:
      disassemblehex(ZIP_EXTRA_CONTENT(extra), ZIP_EXTRA_CONTENTSIZE(extra),
                     stdout);
      break;
  }
}

void ShowExtras(uint8_t *extras, uint16_t extrassize, bool islocal) {
  int i;
  bool first;
  uint8_t *p, *pe;
  if (extrassize) {
    first = true;
    for (p = extras, pe = extras + extrassize, i = 0; p < pe;
         p += ZIP_EXTRA_SIZE(p), ++i) {
      show(".short",
           firstnonnull(findnamebyid(kZipExtraNames, ZIP_EXTRA_HEADERID(p)),
                        gc(xasprintf("0x%04hx", ZIP_EXTRA_HEADERID(p)))),
           gc(xasprintf("%s[%d].%s", "extras", i, "headerid")));
      show(".short", gc(xasprintf("%df-%df", (i + 2) * 10, (i + 1) * 10)),
           gc(xasprintf("%s[%d].%s (%hd %s)", "extras", i, "contentsize",
                        ZIP_EXTRA_CONTENTSIZE(p), "bytes")));
      if (first) {
        first = false;
        printf("%d:", (i + 1) * 10);
      }
      ShowExtra(p, islocal);
      printf("%d:", (i + 2) * 10);
    }
  }
  putchar('\n');
}

void ShowLocalFileHeader(uint8_t *lf, uint16_t idx) {
  printf("\n/\t%s #%hu (%zu %s)\n", "local file", idx + 1,
         ZIP_LFILE_HDRSIZE(lf), "bytes");
  show(".ascii", format(b1, "%`'.*s", 4, lf), "magic");
  show(".byte",
       firstnonnull(findnamebyid(kZipEraNames, ZIP_LFILE_VERSIONNEED(lf)),
                    gc(xasprintf("%d", ZIP_LFILE_VERSIONNEED(lf)))),
       "pkzip version need");
  show(".byte",
       firstnonnull(findnamebyid(kZipOsNames, ZIP_LFILE_OSNEED(lf)),
                    gc(xasprintf("%d", ZIP_LFILE_OSNEED(lf)))),
       "os need");
  ShowGeneralFlag(ZIP_LFILE_GENERALFLAG(lf));
  ShowCompressionMethod(ZIP_LFILE_COMPRESSIONMETHOD(lf));
  ShowTimestamp(ZIP_LFILE_LASTMODIFIEDTIME(lf), ZIP_LFILE_LASTMODIFIEDDATE(lf));
  show(
      ".long",
      format(b1, "%#x", ZIP_LFILE_CRC32(lf)), gc(xasprintf("%s (%#x)", "crc32z", GetZipLfileCompressedSize(lf) /* crc32_z(0, ZIP_LFILE_CONTENT(lf), GetZipLfileCompressedSize(lf)) */)));
  if (ZIP_LFILE_COMPRESSEDSIZE(lf) == 0xFFFFFFFF) {
    show(".long", "0xFFFFFFFF", "compressedsize (zip64)");
  } else {
    show(".long", "3f-2f",
         format(b1, "%s (%u %s)", "compressedsize",
                ZIP_LFILE_COMPRESSEDSIZE(lf), "bytes"));
  }
  show(".long",
       ZIP_LFILE_UNCOMPRESSEDSIZE(lf) == 0xFFFFFFFF
           ? "0xFFFFFFFF"
           : format(b1, "%u", ZIP_LFILE_UNCOMPRESSEDSIZE(lf)),
       "uncompressedsize");
  show(".short", "1f-0f",
       format(b1, "%s (%hu %s)", "namesize", ZIP_LFILE_NAMESIZE(lf), "bytes"));
  show(
      ".short", "2f-1f",
      format(b1, "%s (%hu %s)", "extrasize", ZIP_LFILE_EXTRASIZE(lf), "bytes"));
  printf("0:");
  show(".ascii",
       format(b1, "%`'s",
              gc(strndup(ZIP_LFILE_NAME(lf), ZIP_LFILE_NAMESIZE(lf)))),
       "name");
  printf("1:");
  ShowExtras(ZIP_LFILE_EXTRA(lf), ZIP_LFILE_EXTRASIZE(lf), true);
  printf("2:");
  disassemblehex(ZIP_LFILE_CONTENT(lf), ZIP_LFILE_COMPRESSEDSIZE(lf), stdout);
  printf("3:\n");
}

void DisassembleZip(const char *path, uint8_t *p, size_t n) {
  size_t i, records;
  uint8_t *eocd, *cdir, *cf, *lf;
  CHECK_NOTNULL((eocd = GetZipCdir(p, n)));
  records = GetZipCdirRecords(eocd);
  cdir = p + GetZipCdirOffset(eocd);
  for (i = 0, cf = cdir; i < records; ++i, cf += ZIP_CFILE_HDRSIZE(cf)) {
    CHECK_EQ(kZipCfileHdrMagic, ZIP_CFILE_MAGIC(cf));
    lf = p + GetZipCfileOffset(cf);
    CHECK_EQ(kZipLfileHdrMagic, ZIP_LFILE_MAGIC(lf));
    ShowLocalFileHeader(lf, i);
  }
}

int main(int argc, char *argv[]) {
  int fd;
  uint8_t *map;
  struct stat st;
  showcrashreports();
  CHECK_EQ(2, argc);
  CHECK_NE(-1, (fd = open(argv[1], O_RDONLY)));
  CHECK_NE(-1, fstat(fd, &st));
  CHECK_GE(st.st_size, kZipCdirHdrMinSize);
  CHECK_NE(MAP_FAILED,
           (map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)));
  DisassembleZip(argv[1], map, st.st_size);
  CHECK_NE(-1, munmap(map, st.st_size));
  CHECK_NE(-1, close(fd));
  return 0;
}
