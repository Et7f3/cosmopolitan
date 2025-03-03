#ifndef COSMOPOLITAN_THIRD_PARTY_PYTHON_INCLUDE_EZPRINT_H_
#define COSMOPOLITAN_THIRD_PARTY_PYTHON_INCLUDE_EZPRINT_H_
#include "libc/calls/calls.h"
#include "libc/log/libfatal.internal.h"
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/bytesobject.h"
#include "third_party/python/Include/pyerrors.h"
#include "third_party/python/Include/unicodeobject.h"
#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

static void EzPrint(PyObject *x, const char *s) {
  PyObject *u, *r, *t;
  __printf("%s = ", s);
  if (!s) {
    __printf("NULL");
  } else {
    t = PyObject_Type(x);
    r = PyObject_Repr(t);
    u = PyUnicode_AsUTF8String(r);
    __printf("%S ", PyBytes_GET_SIZE(u), PyBytes_AS_STRING(u));
    Py_DECREF(u);
    Py_DECREF(r);
    Py_DECREF(t);
    r = PyObject_Repr(x);
    u = PyUnicode_AsUTF8String(r);
    __printf("%S", PyBytes_GET_SIZE(u), PyBytes_AS_STRING(u));
    Py_DECREF(u);
    Py_DECREF(r);
  }
  __printf("\n");
}

#define EZPRINT(x) EzPrint(x, #x)

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_THIRD_PARTY_PYTHON_INCLUDE_EZPRINT_H_ */
