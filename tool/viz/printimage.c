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
#include "dsp/core/half.h"
#include "dsp/core/twixt8.h"
#include "dsp/scale/scale.h"
#include "dsp/tty/quant.h"
#include "dsp/tty/tty.h"
#include "libc/assert.h"
#include "libc/bits/bits.h"
#include "libc/bits/safemacros.internal.h"
#include "libc/calls/calls.h"
#include "libc/calls/ioctl.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/struct/winsize.h"
#include "libc/dce.h"
#include "libc/fmt/conv.h"
#include "libc/limits.h"
#include "libc/log/check.h"
#include "libc/log/log.h"
#include "libc/macros.internal.h"
#include "libc/math.h"
#include "libc/mem/mem.h"
#include "libc/rand/rand.h"
#include "libc/runtime/gc.internal.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/ex.h"
#include "libc/sysv/consts/exit.h"
#include "libc/sysv/consts/fileno.h"
#include "libc/sysv/consts/madv.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/termios.h"
#include "libc/x/x.h"
#include "third_party/getopt/getopt.h"
#include "third_party/stb/stb_image.h"
#include "tool/viz/lib/graphic.h"

static struct Flags {
  const char *out;
  bool subpixel;
  bool unsharp;
  bool dither;
  bool ruler;
  bool trailingnewline;
  long half;
  bool full;
  long width;
  long height;
  enum TtyBlocksSelection blocks;
  enum TtyQuantizationAlgorithm quant;
} g_flags;

static wontreturn void PrintUsage(int rc, FILE *f) {
  fprintf(f, "Usage: %s%s", program_invocation_name, "\
 [FLAGS] [PATH]\n\
\n\
FLAGS\n\
\n\
  -o PATH    output path\n\
  -w INT     manual width\n\
  -h INT     manual height\n\
  -4         unicode blocks\n\
  -a         ansi color mode\n\
  -t         true color mode\n\
  -2         use half blocks\n\
  -3         ibm cp437 blocks\n\
  -f         display full size\n\
  -s         unsharp sharpening\n\
  -x         xterm256 color mode\n\
  -d         hilbert curve dithering\n\
  -r         display pixel ruler on sides\n\
  -p         convert to subpixel layout\n\
  -v         increases verbosity\n\
  -?         shows this information\n\
\n\
EXAMPLES\n\
\n\
  printimage.com -sxd lemurs.jpg  # 256-color dither unsharp\n\
\n\
\n");
  exit(rc);
}

static int ParseNumberOption(const char *arg) {
  long x;
  x = strtol(arg, NULL, 0);
  if (!(1 <= x && x <= INT_MAX)) {
    fprintf(stderr, "invalid flexidecimal: %s\n\n", arg);
    exit(EXIT_FAILURE);
  }
  return x;
}

static void GetOpts(int *argc, char *argv[]) {
  int opt;
  struct winsize ws;
  g_flags.quant = kTtyQuantTrue;
  g_flags.blocks = IsWindows() ? kTtyBlocksCp437 : kTtyBlocksUnicode;
  if (*argc == 2 &&
      (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-help") == 0)) {
    PrintUsage(EXIT_SUCCESS, stdout);
  }
  while ((opt = getopt(*argc, argv, "?vpfrtxads234o:w:h:")) != -1) {
    switch (opt) {
      case 'o':
        g_flags.out = optarg;
        break;
      case 'd':
        g_flags.dither = true;
        break;
      case 's':
        g_flags.unsharp = true;
        break;
      case 'w':
        g_flags.trailingnewline = true;
        g_flags.width = ParseNumberOption(optarg);
        break;
      case 'h':
        g_flags.trailingnewline = true;
        g_flags.height = ParseNumberOption(optarg);
        break;
      case 'f':
        g_flags.full = true;
        break;
      case '2':
        g_flags.half = true;
        break;
      case 'r':
        g_flags.ruler = true;
        break;
      case 'p':
        g_flags.subpixel = true;
        break;
      case 'a':
        g_flags.quant = kTtyQuantAnsi;
        break;
      case 'x':
        g_flags.quant = kTtyQuantXterm256;
        break;
      case 't':
        g_flags.quant = kTtyQuantTrue;
        break;
      case '3':
        g_flags.blocks = kTtyBlocksCp437;
        break;
      case '4':
        g_flags.blocks = kTtyBlocksUnicode;
        break;
      case 'v':
        ++__log_level;
        break;
      case '?':
        PrintUsage(EXIT_SUCCESS, stdout);
      default:
        PrintUsage(EX_USAGE, stderr);
    }
  }
  if (optind == *argc) {
    if (!g_flags.out) g_flags.out = "-";
    argv[(*argc)++] = "-";
  }
  if (!g_flags.full && (!g_flags.width || !g_flags.width)) {
    ws.ws_col = 80;
    ws.ws_row = 24;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != -1 ||
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
      g_flags.width = ws.ws_col * (1 + !g_flags.half);
      g_flags.height = ws.ws_row * 2;
    }
  }
  ttyquantsetup(g_flags.quant, kTtyQuantRgb, g_flags.blocks);
}

