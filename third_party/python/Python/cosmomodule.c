/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=4 sts=4 sw=4 fenc=utf-8                                :vi│
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
#define PY_SSIZE_T_CLEAN
#include "dsp/scale/cdecimate2xuint8x8.h"
#include "libc/bits/popcnt.h"
#include "libc/dce.h"
#include "libc/macros.internal.h"
#include "libc/math.h"
#include "libc/mem/mem.h"
#include "libc/nexgen32e/crc32.h"
#include "libc/nexgen32e/rdtsc.h"
#include "libc/nexgen32e/rdtscp.h"
#include "libc/runtime/runtime.h"
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/import.h"
#include "third_party/python/Include/longobject.h"
#include "third_party/python/Include/methodobject.h"
#include "third_party/python/Include/modsupport.h"
#include "third_party/python/Include/moduleobject.h"
#include "third_party/python/Include/pyerrors.h"
#include "third_party/python/Include/pymacro.h"
#include "third_party/python/Include/pyport.h"
#include "third_party/python/Include/yoink.h"
#include "third_party/xed/x86.h"
/* clang-format off */

PYTHON_PROVIDE("cosmo");
PYTHON_PROVIDE("cosmo.exit1");
PYTHON_PROVIDE("cosmo.rdtsc");
PYTHON_PROVIDE("cosmo.crc32c");
PYTHON_PROVIDE("cosmo.syscount");
PYTHON_PROVIDE("cosmo.popcount");
PYTHON_PROVIDE("cosmo.decimate");
PYTHON_PROVIDE("cosmo.getcpucore");
PYTHON_PROVIDE("cosmo.getcpunode");
PYTHON_PROVIDE("cosmo.ftrace");

PyDoc_STRVAR(cosmo_doc,
"Cosmopolitan Libc Module\n\
\n\
This module exposes low-level utilities from the Cosmopolitan library.\n\
\n\
Static objects:\n\
\n\
MODE -- make build mode, e.g. \"\", \"tiny\", \"opt\", \"rel\", etc.\n\
IMAGE_BASE_VIRTUAL -- base address of actually portable executable image\n\
kernel -- o/s platform, e.g. \"linux\", \"xnu\", \"metal\", \"nt\", etc.\n\
kStartTsc -- the rdtsc() value at process creation.");

PyDoc_STRVAR(syscount_doc,
"syscount($module)\n\
--\n\n\
Returns number of SYSCALL instructions issued to kernel by C library.\n\
\n\
Context switching from userspace to kernelspace is expensive! So it is\n\
useful to be able to know how many times that's happening in your app.\n\
\n\
This value currently isn't meaningful on Windows NT, where it currently\n\
tracks the number of POSIX calls that were attempted, but have not been\n\
polyfilled yet.");

static PyObject *
cosmo_syscount(PyObject *self, PyObject *noargs)
{
    return PyLong_FromSize_t(g_syscount);
}

PyDoc_STRVAR(rdtsc_doc,
"rdtsc($module)\n\
--\n\n\
Returns CPU timestamp counter.");

static PyObject *
cosmo_rdtsc(PyObject *self, PyObject *noargs)
{
    return PyLong_FromUnsignedLong(rdtsc());
}

PyDoc_STRVAR(getcpucore_doc,
"getcpucore($module)\n\
--\n\n\
Returns 0-indexed CPU core on which process is currently scheduled.");

static PyObject *
cosmo_getcpucore(PyObject *self, PyObject *noargs)
{
    return PyLong_FromUnsignedLong(TSC_AUX_CORE(rdpid()));
}

PyDoc_STRVAR(getcpunode_doc,
"getcpunode($module)\n\
--\n\n\
Returns 0-indexed NUMA node on which process is currently scheduled.");

static PyObject *
cosmo_getcpunode(PyObject *self, PyObject *noargs)
{
    return PyLong_FromUnsignedLong(TSC_AUX_NODE(rdpid()));
}

PyDoc_STRVAR(ftrace_doc,
"ftrace($module)\n\
--\n\n\
Enables logging of C function calls to stderr, e.g.\n\
\n\
    cosmo.ftrace()\n\
    WeirdFunction()\n\
    cosmo.exit1()\n\
\n\
Please be warned this prints massive amount of text. In order for it\n\
to work, the concomitant .com.dbg binary needs to be present.");

static PyObject *
cosmo_ftrace(PyObject *self, PyObject *noargs)
{
    ftrace_install();
    Py_RETURN_NONE;
}

PyDoc_STRVAR(crc32c_doc,
"crc32c($module, bytes, init=0)\n\
--\n\n\
Computes 32-bit Castagnoli Cyclic Redundancy Check.\n\
\n\
Used by ISCSI, TensorFlow, etc.\n\
Similar to zlib.crc32().");

static PyObject *
cosmo_crc32c(PyObject *self, PyObject *args)
{
    Py_ssize_t n;
    Py_buffer data;
    unsigned crc, init = 0;
    if (!PyArg_ParseTuple(args, "y*|I:crc32c", &data, &init)) return 0;
    crc = crc32c(init, data.buf, data.len);
    PyBuffer_Release(&data);
    return PyLong_FromUnsignedLong(crc);
}

