#ifndef COSMOPOLITAN_LIBC_SYSV_CONSTS_CLOSE_H_
#define COSMOPOLITAN_LIBC_SYSV_CONSTS_CLOSE_H_
#include "libc/runtime/symbolic.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

extern const long CLOSE_RANGE_UNSHARE;
extern const long CLOSE_RANGE_CLOEXEC;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define CLOSE_RANGE_UNSHARE SYMBOLIC(CLOSE_RANGE_UNSHARE)
#define CLOSE_RANGE_CLOEXEC SYMBOLIC(CLOSE_RANGE_CLOEXEC)

#endif /* COSMOPOLITAN_LIBC_SYSV_CONSTS_CLOSE_H_ */