static unsigned char ChessBoard(unsigned y, unsigned x, unsigned char a,
                                unsigned char b) {
  return !((y ^ x) & (1u << 2)) ? a : b;
}

static unsigned char AlphaBackground(unsigned y, unsigned x) {
  return ChessBoard(y, x, 255, 200);
}

static unsigned char OutOfBoundsBackground(unsigned y, unsigned x) {
  return ChessBoard(y, x, 40, 80);
}

static void PrintRulerRight(long yn, long xn, long y, long x,
                            bool *inout_didhalfy) {
  if (y == 0) {
    printf("‾0");
  } else if (yn / 2 <= y && y <= yn / 2 + 1 && !*inout_didhalfy) {
    printf("‾%s%s", "yn/2", y % 2 ? "+1" : "");
    *inout_didhalfy = true;
  } else if (y + 1 == yn / 2 && !*inout_didhalfy) {
    printf("⎯yn/2");
    *inout_didhalfy = true;
  } else if (y + 1 == yn) {
    printf("⎯yn");
  } else if (y + 2 == yn) {
    printf("_yn");
  } else if (!(y % 10)) {
    printf("‾%,u", y);
  }
}

static void PrintImageImpl(long syn, long sxn, unsigned char RGB[3][syn][sxn],
                           long y0, long yn, long x0, long xn, long dy,
                           long dx) {
  long y, x;
  bool didhalfy, didfirstx;
  unsigned char a[3], b[3];
  didhalfy = false;
  for (y = y0; y < yn; y += dy) {
    didfirstx = false;
    if (y) printf("\e[0m\n");
    for (x = x0; x < xn; x += dx) {
      a[0] = RGB[0][y][x];
      a[1] = RGB[1][y][x];
      a[2] = RGB[2][y][x];
      if (y + dy < yn && x + dx < xn) {
        b[0] = RGB[0][y + dy][x + dx];
        b[1] = RGB[1][y + dy][x + dx];
        b[2] = RGB[2][y + dy][x + dx];
      } else {
        b[2] = b[1] = b[0] = OutOfBoundsBackground(y + dy, x + dx);
      }
      printf("\e[48;2;%d;%d;%d;38;2;%d;%d;%dm%lc", a[0], a[1], a[2], b[0], b[1],
             b[2], dy > 1 ? u'▄' : u'▐');
      didfirstx = true;
    }
    printf("\e[0m");
    if (g_flags.ruler) {
      PrintRulerRight(yn, xn, y, x, &didhalfy);
    }
  }
  printf("\n");
}

static void PrintImage(long syn, long sxn, unsigned char RGB[3][syn][sxn],
                       long y0, long yn, long x0, long xn) {
  PrintImageImpl(syn, sxn, RGB, y0, yn, x0, xn, 2, 1);
}

static void PrintImageLR(long syn, long sxn, unsigned char RGB[3][syn][sxn],
                         long y0, long yn, long x0, long xn) {
  PrintImageImpl(syn, sxn, RGB, y0, yn, x0, xn, 1, 2);
}

static void *Deblinterlace(long dyn, long dxn, unsigned char dst[3][dyn][dxn],
                           long syn, long sxn, long scn,
                           const unsigned char src[syn][sxn][scn], long y0,
                           long yn, long x0, long xn) {
  long y, x;
  unsigned char c;
  for (y = y0; y < yn; ++y) {
    for (x = x0; x < xn; ++x) {
      switch (scn) {
        case 1:
          c = src[y][x][0];
          dst[0][y][x] = c;
          dst[1][y][x] = c;
          dst[2][y][x] = c;
          break;
        case 2:
          c = twixt8(AlphaBackground(y, x), src[y][x][0], src[y][x][1]);
          dst[0][y][x] = c;
          dst[1][y][x] = c;
          dst[2][y][x] = c;
          break;
        case 3:
          dst[0][y][x] = src[y][x][0];
          dst[1][y][x] = src[y][x][1];
          dst[2][y][x] = src[y][x][2];
          break;
        case 4:
          c = AlphaBackground(y, x);
          dst[0][y][x] = twixt8(c, src[y][x][0], src[y][x][3]);
          dst[1][y][x] = twixt8(c, src[y][x][1], src[y][x][3]);
          dst[2][y][x] = twixt8(c, src[y][x][2], src[y][x][3]);
          break;
      }
    }
  }
  return dst;
}

static void *DeblinterlaceSubpixelBgr(long dyn, long dxn,
                                      unsigned char dst[3][dyn][dxn][3],
                                      long syn, long sxn,
                                      const unsigned char src[syn][sxn][4],
                                      long y0, long yn, long x0, long xn) {
  long y, x;
  for (y = y0; y < yn; ++y) {
    for (x = x0; x < xn; ++x) {
      dst[0][y][x][0] = 0;
      dst[1][y][x][0] = 0;
      dst[2][y][x][0] = src[y][x][2];
      dst[0][y][x][1] = 0;
      dst[1][y][x][1] = src[y][x][1];
      dst[2][y][x][1] = 0;
      dst[0][y][x][2] = src[y][x][0];
      dst[1][y][x][2] = 0;
      dst[2][y][x][2] = 0;
    }
  }
  return dst;
}