PyDoc_STRVAR(decimate_doc,
"decimate($module, bytes)\n\
--\n\n\
Shrinks byte buffer in half using John Costella's magic kernel.\n\
\n\
This downscales data 2x using an eight-tap convolution, e.g.\n\
\n\
    >>> cosmo.decimate(b'\\xff\\xff\\x00\\x00\\xff\\xff\\x00\\x00\\xff\\xff\\x00\\x00')\n\
    b'\\xff\\x00\\xff\\x00\\xff\\x00'\n\
\n\
This is very fast if SSSE3 is available (Intel 2004+ / AMD 2011+).");

static PyObject *
cosmo_decimate(PyObject *self, PyObject *args)
{
    Py_ssize_t n;
    PyObject *buf;
    Py_buffer data;
    if (!PyArg_ParseTuple(args, "y*:decimate", &data)) return 0;
    if ((buf = PyBytes_FromStringAndSize(0, (n = ROUNDUP(data.len, 16))))) {
        memcpy(PyBytes_AS_STRING(buf), data.buf, data.len);
        memset(PyBytes_AS_STRING(buf) + data.len, 0, n - data.len);
        cDecimate2xUint8x8(n, (void *)PyBytes_AS_STRING(buf),
                           (signed char[8]){-1, -3, 3, 17, 17, 3, -3, -1});
        _PyBytes_Resize(&buf, (data.len + 1) >> 1);
    }
    PyBuffer_Release(&data);
    return buf;
}

PyDoc_STRVAR(popcount_doc,
"popcount($module, bytes)\n\
--\n\n\
Returns population count of byte sequence, e.g.\n\
\n\
    >>> cosmo.popcount(b'\\xff\\x00\\xff')\n\
    16\n\
\n\
The population count is the number of bits that are set to one.\n\
It does the same thing as `Long.bit_count()` but for data buffers.\n\
This goes 30gbps on Nehalem (Intel 2008+) so it's quite fast.");

static PyObject *
cosmo_popcount(PyObject *self, PyObject *args)
{
    Py_ssize_t n;
    const char *p;
    if (!PyArg_ParseTuple(args, "y#:popcount", &p, &n)) return 0;
    return PyLong_FromSize_t(_countbits(p, n));
}

PyDoc_STRVAR(exit1_doc,
"exit1($module)\n\
--\n\n\
Calls _Exit(1).\n\
\n\
This function is intended to abruptly end the process with less\n\
function trace output compared to os._exit(1).");

static PyObject *
cosmo_exit1(PyObject *self, PyObject *args)
{
    _Exit(1);
}

static PyMethodDef cosmo_methods[] = {
    {"exit1", cosmo_exit1, METH_NOARGS, exit1_doc},
    {"rdtsc", cosmo_rdtsc, METH_NOARGS, rdtsc_doc},
    {"crc32c", cosmo_crc32c, METH_VARARGS, crc32c_doc},
    {"syscount", cosmo_syscount, METH_NOARGS, syscount_doc},
    {"popcount", cosmo_popcount, METH_VARARGS, popcount_doc},
    {"decimate", cosmo_decimate, METH_VARARGS, decimate_doc},
    {"getcpucore", cosmo_getcpucore, METH_NOARGS, getcpucore_doc},
    {"getcpunode", cosmo_getcpunode, METH_NOARGS, getcpunode_doc},
#ifdef __PG__
    {"ftrace", cosmo_ftrace, METH_NOARGS, ftrace_doc},
#endif
    {0},
};

static struct PyModuleDef cosmomodule = {
    PyModuleDef_HEAD_INIT,
    "cosmo",
    cosmo_doc,
    -1,
    cosmo_methods,
};

static const char *
GetKernelName(void) {
    if (IsLinux()) {
        return "linux";
    } else if (IsXnu()) {
        return "xnu";
    } else if (IsMetal()) {
        return "metal";
    } else if (IsWindows()) {
        return "nt";
    } else if (IsFreebsd()) {
        return "freebsd";
    } else if (IsOpenbsd()) {
        return "openbsd";
    } else if (IsNetbsd()) {
        return "netbsd";
    } else {
        return "wut";
    }
}

PyMODINIT_FUNC
PyInit_cosmo(void)
{
    PyObject *m;
    if (!(m = PyModule_Create(&cosmomodule))) return 0;
    PyModule_AddStringConstant(m, "MODE", MODE);
    PyModule_AddIntConstant(m, "IMAGE_BASE_VIRTUAL", IMAGE_BASE_VIRTUAL);
    PyModule_AddStringConstant(m, "kernel", GetKernelName());
    PyModule_AddIntConstant(m, "kStartTsc", kStartTsc);
    return !PyErr_Occurred() ? m : 0;
}

_Section(".rodata.pytab.1") const struct _inittab _PyImport_Inittab_cosmo = {
    "cosmo",
    PyInit_cosmo,
};