static void PrintImageSerious(long yn, long xn, unsigned char RGB[3][yn][xn],
                              struct TtyRgb TTY[yn][xn], char *vt) {
  char *p;
  long y, x;
  struct TtyRgb bg = {0x12, 0x34, 0x56, 0};
  struct TtyRgb fg = {0x12, 0x34, 0x56, 0};
  if (g_flags.unsharp) unsharp(3, yn, xn, RGB, yn, xn);
  if (g_flags.dither) dither(yn, xn, RGB, yn, xn);
  for (y = 0; y < yn; ++y) {
    for (x = 0; x < xn; ++x) {
      TTY[y][x] = rgb2tty(RGB[0][y][x], RGB[1][y][x], RGB[2][y][x]);
    }
  }
  p = ttyraster(vt, (void *)TTY, yn, xn, bg, fg);
  *p++ = '\r';
  if (g_flags.trailingnewline) *p++ = '\n';
  p = stpcpy(p, "\e[0m");
  ttywrite(STDOUT_FILENO, vt, p - vt);
}

static void ProcessImage(long yn, long xn, unsigned char RGB[3][yn][xn]) {
  if (g_flags.half) {
    if (g_flags.subpixel) {
      PrintImageLR(yn, xn * 3, RGB, 0, yn, 0, xn * 3);
    } else {
      PrintImage(yn, xn, RGB, 0, yn, 0, xn);
    }
  } else {
    PrintImageSerious(
        yn, xn, RGB, gc(memalign(32, sizeof(struct TtyRgb) * yn * xn)),
        gc(memalign(32, ((yn * xn * strlen("\e[48;2;255;48;2;255m▄")) +
                         (yn * strlen("\e[0m\r\n")) + 128))));
  }
}

void WithImageFile(const char *path,
                   void fn(long yn, long xn, unsigned char RGB[3][yn][xn])) {
  struct stat st;
  void *map, *data, *data2;
  int fd, yn, xn, cn, dyn, dxn, syn, sxn;
  CHECK_NE(-1, (fd = open(path, O_RDONLY)), "%s", path);
  CHECK_NE(-1, fstat(fd, &st));
  CHECK_GT(st.st_size, 0);
  CHECK_LE(st.st_size, INT_MAX);
  fadvise(fd, 0, 0, MADV_WILLNEED | MADV_SEQUENTIAL);
  CHECK_NE(MAP_FAILED,
           (map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)));
  CHECK_NOTNULL(
      (data = gc(stbi_load_from_memory(map, st.st_size, &xn, &yn, &cn, 0))),
      "%s", path);
  CHECK_NE(-1, munmap(map, st.st_size));
  CHECK_NE(-1, close(fd));
  if (g_flags.subpixel) {
    data = DeblinterlaceSubpixelBgr(yn, xn, gc(memalign(32, yn * xn * 4 * 3)),
                                    yn, xn, data, 0, yn, 0, xn);
    xn *= 3;
  } else {
    data = Deblinterlace(yn, xn, gc(memalign(32, yn * xn * 4)), yn, xn, cn,
                         data, 0, yn, 0, xn);
    cn = 3;
  }
  if (g_flags.height && g_flags.width) {
    syn = yn;
    sxn = xn;
    dyn = g_flags.height;
    dxn = g_flags.width;
#if 1
    while (HALF(syn) > dyn || HALF(sxn) > dxn) {
      if (HALF(sxn) > dxn) {
        Magikarp2xX(yn, xn, data, syn, sxn);
        Magikarp2xX(yn, xn, (char *)data + yn * xn, syn, sxn);
        Magikarp2xX(yn, xn, (char *)data + yn * xn * 2, syn, sxn);
        sxn = HALF(sxn);
      }
      if (HALF(syn) > dyn) {
        Magikarp2xY(yn, xn, data, syn, sxn);
        Magikarp2xY(yn, xn, (char *)data + yn * xn, syn, sxn);
        Magikarp2xY(yn, xn, (char *)data + yn * xn * 2, syn, sxn);
        syn = HALF(syn);
      }
    }
#endif
    data = EzGyarados(3, dyn, dxn, gc(memalign(32, dyn * dxn * 3)), cn, yn, xn,
                      data, 0, cn, dyn, dxn, syn, sxn, 0, 0, 0, 0);
    yn = dyn;
    xn = dxn;
  }
  fn(yn, xn, data);
}

int main(int argc, char *argv[]) {
  int i;
  GetOpts(&argc, argv);
  stbi_set_unpremultiply_on_load(true);
  for (i = optind; i < argc; ++i) {
    WithImageFile(argv[i], ProcessImage);
  }
  return 0;
}
