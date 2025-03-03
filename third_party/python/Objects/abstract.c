/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=4 sts=4 sw=4 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/log/log.h"
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/bytearrayobject.h"
#include "third_party/python/Include/ceval.h"
#include "third_party/python/Include/dictobject.h"
#include "third_party/python/Include/floatobject.h"
#include "third_party/python/Include/funcobject.h"
#include "third_party/python/Include/iterobject.h"
#include "third_party/python/Include/listobject.h"
#include "third_party/python/Include/longintrepr.h"
#include "third_party/python/Include/methodobject.h"
#include "third_party/python/Include/modsupport.h"
#include "third_party/python/Include/object.h"
#include "third_party/python/Include/pyerrors.h"
#include "third_party/python/Include/pymacro.h"
#include "third_party/python/Include/pymem.h"
#include "third_party/python/Include/sliceobject.h"
#include "third_party/python/Include/structmember.h"
#include "third_party/python/Include/tupleobject.h"
#include "third_party/python/Include/warnings.h"
/* clang-format off */

/* Abstract Object Interface (many thanks to Jim Fulton) */

/* Shorthands to return certain errors */

static PyObject *
type_error(const char *msg, PyObject *obj)
{
    PyErr_Format(PyExc_TypeError, msg, obj->ob_type->tp_name);
    return NULL;
}

static PyObject *
null_error(void)
{
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_SystemError,
                        "null argument to internal routine");
    }
    return NULL;
}

/* Operations on any object */

PyObject *
PyObject_Type(PyObject *o)
{
    PyObject *v;
    if (o == NULL) {
        return null_error();
    }
    v = (PyObject *)o->ob_type;
    Py_INCREF(v);
    return v;
}

Py_ssize_t
PyObject_Size(PyObject *o)
{
    PySequenceMethods *m;
    if (o == NULL) {
        null_error();
        return -1;
    }
    m = o->ob_type->tp_as_sequence;
    if (m && m->sq_length)
        return m->sq_length(o);
    return PyMapping_Size(o);
}

#undef PyObject_Length
Py_ssize_t
PyObject_Length(PyObject *o)
{
    return PyObject_Size(o);
}
#define PyObject_Length PyObject_Size

int
_PyObject_HasLen(PyObject *o) {
    return (Py_TYPE(o)->tp_as_sequence && Py_TYPE(o)->tp_as_sequence->sq_length) ||
        (Py_TYPE(o)->tp_as_mapping && Py_TYPE(o)->tp_as_mapping->mp_length);
}

/* The length hint function returns a non-negative value from o.__len__()
   or o.__length_hint__(). If those methods aren't found the defaultvalue is
   returned.  If one of the calls fails with an exception other than TypeError
   this function returns -1.
*/

Py_ssize_t
PyObject_LengthHint(PyObject *o, Py_ssize_t defaultvalue)
{
    PyObject *hint, *result;
    Py_ssize_t res;
    _Py_IDENTIFIER(__length_hint__);
    if (_PyObject_HasLen(o)) {
        res = PyObject_Length(o);
        if (res < 0 && PyErr_Occurred()) {
            if (!PyErr_ExceptionMatches(PyExc_TypeError)) {
                return -1;
            }
            PyErr_Clear();
        }
        else {
            return res;
        }
    }
    hint = _PyObject_LookupSpecial(o, &PyId___length_hint__);
    if (hint == NULL) {
        if (PyErr_Occurred()) {
            return -1;
        }
        return defaultvalue;
    }
    result = PyObject_CallFunctionObjArgs(hint, NULL);
    Py_DECREF(hint);
    if (result == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Clear();
            return defaultvalue;
        }
        return -1;
    }
    else if (result == Py_NotImplemented) {
        Py_DECREF(result);
        return defaultvalue;
    }
    if (!PyLong_Check(result)) {
        PyErr_Format(PyExc_TypeError, "__length_hint__ must be an integer, not %.100s",
            Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        return -1;
    }
    res = PyLong_AsSsize_t(result);
    Py_DECREF(result);
    if (res < 0 && PyErr_Occurred()) {
        return -1;
    }
    if (res < 0) {
        PyErr_Format(PyExc_ValueError, "__length_hint__() should return >= 0");
        return -1;
    }
    return res;
}

PyObject *
PyObject_GetItem(PyObject *o, PyObject *key)
{
    PyMappingMethods *m;

    if (o == NULL || key == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_mapping;
    if (m && m->mp_subscript) {
        PyObject *item = m->mp_subscript(o, key);
        assert((item != NULL) ^ (PyErr_Occurred() != NULL));
        return item;
    }

    if (o->ob_type->tp_as_sequence) {
        if (PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return NULL;
            return PySequence_GetItem(o, key_value);
        }
        else if (o->ob_type->tp_as_sequence->sq_item)
            return type_error("sequence index must "
                              "be integer, not '%.200s'", key);
    }

    return type_error("'%.200s' object is not subscriptable", o);
}

int
PyObject_SetItem(PyObject *o, PyObject *key, PyObject *value)
{
    PyMappingMethods *m;

    if (o == NULL || key == NULL || value == NULL) {
        null_error();
        return -1;
    }
    m = o->ob_type->tp_as_mapping;
    if (m && m->mp_ass_subscript)
        return m->mp_ass_subscript(o, key, value);

    if (o->ob_type->tp_as_sequence) {
        if (PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return -1;
            return PySequence_SetItem(o, key_value, value);
        }
        else if (o->ob_type->tp_as_sequence->sq_ass_item) {
            type_error("sequence index must be "
                       "integer, not '%.200s'", key);
            return -1;
        }
    }

    type_error("'%.200s' object does not support item assignment", o);
    return -1;
}

int
PyObject_DelItem(PyObject *o, PyObject *key)
{
    PyMappingMethods *m;

    if (o == NULL || key == NULL) {
        null_error();
        return -1;
    }
    m = o->ob_type->tp_as_mapping;
    if (m && m->mp_ass_subscript)
        return m->mp_ass_subscript(o, key, (PyObject*)NULL);

    if (o->ob_type->tp_as_sequence) {
        if (PyIndex_Check(key)) {
            Py_ssize_t key_value;
            key_value = PyNumber_AsSsize_t(key, PyExc_IndexError);
            if (key_value == -1 && PyErr_Occurred())
                return -1;
            return PySequence_DelItem(o, key_value);
        }
        else if (o->ob_type->tp_as_sequence->sq_ass_item) {
            type_error("sequence index must be "
                       "integer, not '%.200s'", key);
            return -1;
        }
    }

    type_error("'%.200s' object does not support item deletion", o);
    return -1;
}

int
PyObject_DelItemString(PyObject *o, const char *key)
{
    PyObject *okey;
    int ret;

    if (o == NULL || key == NULL) {
        null_error();
        return -1;
    }
    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return -1;
    ret = PyObject_DelItem(o, okey);
    Py_DECREF(okey);
    return ret;
}

/* We release the buffer right after use of this function which could
   cause issues later on.  Don't use these functions in new code.
 */
int
PyObject_AsCharBuffer(PyObject *obj,
                      const char **buffer,
                      Py_ssize_t *buffer_len)
{
    return PyObject_AsReadBuffer(obj, (const void **)buffer, buffer_len);
}

int
PyObject_CheckReadBuffer(PyObject *obj)
{
    PyBufferProcs *pb = obj->ob_type->tp_as_buffer;
    Py_buffer view;

    if (pb == NULL ||
        pb->bf_getbuffer == NULL)
        return 0;
    if ((*pb->bf_getbuffer)(obj, &view, PyBUF_SIMPLE) == -1) {
        PyErr_Clear();
        return 0;
    }
    PyBuffer_Release(&view);
    return 1;
}

int PyObject_AsReadBuffer(PyObject *obj,
                          const void **buffer,
                          Py_ssize_t *buffer_len)
{
    Py_buffer view;

    if (obj == NULL || buffer == NULL || buffer_len == NULL) {
        null_error();
        return -1;
    }
    if (PyObject_GetBuffer(obj, &view, PyBUF_SIMPLE) != 0)
        return -1;

    *buffer = view.buf;
    *buffer_len = view.len;
    PyBuffer_Release(&view);
    return 0;
}

int PyObject_AsWriteBuffer(PyObject *obj,
                           void **buffer,
                           Py_ssize_t *buffer_len)
{
    PyBufferProcs *pb;
    Py_buffer view;

    if (obj == NULL || buffer == NULL || buffer_len == NULL) {
        null_error();
        return -1;
    }
    pb = obj->ob_type->tp_as_buffer;
    if (pb == NULL ||
        pb->bf_getbuffer == NULL ||
        ((*pb->bf_getbuffer)(obj, &view, PyBUF_WRITABLE) != 0)) {
        PyErr_SetString(PyExc_TypeError,
                        "expected a writable bytes-like object");
        return -1;
    }

    *buffer = view.buf;
    *buffer_len = view.len;
    PyBuffer_Release(&view);
    return 0;
}

/* Buffer C-API for Python 3.0 */

int
PyObject_GetBuffer(PyObject *obj, Py_buffer *view, int flags)
{
    PyBufferProcs *pb = obj->ob_type->tp_as_buffer;

    if (pb == NULL || pb->bf_getbuffer == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "a bytes-like object is required, not '%.100s'",
                     Py_TYPE(obj)->tp_name);
        return -1;
    }
    return (*pb->bf_getbuffer)(obj, view, flags);
}

static int
_IsFortranContiguous(const Py_buffer *view)
{
    Py_ssize_t sd, dim;
    int i;

    /* 1) len = product(shape) * itemsize
       2) itemsize > 0
       3) len = 0 <==> exists i: shape[i] = 0 */
    if (view->len == 0) return 1;
    if (view->strides == NULL) {  /* C-contiguous by definition */
        /* Trivially F-contiguous */
        if (view->ndim <= 1) return 1;

        /* ndim > 1 implies shape != NULL */
        assert(view->shape != NULL);

        /* Effectively 1-d */
        sd = 0;
        for (i=0; i<view->ndim; i++) {
            if (view->shape[i] > 1) sd += 1;
        }
        return sd <= 1;
    }

    /* strides != NULL implies both of these */
    assert(view->ndim > 0);
    assert(view->shape != NULL);

    sd = view->itemsize;
    for (i=0; i<view->ndim; i++) {
        dim = view->shape[i];
        if (dim > 1 && view->strides[i] != sd) {
            return 0;
        }
        sd *= dim;
    }
    return 1;
}

static int
_IsCContiguous(const Py_buffer *view)
{
    Py_ssize_t sd, dim;
    int i;

    /* 1) len = product(shape) * itemsize
       2) itemsize > 0
       3) len = 0 <==> exists i: shape[i] = 0 */
    if (view->len == 0) return 1;
    if (view->strides == NULL) return 1; /* C-contiguous by definition */

    /* strides != NULL implies both of these */
    assert(view->ndim > 0);
    assert(view->shape != NULL);

    sd = view->itemsize;
    for (i=view->ndim-1; i>=0; i--) {
        dim = view->shape[i];
        if (dim > 1 && view->strides[i] != sd) {
            return 0;
        }
        sd *= dim;
    }
    return 1;
}

int
PyBuffer_IsContiguous(const Py_buffer *view, char order)
{

    if (view->suboffsets != NULL) return 0;

    if (order == 'C')
        return _IsCContiguous(view);
    else if (order == 'F')
        return _IsFortranContiguous(view);
    else if (order == 'A')
        return (_IsCContiguous(view) || _IsFortranContiguous(view));
    return 0;
}


void*
PyBuffer_GetPointer(Py_buffer *view, Py_ssize_t *indices)
{
    char* pointer;
    int i;
    pointer = (char *)view->buf;
    for (i = 0; i < view->ndim; i++) {
        pointer += view->strides[i]*indices[i];
        if ((view->suboffsets != NULL) && (view->suboffsets[i] >= 0)) {
            pointer = *((char**)pointer) + view->suboffsets[i];
        }
    }
    return (void*)pointer;
}


void
_Py_add_one_to_index_F(int nd, Py_ssize_t *index, const Py_ssize_t *shape)
{
    int k;

    for (k=0; k<nd; k++) {
        if (index[k] < shape[k]-1) {
            index[k]++;
            break;
        }
        else {
            index[k] = 0;
        }
    }
}

void
_Py_add_one_to_index_C(int nd, Py_ssize_t *index, const Py_ssize_t *shape)
{
    int k;

    for (k=nd-1; k>=0; k--) {
        if (index[k] < shape[k]-1) {
            index[k]++;
            break;
        }
        else {
            index[k] = 0;
        }
    }
}

int
PyBuffer_FromContiguous(Py_buffer *view, void *buf, Py_ssize_t len, char fort)
{
    int k;
    void (*addone)(int, Py_ssize_t *, const Py_ssize_t *);
    Py_ssize_t *indices, elements;
    char *src, *ptr;

    if (len > view->len) {
        len = view->len;
    }

    if (PyBuffer_IsContiguous(view, fort)) {
        /* simplest copy is all that is needed */
        memcpy(view->buf, buf, len);
        return 0;
    }

    /* Otherwise a more elaborate scheme is needed */

    /* view->ndim <= 64 */
    indices = (Py_ssize_t *)PyMem_Malloc(sizeof(Py_ssize_t)*(view->ndim));
    if (indices == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    for (k=0; k<view->ndim;k++) {
        indices[k] = 0;
    }

    if (fort == 'F') {
        addone = _Py_add_one_to_index_F;
    }
    else {
        addone = _Py_add_one_to_index_C;
    }
    src = buf;
    /* XXX : This is not going to be the fastest code in the world
             several optimizations are possible.
     */
    elements = len / view->itemsize;
    while (elements--) {
        ptr = PyBuffer_GetPointer(view, indices);
        memcpy(ptr, src, view->itemsize);
        src += view->itemsize;
        addone(view->ndim, indices, view->shape);
    }

    PyMem_Free(indices);
    return 0;
}

int PyObject_CopyData(PyObject *dest, PyObject *src)
{
    Py_buffer view_dest, view_src;
    int k;
    Py_ssize_t *indices, elements;
    char *dptr, *sptr;

    if (!PyObject_CheckBuffer(dest) ||
        !PyObject_CheckBuffer(src)) {
        PyErr_SetString(PyExc_TypeError,
                        "both destination and source must be "\
                        "bytes-like objects");
        return -1;
    }

    if (PyObject_GetBuffer(dest, &view_dest, PyBUF_FULL) != 0) return -1;
    if (PyObject_GetBuffer(src, &view_src, PyBUF_FULL_RO) != 0) {
        PyBuffer_Release(&view_dest);
        return -1;
    }

    if (view_dest.len < view_src.len) {
        PyErr_SetString(PyExc_BufferError,
                        "destination is too small to receive data from source");
        PyBuffer_Release(&view_dest);
        PyBuffer_Release(&view_src);
        return -1;
    }

    if ((PyBuffer_IsContiguous(&view_dest, 'C') &&
         PyBuffer_IsContiguous(&view_src, 'C')) ||
        (PyBuffer_IsContiguous(&view_dest, 'F') &&
         PyBuffer_IsContiguous(&view_src, 'F'))) {
        /* simplest copy is all that is needed */
        memcpy(view_dest.buf, view_src.buf, view_src.len);
        PyBuffer_Release(&view_dest);
        PyBuffer_Release(&view_src);
        return 0;
    }

    /* Otherwise a more elaborate copy scheme is needed */

    /* XXX(nnorwitz): need to check for overflow! */
    indices = (Py_ssize_t *)PyMem_Malloc(sizeof(Py_ssize_t)*view_src.ndim);
    if (indices == NULL) {
        PyErr_NoMemory();
        PyBuffer_Release(&view_dest);
        PyBuffer_Release(&view_src);
        return -1;
    }
    for (k=0; k<view_src.ndim;k++) {
        indices[k] = 0;
    }
    elements = 1;
    for (k=0; k<view_src.ndim; k++) {
        /* XXX(nnorwitz): can this overflow? */
        elements *= view_src.shape[k];
    }
    while (elements--) {
        _Py_add_one_to_index_C(view_src.ndim, indices, view_src.shape);
        dptr = PyBuffer_GetPointer(&view_dest, indices);
        sptr = PyBuffer_GetPointer(&view_src, indices);
        memcpy(dptr, sptr, view_src.itemsize);
    }
    PyMem_Free(indices);
    PyBuffer_Release(&view_dest);
    PyBuffer_Release(&view_src);
    return 0;
}

void
PyBuffer_FillContiguousStrides(int nd, Py_ssize_t *shape,
                               Py_ssize_t *strides, int itemsize,
                               char fort)
{
    int k;
    Py_ssize_t sd;

    sd = itemsize;
    if (fort == 'F') {
        for (k=0; k<nd; k++) {
            strides[k] = sd;
            sd *= shape[k];
        }
    }
    else {
        for (k=nd-1; k>=0; k--) {
            strides[k] = sd;
            sd *= shape[k];
        }
    }
    return;
}

int
PyBuffer_FillInfo(Py_buffer *view, PyObject *obj, void *buf, Py_ssize_t len,
                  int readonly, int flags)
{
    if (view == NULL) {
        PyErr_SetString(PyExc_BufferError,
            "PyBuffer_FillInfo: view==NULL argument is obsolete");
        return -1;
    }

    if (((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE) &&
        (readonly == 1)) {
        PyErr_SetString(PyExc_BufferError,
                        "Object is not writable.");
        return -1;
    }

    view->obj = obj;
    if (obj)
        Py_INCREF(obj);
    view->buf = buf;
    view->len = len;
    view->readonly = readonly;
    view->itemsize = 1;
    view->format = NULL;
    if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
        view->format = "B";
    view->ndim = 1;
    view->shape = NULL;
    if ((flags & PyBUF_ND) == PyBUF_ND)
        view->shape = &(view->len);
    view->strides = NULL;
    if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
        view->strides = &(view->itemsize);
    view->suboffsets = NULL;
    view->internal = NULL;
    return 0;
}

void
PyBuffer_Release(Py_buffer *view)
{
    PyObject *obj = view->obj;
    PyBufferProcs *pb;
    if (obj == NULL)
        return;
    pb = Py_TYPE(obj)->tp_as_buffer;
    if (pb && pb->bf_releasebuffer)
        pb->bf_releasebuffer(obj, view);
    view->obj = NULL;
    Py_DECREF(obj);
}

PyObject *
PyObject_Format(PyObject *obj, PyObject *format_spec)
{
    PyObject *meth;
    PyObject *empty = NULL;
    PyObject *result = NULL;
    _Py_IDENTIFIER(__format__);

    if (format_spec != NULL && !PyUnicode_Check(format_spec)) {
        PyErr_Format(PyExc_SystemError,
                     "Format specifier must be a string, not %.200s",
                     Py_TYPE(format_spec)->tp_name);
        return NULL;
    }

    /* Fast path for common types. */
    if (format_spec == NULL || PyUnicode_GET_LENGTH(format_spec) == 0) {
        if (PyUnicode_CheckExact(obj)) {
            Py_INCREF(obj);
            return obj;
        }
        if (PyLong_CheckExact(obj)) {
            return PyObject_Str(obj);
        }
    }

    /* If no format_spec is provided, use an empty string */
    if (format_spec == NULL) {
        empty = PyUnicode_New(0, 0);
        format_spec = empty;
    }

    /* Find the (unbound!) __format__ method */
    meth = _PyObject_LookupSpecial(obj, &PyId___format__);
    if (meth == NULL) {
        if (!PyErr_Occurred())
            PyErr_Format(PyExc_TypeError,
                         "Type %.100s doesn't define __format__",
                         Py_TYPE(obj)->tp_name);
        goto done;
    }

    /* And call it. */
    result = PyObject_CallFunctionObjArgs(meth, format_spec, NULL);
    Py_DECREF(meth);

    if (result && !PyUnicode_Check(result)) {
        PyErr_Format(PyExc_TypeError,
             "__format__ must return a str, not %.200s",
             Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        result = NULL;
        goto done;
    }

done:
    Py_XDECREF(empty);
    return result;
}
/* Operations on numbers */

int
PyNumber_Check(PyObject *o)
{
    return o && o->ob_type->tp_as_number &&
           (o->ob_type->tp_as_number->nb_int ||
        o->ob_type->tp_as_number->nb_float);
}

/* Binary operators */

#define NB_SLOT(x) offsetof(PyNumberMethods, x)
#define NB_BINOP(nb_methods, slot) \
        (*(binaryfunc*)(& ((char*)nb_methods)[slot]))
#define NB_TERNOP(nb_methods, slot) \
        (*(ternaryfunc*)(& ((char*)nb_methods)[slot]))

static noinline PyObject *
binop_type_error(PyObject *v, PyObject *w, const char *op_name)
{
    PyErr_Format(PyExc_TypeError,
                 "unsupported operand type(s) for %.100s: "
                 "'%.100s' and '%.100s'",
                 op_name,
                 v->ob_type->tp_name,
                 w->ob_type->tp_name);
    return NULL;
}

static noinline PyObject *
binop_not_implemented(PyObject *v, PyObject *w,
                      const int op_slot, const char *op_name)
{
    if (op_slot == NB_SLOT(nb_rshift) &&
        PyCFunction_Check(v) &&
        strcmp(((PyCFunctionObject *)v)->m_ml->ml_name, "print") == 0)
    {
        PyErr_Format(PyExc_TypeError,
                     "unsupported operand type(s) for %.100s: "
                     "'%.100s' and '%.100s'. Did you mean \"print(<message>, "
                     "file=<output_stream>)\"?",
                     op_name,
                     v->ob_type->tp_name,
                     w->ob_type->tp_name);
        return NULL;
    }
    return binop_type_error(v, w, op_name);
}

/*
  Calling scheme used for binary operations:

  Order operations are tried until either a valid result or error:
    w.op(v,w)[*], v.op(v,w), w.op(v,w)

  [*] only when v->ob_type != w->ob_type && w->ob_type is a subclass of
      v->ob_type
 */

static PyObject *
binary_op1(PyObject *v, PyObject *w, const int op_slot)
{
    PyObject *x;
    binaryfunc slotv = NULL;
    binaryfunc slotw = NULL;
    if (v->ob_type->tp_as_number != NULL)
        slotv = NB_BINOP(v->ob_type->tp_as_number, op_slot);
    if (w->ob_type != v->ob_type &&
        w->ob_type->tp_as_number != NULL) {
        slotw = NB_BINOP(w->ob_type->tp_as_number, op_slot);
        if (slotw == slotv)
            slotw = NULL;
    }
    if (slotv) {
        if (slotw && PyType_IsSubtype(w->ob_type, v->ob_type)) {
            x = slotw(v, w);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
            slotw = NULL;
        }
        x = slotv(v, w);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    if (slotw) {
        x = slotw(v, w);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    Py_RETURN_NOTIMPLEMENTED;
}

static PyObject *
binary_op(PyObject *v, PyObject *w, const int op_slot, const char *op_name)
{
    PyObject *result;
    if ((result = binary_op1(v, w, op_slot)) != Py_NotImplemented) {
        return result;
    } else {
        Py_DECREF(result);
        return binop_not_implemented(v, w, op_slot, op_name);
    }
}

PyObject *
PyNumber_Subtract(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_subtract), "-");
}

PyObject *
PyNumber_Or(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_or), "|");
}

PyObject *
PyNumber_Xor(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_xor), "^");
}

PyObject *
PyNumber_And(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_and), "&");
}

PyObject *
PyNumber_Lshift(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_lshift), "<<");
}

PyObject *
PyNumber_Rshift(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_rshift), ">>");
}

PyObject *
PyNumber_Divmod(PyObject *v, PyObject *w)
{
    return binary_op(v, w, offsetof(PyNumberMethods, nb_divmod), "divmod()");
}

/*
  Calling scheme used for ternary operations:

  Order operations are tried until either a valid result or error:
    v.op(v,w,z), w.op(v,w,z), z.op(v,w,z)
 */

static PyObject *
ternary_op(PyObject *v,
           PyObject *w,
           PyObject *z,
           const int op_slot,
           const char *op_name)
{
    PyNumberMethods *mv, *mw, *mz;
    PyObject *x = NULL;
    ternaryfunc slotv = NULL;
    ternaryfunc slotw = NULL;
    ternaryfunc slotz = NULL;
    mv = v->ob_type->tp_as_number;
    mw = w->ob_type->tp_as_number;
    if (mv != NULL)
        slotv = NB_TERNOP(mv, op_slot);
    if (w->ob_type != v->ob_type &&
        mw != NULL) {
        slotw = NB_TERNOP(mw, op_slot);
        if (slotw == slotv)
            slotw = NULL;
    }
    if (slotv) {
        if (slotw && PyType_IsSubtype(w->ob_type, v->ob_type)) {
            x = slotw(v, w, z);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
            slotw = NULL;
        }
        x = slotv(v, w, z);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    if (slotw) {
        x = slotw(v, w, z);
        if (x != Py_NotImplemented)
            return x;
        Py_DECREF(x); /* can't do it */
    }
    mz = z->ob_type->tp_as_number;
    if (mz != NULL) {
        slotz = NB_TERNOP(mz, op_slot);
        if (slotz == slotv || slotz == slotw)
            slotz = NULL;
        if (slotz) {
            x = slotz(v, w, z);
            if (x != Py_NotImplemented)
                return x;
            Py_DECREF(x); /* can't do it */
        }
    }
    if (z == Py_None)
        PyErr_Format(
            PyExc_TypeError,
            "unsupported operand type(s) for ** or pow(): "
            "'%.100s' and '%.100s'",
            v->ob_type->tp_name,
            w->ob_type->tp_name);
    else
        PyErr_Format(
            PyExc_TypeError,
            "unsupported operand type(s) for pow(): "
            "'%.100s', '%.100s', '%.100s'",
            v->ob_type->tp_name,
            w->ob_type->tp_name,
            z->ob_type->tp_name);
    return NULL;
}

PyObject *
PyNumber_Add(PyObject *v, PyObject *w)
{
    PyObject *result = binary_op1(v, w, NB_SLOT(nb_add));
    if (result == Py_NotImplemented) {
        PySequenceMethods *m = v->ob_type->tp_as_sequence;
        Py_DECREF(result);
        if (m && m->sq_concat) {
            return (*m->sq_concat)(v, w);
        }
        result = binop_type_error(v, w, "+");
    }
    return result;
}

static PyObject *
sequence_repeat(ssizeargfunc repeatfunc, PyObject *seq, PyObject *n)
{
    Py_ssize_t count;
    if (PyIndex_Check(n)) {
        count = PyNumber_AsSsize_t(n, PyExc_OverflowError);
        if (count == -1 && PyErr_Occurred())
            return NULL;
    }
    else {
        return type_error("can't multiply sequence by "
                          "non-int of type '%.200s'", n);
    }
    return (*repeatfunc)(seq, count);
}

PyObject *
PyNumber_Multiply(PyObject *v, PyObject *w)
{
    PyObject *result = binary_op1(v, w, NB_SLOT(nb_multiply));
    if (result == Py_NotImplemented) {
        PySequenceMethods *mv = v->ob_type->tp_as_sequence;
        PySequenceMethods *mw = w->ob_type->tp_as_sequence;
        Py_DECREF(result);
        if  (mv && mv->sq_repeat) {
            return sequence_repeat(mv->sq_repeat, v, w);
        }
        else if (mw && mw->sq_repeat) {
            return sequence_repeat(mw->sq_repeat, w, v);
        }
        result = binop_type_error(v, w, "*");
    }
    return result;
}

PyObject *
PyNumber_MatrixMultiply(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_matrix_multiply), "@");
}

PyObject *
PyNumber_FloorDivide(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_floor_divide), "//");
}

PyObject *
PyNumber_TrueDivide(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_true_divide), "/");
}

PyObject *
PyNumber_Remainder(PyObject *v, PyObject *w)
{
    return binary_op(v, w, NB_SLOT(nb_remainder), "%");
}

PyObject *
PyNumber_Power(PyObject *v, PyObject *w, PyObject *z)
{
    return ternary_op(v, w, z, NB_SLOT(nb_power), "** or pow()");
}

/* Binary in-place operators */

/* The in-place operators are defined to fall back to the 'normal',
   non in-place operations, if the in-place methods are not in place.

   - If the left hand object has the appropriate struct members, and
     they are filled, call the appropriate function and return the
     result.  No coercion is done on the arguments; the left-hand object
     is the one the operation is performed on, and it's up to the
     function to deal with the right-hand object.

   - Otherwise, in-place modification is not supported. Handle it exactly as
     a non in-place operation of the same kind.

   */

static PyObject *
binary_iop1(PyObject *v, PyObject *w, const int iop_slot, const int op_slot)
{
    PyNumberMethods *mv = v->ob_type->tp_as_number;
    if (mv != NULL) {
        binaryfunc slot = NB_BINOP(mv, iop_slot);
        if (slot) {
            PyObject *x = (slot)(v, w);
            if (x != Py_NotImplemented) {
                return x;
            }
            Py_DECREF(x);
        }
    }
    return binary_op1(v, w, op_slot);
}

static PyObject *
binary_iop(PyObject *v, PyObject *w, const int iop_slot, const int op_slot,
                const char *op_name)
{
    PyObject *result = binary_iop1(v, w, iop_slot, op_slot);
    if (result == Py_NotImplemented) {
        Py_DECREF(result);
        return binop_type_error(v, w, op_name);
    }
    return result;
}

#define INPLACE_BINOP(func, iop, op, op_name) \
    PyObject * \
    func(PyObject *v, PyObject *w) { \
        return binary_iop(v, w, NB_SLOT(iop), NB_SLOT(op), op_name); \
    }

INPLACE_BINOP(PyNumber_InPlaceOr, nb_inplace_or, nb_or, "|=")
INPLACE_BINOP(PyNumber_InPlaceXor, nb_inplace_xor, nb_xor, "^=")
INPLACE_BINOP(PyNumber_InPlaceAnd, nb_inplace_and, nb_and, "&=")
INPLACE_BINOP(PyNumber_InPlaceLshift, nb_inplace_lshift, nb_lshift, "<<=")
INPLACE_BINOP(PyNumber_InPlaceRshift, nb_inplace_rshift, nb_rshift, ">>=")
INPLACE_BINOP(PyNumber_InPlaceSubtract, nb_inplace_subtract, nb_subtract, "-=")
INPLACE_BINOP(PyNumber_InMatrixMultiply, nb_inplace_matrix_multiply, nb_matrix_multiply, "@=")

PyObject *
PyNumber_InPlaceFloorDivide(PyObject *v, PyObject *w)
{
    return binary_iop(v, w, NB_SLOT(nb_inplace_floor_divide),
                      NB_SLOT(nb_floor_divide), "//=");
}

PyObject *
PyNumber_InPlaceTrueDivide(PyObject *v, PyObject *w)
{
    return binary_iop(v, w, NB_SLOT(nb_inplace_true_divide),
                      NB_SLOT(nb_true_divide), "/=");
}

PyObject *
PyNumber_InPlaceAdd(PyObject *v, PyObject *w)
{
    PyObject *result = binary_iop1(v, w, NB_SLOT(nb_inplace_add),
                                   NB_SLOT(nb_add));
    if (result == Py_NotImplemented) {
        PySequenceMethods *m = v->ob_type->tp_as_sequence;
        Py_DECREF(result);
        if (m != NULL) {
            binaryfunc f = NULL;
            f = m->sq_inplace_concat;
            if (f == NULL)
                f = m->sq_concat;
            if (f != NULL)
                return (*f)(v, w);
        }
        result = binop_type_error(v, w, "+=");
    }
    return result;
}

PyObject *
PyNumber_InPlaceMultiply(PyObject *v, PyObject *w)
{
    PyObject *result = binary_iop1(v, w, NB_SLOT(nb_inplace_multiply),
                                   NB_SLOT(nb_multiply));
    if (result == Py_NotImplemented) {
        ssizeargfunc f = NULL;
        PySequenceMethods *mv = v->ob_type->tp_as_sequence;
        PySequenceMethods *mw = w->ob_type->tp_as_sequence;
        Py_DECREF(result);
        if (mv != NULL) {
            f = mv->sq_inplace_repeat;
            if (f == NULL)
                f = mv->sq_repeat;
            if (f != NULL)
                return sequence_repeat(f, v, w);
        }
        else if (mw != NULL) {
            /* Note that the right hand operand should not be
             * mutated in this case so sq_inplace_repeat is not
             * used. */
            if (mw->sq_repeat)
                return sequence_repeat(mw->sq_repeat, w, v);
        }
        result = binop_type_error(v, w, "*=");
    }
    return result;
}

PyObject *
PyNumber_InPlaceMatrixMultiply(PyObject *v, PyObject *w)
{
    return binary_iop(v, w, NB_SLOT(nb_inplace_matrix_multiply),
                      NB_SLOT(nb_matrix_multiply), "@=");
}

PyObject *
PyNumber_InPlaceRemainder(PyObject *v, PyObject *w)
{
    return binary_iop(v, w, NB_SLOT(nb_inplace_remainder),
                            NB_SLOT(nb_remainder), "%=");
}

PyObject *
PyNumber_InPlacePower(PyObject *v, PyObject *w, PyObject *z)
{
    if (v->ob_type->tp_as_number &&
        v->ob_type->tp_as_number->nb_inplace_power != NULL) {
        return ternary_op(v, w, z, NB_SLOT(nb_inplace_power), "**=");
    }
    else {
        return ternary_op(v, w, z, NB_SLOT(nb_power), "**=");
    }
}


/* Unary operators and functions */

PyObject *
PyNumber_Negative(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_number;
    if (m && m->nb_negative)
        return (*m->nb_negative)(o);

    return type_error("bad operand type for unary -: '%.200s'", o);
}

PyObject *
PyNumber_Positive(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_number;
    if (m && m->nb_positive)
        return (*m->nb_positive)(o);

    return type_error("bad operand type for unary +: '%.200s'", o);
}

PyObject *
PyNumber_Invert(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_number;
    if (m && m->nb_invert)
        return (*m->nb_invert)(o);

    return type_error("bad operand type for unary ~: '%.200s'", o);
}

PyObject *
PyNumber_Absolute(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_number;
    if (m && m->nb_absolute)
        return m->nb_absolute(o);

    return type_error("bad operand type for abs(): '%.200s'", o);
}

/* Return a Python int from the object item.
   Raise TypeError if the result is not an int
   or if the object cannot be interpreted as an index.
*/
PyObject *
PyNumber_Index(PyObject *item)
{
    PyObject *result = NULL;
    if (item == NULL) {
        return null_error();
    }

    if (PyLong_Check(item)) {
        Py_INCREF(item);
        return item;
    }
    if (!PyIndex_Check(item)) {
        PyErr_Format(PyExc_TypeError,
                     "'%.200s' object cannot be interpreted "
                     "as an integer", item->ob_type->tp_name);
        return NULL;
    }
    result = item->ob_type->tp_as_number->nb_index(item);
    if (!result || PyLong_CheckExact(result))
        return result;
    if (!PyLong_Check(result)) {
        PyErr_Format(PyExc_TypeError,
                     "__index__ returned non-int (type %.200s)",
                     result->ob_type->tp_name);
        Py_DECREF(result);
        return NULL;
    }
    /* Issue #17576: warn if 'result' not of exact type int. */
    if (PyErr_WarnFormat(PyExc_DeprecationWarning, 1,
            "__index__ returned non-int (type %.200s).  "
            "The ability to return an instance of a strict subclass of int "
            "is deprecated, and may be removed in a future version of Python.",
            result->ob_type->tp_name)) {
        Py_DECREF(result);
        return NULL;
    }
    return result;
}

/* Return an error on Overflow only if err is not NULL*/

Py_ssize_t
PyNumber_AsSsize_t(PyObject *item, PyObject *err)
{
    PyObject *value;
    PyObject *runerr;
    Py_ssize_t result;

    if (item && PyLong_Check(item)) {
        Py_INCREF(item);
        value = item;
    } else if (!(value = PyNumber_Index(item))) {
        return -1;
    }

    /* We're done if PyLong_AsSsize_t() returns without error. */
    result = PyLong_AsSsize_t(value);
    if (result != -1 || !(runerr = PyErr_Occurred()))
        goto finish;

    /* Error handling code -- only manage OverflowError differently */
    if (!PyErr_GivenExceptionMatches(runerr, PyExc_OverflowError))
        goto finish;

    PyErr_Clear();
    /* If no error-handling desired then the default clipping
       is sufficient.
     */
    if (!err) {
        assert(PyLong_Check(value));
        /* Whether or not it is less than or equal to
           zero is determined by the sign of ob_size
        */
        if (_PyLong_Sign(value) < 0)
            result = PY_SSIZE_T_MIN;
        else
            result = PY_SSIZE_T_MAX;
    }
    else {
        /* Otherwise replace the error with caller's error object. */
        PyErr_Format(err,
                     "cannot fit '%.200s' into an index-sized integer",
                     item->ob_type->tp_name);
    }

 finish:
    Py_DECREF(value);
    return result;
}


PyObject *
PyNumber_Long(PyObject *o)
{
    PyObject *result;
    PyNumberMethods *m;
    PyObject *trunc_func;
    Py_buffer view;
    _Py_IDENTIFIER(__trunc__);

    if (o == NULL) {
        return null_error();
    }

    if (PyLong_CheckExact(o)) {
        Py_INCREF(o);
        return o;
    }
    m = o->ob_type->tp_as_number;
    if (m && m->nb_int) { /* This should include subclasses of int */
        result = (PyObject *)_PyLong_FromNbInt(o);
        if (result != NULL && !PyLong_CheckExact(result)) {
            Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
        }
        return result;
    }
    trunc_func = _PyObject_LookupSpecial(o, &PyId___trunc__);
    if (trunc_func) {
        result = PyEval_CallObject(trunc_func, NULL);
        Py_DECREF(trunc_func);
        if (result == NULL || PyLong_CheckExact(result)) {
            return result;
        }
        if (PyLong_Check(result)) {
            Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
            return result;
        }
        /* __trunc__ is specified to return an Integral type,
           but int() needs to return an int. */
        m = result->ob_type->tp_as_number;
        if (m == NULL || m->nb_int == NULL) {
            PyErr_Format(
                PyExc_TypeError,
                "__trunc__ returned non-Integral (type %.200s)",
                result->ob_type->tp_name);
            Py_DECREF(result);
            return NULL;
        }
        Py_SETREF(result, (PyObject *)_PyLong_FromNbInt(result));
        if (result != NULL && !PyLong_CheckExact(result)) {
            Py_SETREF(result, _PyLong_Copy((PyLongObject *)result));
        }
        return result;
    }
    if (PyErr_Occurred())
        return NULL;

    if (PyUnicode_Check(o))
        /* The below check is done in PyLong_FromUnicode(). */
        return PyLong_FromUnicodeObject(o, 10);

    if (PyBytes_Check(o))
        /* need to do extra error checking that PyLong_FromString()
         * doesn't do.  In particular int('9\x005') must raise an
         * exception, not truncate at the null.
         */
        return _PyLong_FromBytes(PyBytes_AS_STRING(o),
                                 PyBytes_GET_SIZE(o), 10);

    if (PyByteArray_Check(o))
        return _PyLong_FromBytes(PyByteArray_AS_STRING(o),
                                 PyByteArray_GET_SIZE(o), 10);

    if (PyObject_GetBuffer(o, &view, PyBUF_SIMPLE) == 0) {
        PyObject *bytes;

        /* Copy to NUL-terminated buffer. */
        bytes = PyBytes_FromStringAndSize((const char *)view.buf, view.len);
        if (bytes == NULL) {
            PyBuffer_Release(&view);
            return NULL;
        }
        result = _PyLong_FromBytes(PyBytes_AS_STRING(bytes),
                                   PyBytes_GET_SIZE(bytes), 10);
        Py_DECREF(bytes);
        PyBuffer_Release(&view);
        return result;
    }

    return type_error("int() argument must be a string, a bytes-like object "
                      "or a number, not '%.200s'", o);
}

PyObject *
PyNumber_Float(PyObject *o)
{
    PyNumberMethods *m;

    if (o == NULL) {
        return null_error();
    }

    if (PyFloat_CheckExact(o)) {
        Py_INCREF(o);
        return o;
    }
    m = o->ob_type->tp_as_number;
    if (m && m->nb_float) { /* This should include subclasses of float */
        PyObject *res = m->nb_float(o);
        double val;
        if (!res || PyFloat_CheckExact(res)) {
            return res;
        }
        if (!PyFloat_Check(res)) {
            PyErr_Format(PyExc_TypeError,
                         "%.50s.__float__ returned non-float (type %.50s)",
                         o->ob_type->tp_name, res->ob_type->tp_name);
            Py_DECREF(res);
            return NULL;
        }
        /* Issue #26983: warn if 'res' not of exact type float. */
        if (PyErr_WarnFormat(PyExc_DeprecationWarning, 1,
                "%.50s.__float__ returned non-float (type %.50s).  "
                "The ability to return an instance of a strict subclass of float "
                "is deprecated, and may be removed in a future version of Python.",
                o->ob_type->tp_name, res->ob_type->tp_name)) {
            Py_DECREF(res);
            return NULL;
        }
        val = PyFloat_AS_DOUBLE(res);
        Py_DECREF(res);
        return PyFloat_FromDouble(val);
    }
    if (PyFloat_Check(o)) { /* A float subclass with nb_float == NULL */
        return PyFloat_FromDouble(PyFloat_AS_DOUBLE(o));
    }
    return PyFloat_FromString(o);
}


PyObject *
PyNumber_ToBase(PyObject *n, int base)
{
    PyObject *res = NULL;
    PyObject *index = PyNumber_Index(n);

    if (!index)
        return NULL;
    if (PyLong_Check(index))
        res = _PyLong_Format(index, base);
    else
        /* It should not be possible to get here, as
           PyNumber_Index already has a check for the same
           condition */
        PyErr_SetString(PyExc_ValueError, "PyNumber_ToBase: index not int");
    Py_DECREF(index);
    return res;
}


/* Operations on sequences */

int
PySequence_Check(PyObject *s)
{
    if (PyDict_Check(s))
        return 0;
    return s->ob_type->tp_as_sequence &&
        s->ob_type->tp_as_sequence->sq_item != NULL;
}

Py_ssize_t
PySequence_Size(PyObject *s)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_length)
        return m->sq_length(s);

    type_error("object of type '%.200s' has no len()", s);
    return -1;
}

#undef PySequence_Length
Py_ssize_t
PySequence_Length(PyObject *s)
{
    return PySequence_Size(s);
}
#define PySequence_Length PySequence_Size

PyObject *
PySequence_Concat(PyObject *s, PyObject *o)
{
    PySequenceMethods *m;

    if (s == NULL || o == NULL) {
        return null_error();
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_concat)
        return m->sq_concat(s, o);

    /* Instances of user classes defining an __add__() method only
       have an nb_add slot, not an sq_concat slot.      So we fall back
       to nb_add if both arguments appear to be sequences. */
    if (PySequence_Check(s) && PySequence_Check(o)) {
        PyObject *result = binary_op1(s, o, NB_SLOT(nb_add));
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be concatenated", s);
}

PyObject *
PySequence_Repeat(PyObject *o, Py_ssize_t count)
{
    PySequenceMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_sequence;
    if (m && m->sq_repeat)
        return m->sq_repeat(o, count);

    /* Instances of user classes defining a __mul__() method only
       have an nb_multiply slot, not an sq_repeat slot. so we fall back
       to nb_multiply if o appears to be a sequence. */
    if (PySequence_Check(o)) {
        PyObject *n, *result;
        n = PyLong_FromSsize_t(count);
        if (n == NULL)
            return NULL;
        result = binary_op1(o, n, NB_SLOT(nb_multiply));
        Py_DECREF(n);
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be repeated", o);
}

PyObject *
PySequence_InPlaceConcat(PyObject *s, PyObject *o)
{
    PySequenceMethods *m;

    if (s == NULL || o == NULL) {
        return null_error();
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_inplace_concat)
        return m->sq_inplace_concat(s, o);
    if (m && m->sq_concat)
        return m->sq_concat(s, o);

    if (PySequence_Check(s) && PySequence_Check(o)) {
        PyObject *result = binary_iop1(s, o, NB_SLOT(nb_inplace_add),
                                       NB_SLOT(nb_add));
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be concatenated", s);
}

PyObject *
PySequence_InPlaceRepeat(PyObject *o, Py_ssize_t count)
{
    PySequenceMethods *m;

    if (o == NULL) {
        return null_error();
    }

    m = o->ob_type->tp_as_sequence;
    if (m && m->sq_inplace_repeat)
        return m->sq_inplace_repeat(o, count);
    if (m && m->sq_repeat)
        return m->sq_repeat(o, count);

    if (PySequence_Check(o)) {
        PyObject *n, *result;
        n = PyLong_FromSsize_t(count);
        if (n == NULL)
            return NULL;
        result = binary_iop1(o, n, NB_SLOT(nb_inplace_multiply),
                             NB_SLOT(nb_multiply));
        Py_DECREF(n);
        if (result != Py_NotImplemented)
            return result;
        Py_DECREF(result);
    }
    return type_error("'%.200s' object can't be repeated", o);
}

PyObject *
PySequence_GetItem(PyObject *s, Py_ssize_t i)
{
    PySequenceMethods *m;

    if (s == NULL) {
        return null_error();
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0) {
                    assert(PyErr_Occurred());
                    return NULL;
                }
                i += l;
            }
        }
        return m->sq_item(s, i);
    }

    return type_error("'%.200s' object does not support indexing", s);
}

PyObject *
PySequence_GetSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2)
{
    PyMappingMethods *mp;

    if (!s) {
        return null_error();
    }

    mp = s->ob_type->tp_as_mapping;
    if (mp && mp->mp_subscript) {
        PyObject *res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return NULL;
        res = mp->mp_subscript(s, slice);
        Py_DECREF(slice);
        return res;
    }

    return type_error("'%.200s' object is unsliceable", s);
}

int
PySequence_SetItem(PyObject *s, Py_ssize_t i, PyObject *o)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_ass_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0)
                    return -1;
                i += l;
            }
        }
        return m->sq_ass_item(s, i, o);
    }

    type_error("'%.200s' object does not support item assignment", s);
    return -1;
}

int
PySequence_DelItem(PyObject *s, Py_ssize_t i)
{
    PySequenceMethods *m;

    if (s == NULL) {
        null_error();
        return -1;
    }

    m = s->ob_type->tp_as_sequence;
    if (m && m->sq_ass_item) {
        if (i < 0) {
            if (m->sq_length) {
                Py_ssize_t l = (*m->sq_length)(s);
                if (l < 0)
                    return -1;
                i += l;
            }
        }
        return m->sq_ass_item(s, i, (PyObject *)NULL);
    }

    type_error("'%.200s' object doesn't support item deletion", s);
    return -1;
}

int
PySequence_SetSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2, PyObject *o)
{
    PyMappingMethods *mp;

    if (s == NULL) {
        null_error();
        return -1;
    }

    mp = s->ob_type->tp_as_mapping;
    if (mp && mp->mp_ass_subscript) {
        int res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return -1;
        res = mp->mp_ass_subscript(s, slice, o);
        Py_DECREF(slice);
        return res;
    }

    type_error("'%.200s' object doesn't support slice assignment", s);
    return -1;
}

int
PySequence_DelSlice(PyObject *s, Py_ssize_t i1, Py_ssize_t i2)
{
    PyMappingMethods *mp;

    if (s == NULL) {
        null_error();
        return -1;
    }

    mp = s->ob_type->tp_as_mapping;
    if (mp && mp->mp_ass_subscript) {
        int res;
        PyObject *slice = _PySlice_FromIndices(i1, i2);
        if (!slice)
            return -1;
        res = mp->mp_ass_subscript(s, slice, NULL);
        Py_DECREF(slice);
        return res;
    }
    type_error("'%.200s' object doesn't support slice deletion", s);
    return -1;
}

PyObject *
PySequence_Tuple(PyObject *v)
{
    PyObject *it;  /* iter(v) */
    Py_ssize_t n;             /* guess for result tuple size */
    PyObject *result = NULL;
    Py_ssize_t j;

    if (v == NULL) {
        return null_error();
    }

    /* Special-case the common tuple and list cases, for efficiency. */
    if (PyTuple_CheckExact(v)) {
        /* Note that we can't know whether it's safe to return
           a tuple *subclass* instance as-is, hence the restriction
           to exact tuples here.  In contrast, lists always make
           a copy, so there's no need for exactness below. */
        Py_INCREF(v);
        return v;
    }
    if (PyList_CheckExact(v))
        return PyList_AsTuple(v);

    /* Get iterator. */
    it = PyObject_GetIter(v);
    if (it == NULL)
        return NULL;

    /* Guess result size and allocate space. */
    n = PyObject_LengthHint(v, 10);
    if (n == -1)
        goto Fail;
    result = PyTuple_New(n);
    if (result == NULL)
        goto Fail;

    /* Fill the tuple. */
    for (j = 0; ; ++j) {
        PyObject *item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred())
                goto Fail;
            break;
        }
        if (j >= n) {
            size_t newn = (size_t)n;
            /* The over-allocation strategy can grow a bit faster
               than for lists because unlike lists the
               over-allocation isn't permanent -- we reclaim
               the excess before the end of this routine.
               So, grow by ten and then add 25%.
            */
            newn += 10u;
            newn += newn >> 2;
            if (newn > PY_SSIZE_T_MAX) {
                /* Check for overflow */
                PyErr_NoMemory();
                Py_DECREF(item);
                goto Fail;
            }
            n = (Py_ssize_t)newn;
            if (_PyTuple_Resize(&result, n) != 0) {
                Py_DECREF(item);
                goto Fail;
            }
        }
        PyTuple_SET_ITEM(result, j, item);
    }

    /* Cut tuple back if guess was too large. */
    if (j < n &&
        _PyTuple_Resize(&result, j) != 0)
        goto Fail;

    Py_DECREF(it);
    return result;

Fail:
    Py_XDECREF(result);
    Py_DECREF(it);
    return NULL;
}

PyObject *
PySequence_List(PyObject *v)
{
    PyObject *result;  /* result list */
    PyObject *rv;          /* return value from PyList_Extend */

    if (v == NULL) {
        return null_error();
    }

    result = PyList_New(0);
    if (result == NULL)
        return NULL;

    rv = _PyList_Extend((PyListObject *)result, v);
    if (rv == NULL) {
        Py_DECREF(result);
        return NULL;
    }
    Py_DECREF(rv);
    return result;
}

PyObject *
PySequence_Fast(PyObject *v, const char *m)
{
    PyObject *it;

    if (v == NULL) {
        return null_error();
    }

    if (PyList_CheckExact(v) || PyTuple_CheckExact(v)) {
        Py_INCREF(v);
        return v;
    }

    it = PyObject_GetIter(v);
    if (it == NULL) {
        if (PyErr_ExceptionMatches(PyExc_TypeError))
            PyErr_SetString(PyExc_TypeError, m);
        return NULL;
    }

    v = PySequence_List(it);
    Py_DECREF(it);

    return v;
}

/* Iterate over seq.  Result depends on the operation:
   PY_ITERSEARCH_COUNT:  -1 if error, else # of times obj appears in seq.
   PY_ITERSEARCH_INDEX:  0-based index of first occurrence of obj in seq;
    set ValueError and return -1 if none found; also return -1 on error.
   Py_ITERSEARCH_CONTAINS:  return 1 if obj in seq, else 0; -1 on error.
*/
Py_ssize_t
_PySequence_IterSearch(PyObject *seq, PyObject *obj, int operation)
{
    Py_ssize_t n;
    int wrapped;  /* for PY_ITERSEARCH_INDEX, true iff n wrapped around */
    PyObject *it;  /* iter(seq) */

    if (seq == NULL || obj == NULL) {
        null_error();
        return -1;
    }

    it = PyObject_GetIter(seq);
    if (it == NULL) {
        type_error("argument of type '%.200s' is not iterable", seq);
        return -1;
    }

    n = wrapped = 0;
    for (;;) {
        int cmp;
        PyObject *item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred())
                goto Fail;
            break;
        }

        cmp = PyObject_RichCompareBool(obj, item, Py_EQ);
        Py_DECREF(item);
        if (cmp < 0)
            goto Fail;
        if (cmp > 0) {
            switch (operation) {
            case PY_ITERSEARCH_COUNT:
                if (n == PY_SSIZE_T_MAX) {
                    PyErr_SetString(PyExc_OverflowError,
                           "count exceeds C integer size");
                    goto Fail;
                }
                ++n;
                break;

            case PY_ITERSEARCH_INDEX:
                if (wrapped) {
                    PyErr_SetString(PyExc_OverflowError,
                           "index exceeds C integer size");
                    goto Fail;
                }
                goto Done;

            case PY_ITERSEARCH_CONTAINS:
                n = 1;
                goto Done;

            default:
                assert(!"unknown operation");
            }
        }

        if (operation == PY_ITERSEARCH_INDEX) {
            if (n == PY_SSIZE_T_MAX)
                wrapped = 1;
            ++n;
        }
    }

    if (operation != PY_ITERSEARCH_INDEX)
        goto Done;

    PyErr_SetString(PyExc_ValueError,
                    "sequence.index(x): x not in sequence");
    /* fall into failure code */
Fail:
    n = -1;
    /* fall through */
Done:
    Py_DECREF(it);
    return n;

}

/* Return # of times o appears in s. */
Py_ssize_t
PySequence_Count(PyObject *s, PyObject *o)
{
    return _PySequence_IterSearch(s, o, PY_ITERSEARCH_COUNT);
}

/* Return -1 if error; 1 if ob in seq; 0 if ob not in seq.
 * Use sq_contains if possible, else defer to _PySequence_IterSearch().
 */
int
PySequence_Contains(PyObject *seq, PyObject *ob)
{
    Py_ssize_t result;
    PySequenceMethods *sqm = seq->ob_type->tp_as_sequence;
    if (sqm != NULL && sqm->sq_contains != NULL)
        return (*sqm->sq_contains)(seq, ob);
    result = _PySequence_IterSearch(seq, ob, PY_ITERSEARCH_CONTAINS);
    return Py_SAFE_DOWNCAST(result, Py_ssize_t, int);
}

/* Backwards compatibility */
#undef PySequence_In
int
PySequence_In(PyObject *w, PyObject *v)
{
    return PySequence_Contains(w, v);
}

Py_ssize_t
PySequence_Index(PyObject *s, PyObject *o)
{
    return _PySequence_IterSearch(s, o, PY_ITERSEARCH_INDEX);
}

/* Operations on mappings */

int
PyMapping_Check(PyObject *o)
{
    return o && o->ob_type->tp_as_mapping &&
        o->ob_type->tp_as_mapping->mp_subscript;
}

Py_ssize_t
PyMapping_Size(PyObject *o)
{
    PyMappingMethods *m;

    if (o == NULL) {
        null_error();
        return -1;
    }

    m = o->ob_type->tp_as_mapping;
    if (m && m->mp_length)
        return m->mp_length(o);

    type_error("object of type '%.200s' has no len()", o);
    return -1;
}

#undef PyMapping_Length
Py_ssize_t
PyMapping_Length(PyObject *o)
{
    return PyMapping_Size(o);
}
#define PyMapping_Length PyMapping_Size

PyObject *
PyMapping_GetItemString(PyObject *o, const char *key)
{
    PyObject *okey, *r;
    if (key == NULL) {
        return null_error();
    }
    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return NULL;
    r = PyObject_GetItem(o, okey);
    Py_DECREF(okey);
    return r;
}

int
PyMapping_SetItemString(PyObject *o, const char *key, PyObject *value)
{
    PyObject *okey;
    int r;
    if (key == NULL) {
        null_error();
        return -1;
    }
    okey = PyUnicode_FromString(key);
    if (okey == NULL)
        return -1;
    r = PyObject_SetItem(o, okey, value);
    Py_DECREF(okey);
    return r;
}

int
PyMapping_HasKeyString(PyObject *o, const char *key)
{
    PyObject *v;
    v = PyMapping_GetItemString(o, key);
    if (v) {
        Py_DECREF(v);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

int
PyMapping_HasKey(PyObject *o, PyObject *key)
{
    PyObject *v;
    v = PyObject_GetItem(o, key);
    if (v) {
        Py_DECREF(v);
        return 1;
    }
    PyErr_Clear();
    return 0;
}

PyObject *
PyMapping_Keys(PyObject *o)
{
    PyObject *keys;
    PyObject *fast;
    _Py_IDENTIFIER(keys);
    if (PyDict_CheckExact(o))
        return PyDict_Keys(o);
    keys = _PyObject_CallMethodId(o, &PyId_keys, NULL);
    if (keys == NULL)
        return NULL;
    fast = PySequence_Fast(keys, "o.keys() are not iterable");
    Py_DECREF(keys);
    return fast;
}

PyObject *
PyMapping_Items(PyObject *o)
{
    PyObject *items;
    PyObject *fast;
    _Py_IDENTIFIER(items);
    if (PyDict_CheckExact(o))
        return PyDict_Items(o);
    items = _PyObject_CallMethodId(o, &PyId_items, NULL);
    if (items == NULL)
        return NULL;
    fast = PySequence_Fast(items, "o.items() are not iterable");
    Py_DECREF(items);
    return fast;
}

PyObject *
PyMapping_Values(PyObject *o)
{
    PyObject *values;
    PyObject *fast;
    _Py_IDENTIFIER(values);
    if (PyDict_CheckExact(o))
        return PyDict_Values(o);
    values = _PyObject_CallMethodId(o, &PyId_values, NULL);
    if (values == NULL)
        return NULL;
    fast = PySequence_Fast(values, "o.values() are not iterable");
    Py_DECREF(values);
    return fast;
}

PyObject *
PyObject_CallObject(PyObject *o, PyObject *a)
{
    return PyEval_CallObjectWithKeywords(o, a, NULL);
}

PyObject*
(_Py_CheckFunctionResult)(PyObject *func, PyObject *result, const char *where)
{
    int err_occurred = (PyErr_Occurred() != NULL);
    assert((func != NULL) ^ (where != NULL));
    if (result == NULL) {
        if (!err_occurred) {
            if (func)
                PyErr_Format(PyExc_SystemError,
                             "%R returned NULL without setting an error",
                             func);
            else
                PyErr_Format(PyExc_SystemError,
                             "%s returned NULL without setting an error",
                             where);
#ifdef Py_DEBUG
            /* Ensure that the bug is caught in debug mode */
            Py_FatalError("a function returned NULL without setting an error");
#endif
            return NULL;
        }
    }
    else {
        if (err_occurred) {
            Py_DECREF(result);
            if (func) {
                _PyErr_FormatFromCause(PyExc_SystemError,
                        "%R returned a result with an error set",
                        func);
            }
            else {
                _PyErr_FormatFromCause(PyExc_SystemError,
                        "%s returned a result with an error set",
                        where);
            }
#ifdef Py_DEBUG
            /* Ensure that the bug is caught in debug mode */
            Py_FatalError("a function returned a result with an error set");
#endif
            return NULL;
        }
    }
    return result;
}

PyObject *
PyObject_Call(PyObject *func, PyObject *args, PyObject *kwargs)
{
    ternaryfunc call;
    PyObject *result;
    /* PyObject_Call() must not be called with an exception set,
       because it may clear it (directly or indirectly) and so the
       caller loses its exception */
    assert(!PyErr_Occurred());
    assert(PyTuple_Check(args));
    assert(kwargs == NULL || PyDict_Check(kwargs));
    call = func->ob_type->tp_call;
    if (call == NULL) {
        PyErr_Format(PyExc_TypeError, "'%.200s' object is not callable",
                     func->ob_type->tp_name);
        return NULL;
    }
    if (Py_EnterRecursiveCall(" while calling a Python object"))
        return NULL;
    result = (*call)(func, args, kwargs);
    Py_LeaveRecursiveCall();
    return _Py_CheckFunctionResult(func, result, NULL);
}

PyObject *
_PyStack_AsTuple(PyObject **stack, Py_ssize_t nargs)
{
    Py_ssize_t i;
    PyObject *item, *args;
    if ((args = PyTuple_New(nargs))) {
        for (i=0; i < nargs; i++) {
            item = stack[i];
            Py_INCREF(item);
            PyTuple_SET_ITEM(args, i, item);
        }
        return args;
    } else {
        return 0;
    }
}

PyObject *
_PyObject_FastCallDict(PyObject *func, PyObject **args, Py_ssize_t nargs,
                       PyObject *kwargs)
{
    ternaryfunc call;
    PyObject *result = NULL;
    /* _PyObject_FastCallDict() must not be called with an exception set,
       because it may clear it (directly or indirectly) and so the
       caller loses its exception */
    assert(!PyErr_Occurred());
    assert(func != NULL);
    assert(nargs >= 0);
    assert(nargs == 0 || args != NULL);
    assert(kwargs == NULL || PyDict_Check(kwargs));
    if (Py_EnterRecursiveCall(" while calling a Python object")) {
        return NULL;
    }
    if (PyFunction_Check(func)) {
        result = _PyFunction_FastCallDict(func, args, nargs, kwargs);
    }
    else if (PyCFunction_Check(func)) {
        result = _PyCFunction_FastCallDict(func, args, nargs, kwargs);
    }
    else {
        Py_ssize_t i;
        PyObject *item, *tuple;
        /* Slow-path: build a temporary tuple */
        call = func->ob_type->tp_call;
        if (call == NULL) {
            PyErr_Format(PyExc_TypeError, "'%.200s' object is not callable",
                         func->ob_type->tp_name);
            goto exit;
        }
        /* [jart] inlined _PyStack_AsTuple b/c profiling */
        if (!(tuple = PyTuple_New(nargs))) return 0;
        for (i = 0; i < nargs; i++) {
            item = args[i];
            Py_INCREF(item);
            PyTuple_SET_ITEM(tuple, i, item);
        }
        result = (*call)(func, tuple, kwargs);
        Py_DECREF(tuple);
        result = _Py_CheckFunctionResult(func, result, NULL);
    }
exit:
    Py_LeaveRecursiveCall();
    return result;
}

/* Positional arguments are obj followed by args. */
PyObject *
_PyObject_Call_Prepend(PyObject *func,
                       PyObject *obj, PyObject *args, PyObject *kwargs)
{
    PyObject *small_stack[8];
    PyObject **stack;
    Py_ssize_t argcount;
    PyObject *result;
    assert(PyTuple_Check(args));
    argcount = PyTuple_GET_SIZE(args);
    if (argcount + 1 <= (Py_ssize_t)Py_ARRAY_LENGTH(small_stack)) {
        stack = small_stack;
    }
    else {
        stack = PyMem_Malloc((argcount + 1) * sizeof(PyObject *));
        if (stack == NULL) {
            PyErr_NoMemory();
            return NULL;
        }
    }
    /* use borrowed references */
    stack[0] = obj;
    memcpy(&stack[1],
              &PyTuple_GET_ITEM(args, 0),
              argcount * sizeof(PyObject *));
    result = _PyObject_FastCallDict(func,
                                    stack, argcount + 1,
                                    kwargs);
    if (stack != small_stack) {
        PyMem_Free(stack);
    }
    return result;
}

PyObject *
_PyStack_AsDict(PyObject **values, PyObject *kwnames)
{
    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);
    PyObject *kwdict;
    Py_ssize_t i;
    kwdict = PyDict_New();
    if (kwdict == NULL) {
        return NULL;
    }
    for (i = 0; i < nkwargs; i++) {
        PyObject *key = PyTuple_GET_ITEM(kwnames, i);
        PyObject *value = *values++;
        assert(PyUnicode_CheckExact(key));
        assert(PyDict_GetItem(kwdict, key) == NULL);
        if (PyDict_SetItem(kwdict, key, value)) {
            Py_DECREF(kwdict);
            return NULL;
        }
    }
    return kwdict;
}

int
_PyStack_UnpackDict(PyObject **args, Py_ssize_t nargs, PyObject *kwargs,
                    PyObject ***p_stack, PyObject **p_kwnames)
{
    PyObject **stack, **kwstack;
    Py_ssize_t nkwargs;
    Py_ssize_t pos, i;
    PyObject *key, *value;
    PyObject *kwnames;
    assert(nargs >= 0);
    assert(kwargs == NULL || PyDict_CheckExact(kwargs));
    if (kwargs == NULL || (nkwargs = PyDict_Size(kwargs)) == 0) {
        *p_stack = args;
        *p_kwnames = NULL;
        return 0;
    }
    if ((size_t)nargs > PY_SSIZE_T_MAX / sizeof(stack[0]) - (size_t)nkwargs) {
        PyErr_NoMemory();
        return -1;
    }
    stack = PyMem_Malloc((nargs + nkwargs) * sizeof(stack[0]));
    if (stack == NULL) {
        PyErr_NoMemory();
        return -1;
    }
    kwnames = PyTuple_New(nkwargs);
    if (kwnames == NULL) {
        PyMem_Free(stack);
        return -1;
    }
    /* Copy position arguments (borrowed references) */
    memcpy(stack, args, nargs * sizeof(stack[0]));
    kwstack = stack + nargs;
    pos = i = 0;
    /* This loop doesn't support lookup function mutating the dictionary
       to change its size. It's a deliberate choice for speed, this function is
       called in the performance critical hot code. */
    while (PyDict_Next(kwargs, &pos, &key, &value)) {
        Py_INCREF(key);
        PyTuple_SET_ITEM(kwnames, i, key);
        /* The stack contains borrowed references */
        kwstack[i] = value;
        i++;
    }
    *p_stack = stack;
    *p_kwnames = kwnames;
    return 0;
}

PyObject *
_PyObject_FastCallKeywords(PyObject *func, PyObject **stack, Py_ssize_t nargs,
                           PyObject *kwnames)
{
    PyObject *kwdict, *result;
    Py_ssize_t nkwargs = (kwnames == NULL) ? 0 : PyTuple_GET_SIZE(kwnames);
    assert(nargs >= 0);
    assert(kwnames == NULL || PyTuple_CheckExact(kwnames));
    assert((nargs == 0 && nkwargs == 0) || stack != NULL);
    /* kwnames must only contains str strings, no subclass, and all keys must
       be unique: these are implemented in Python/ceval.c and
       _PyArg_ParseStack(). */
    if (PyFunction_Check(func)) {
        return _PyFunction_FastCallKeywords(func, stack, nargs, kwnames);
    }
    if (PyCFunction_Check(func)) {
        return _PyCFunction_FastCallKeywords(func, stack, nargs, kwnames);
    }
    if (nkwargs > 0) {
        kwdict = _PyStack_AsDict(stack + nargs, kwnames);
        if (kwdict == NULL) {
            return NULL;
        }
    }
    else {
        kwdict = NULL;
    }
    result = _PyObject_FastCallDict(func, stack, nargs, kwdict);
    Py_XDECREF(kwdict);
    return result;
}

static PyObject*
call_function_tail(PyObject *callable, PyObject *args)
{
    PyObject *result;
    assert(args != NULL);
    if (!PyTuple_Check(args)) {
        result = _PyObject_CallArg1(callable, args);
    }
    else {
        result = PyObject_Call(callable, args, NULL);
    }
    return result;
}

PyObject *
PyObject_CallFunction(PyObject *callable, const char *format, ...)
{
    va_list va;
    PyObject *args, *result;
    if (callable == NULL) {
        return null_error();
    }
    if (!format || !*format) {
        return _PyObject_CallNoArg(callable);
    }
    va_start(va, format);
    args = Py_VaBuildValue(format, va);
    va_end(va);
    if (args == NULL) {
        return NULL;
    }
    result = call_function_tail(callable, args);
    Py_DECREF(args);
    return result;
}

PyObject *
_PyObject_CallFunction_SizeT(PyObject *callable, const char *format, ...)
{
    va_list va;
    PyObject *args, *result;
    if (callable == NULL) {
        return null_error();
    }
    if (!format || !*format) {
        return _PyObject_CallNoArg(callable);
    }
    va_start(va, format);
    args = _Py_VaBuildValue_SizeT(format, va);
    va_end(va);
    if (args == NULL) {
        return NULL;
    }
    result = call_function_tail(callable, args);
    Py_DECREF(args);
    return result;
}

static PyObject*
callmethod(PyObject* func, const char *format, va_list va, int is_size_t)
{
    PyObject *args, *result;
    assert(func != NULL);
    if (!PyCallable_Check(func)) {
        type_error("attribute of type '%.200s' is not callable", func);
        return NULL;
    }
    if (!format || !*format) {
        return _PyObject_CallNoArg(func);
    }
    if (is_size_t) {
        args = _Py_VaBuildValue_SizeT(format, va);
    }
    else {
        args = Py_VaBuildValue(format, va);
    }
    if (args == NULL) {
        return NULL;
    }
    result = call_function_tail(func, args);
    Py_DECREF(args);
    return result;
}

PyObject *
PyObject_CallMethod(PyObject *o, const char *name, const char *format, ...)
{
    va_list va;
    PyObject *func = NULL;
    PyObject *retval = NULL;
    if (o == NULL || name == NULL) {
        return null_error();
    }
    func = PyObject_GetAttrString(o, name);
    if (func == NULL)
        return NULL;
    va_start(va, format);
    retval = callmethod(func, format, va, 0);
    va_end(va);
    Py_DECREF(func);
    return retval;
}

PyObject *
_PyObject_CallMethodId(PyObject *o, _Py_Identifier *name,
                       const char *format, ...)
{
    va_list va;
    PyObject *func = NULL;
    PyObject *retval = NULL;
    if (o == NULL || name == NULL) {
        return null_error();
    }
    func = _PyObject_GetAttrId(o, name);
    if (func == NULL)
        return NULL;
    va_start(va, format);
    retval = callmethod(func, format, va, 0);
    va_end(va);
    Py_DECREF(func);
    return retval;
}

PyObject *
_PyObject_CallMethod_SizeT(PyObject *o, const char *name,
                           const char *format, ...)
{
    va_list va;
    PyObject *func = NULL;
    PyObject *retval;
    if (o == NULL || name == NULL) {
        return null_error();
    }
    func = PyObject_GetAttrString(o, name);
    if (func == NULL)
        return NULL;
    va_start(va, format);
    retval = callmethod(func, format, va, 1);
    va_end(va);
    Py_DECREF(func);
    return retval;
}

PyObject *
_PyObject_CallMethodId_SizeT(PyObject *o, _Py_Identifier *name,
                             const char *format, ...)
{
    va_list va;
    PyObject *func = NULL;
    PyObject *retval;
    if (o == NULL || name == NULL) {
        return null_error();
    }
    func = _PyObject_GetAttrId(o, name);
    if (func == NULL) {
        return NULL;
    }
    va_start(va, format);
    retval = callmethod(func, format, va, 1);
    va_end(va);
    Py_DECREF(func);
    return retval;
}

static PyObject **
objargs_mkstack(PyObject **small_stack, Py_ssize_t small_stack_size,
                va_list va, Py_ssize_t *p_nargs)
{
    Py_ssize_t i, n;
    va_list countva;
    PyObject **stack;
    /* Count the number of arguments */
    va_copy(countva, va);
    n = 0;
    while (1) {
        PyObject *arg = va_arg(countva, PyObject *);
        if (arg == NULL) {
            break;
        }
        n++;
    }
    *p_nargs = n;
    /* Copy arguments */
    if (n <= small_stack_size) {
        stack = small_stack;
    }
    else {
        stack = PyMem_Malloc(n * sizeof(stack[0]));
        if (stack == NULL) {
            va_end(countva);
            PyErr_NoMemory();
            return NULL;
        }
    }
    for (i = 0; i < n; ++i) {
        stack[i] = va_arg(va, PyObject *);
    }
    va_end(countva);
    return stack;
}

PyObject *
PyObject_CallMethodObjArgs(PyObject *callable, PyObject *name, ...)
{
    PyObject *small_stack[5];
    PyObject **stack;
    Py_ssize_t nargs;
    PyObject *result;
    va_list vargs;
    if (callable == NULL || name == NULL) {
        return null_error();
    }
    callable = PyObject_GetAttr(callable, name);
    if (callable == NULL)
        return NULL;
    /* count the args */
    va_start(vargs, name);
    stack = objargs_mkstack(small_stack, Py_ARRAY_LENGTH(small_stack),
                            vargs, &nargs);
    va_end(vargs);
    if (stack == NULL) {
        Py_DECREF(callable);
        return NULL;
    }
    result = _PyObject_FastCall(callable, stack, nargs);
    Py_DECREF(callable);
    if (stack != small_stack) {
        PyMem_Free(stack);
    }
    return result;
}

PyObject *
_PyObject_CallMethodIdObjArgs(PyObject *callable,
                              struct _Py_Identifier *name, ...)
{
    PyObject *small_stack[5];
    PyObject **stack;
    Py_ssize_t nargs;
    PyObject *result;
    va_list vargs;
    if (callable == NULL || name == NULL) {
        return null_error();
    }
    callable = _PyObject_GetAttrId(callable, name);
    if (callable == NULL)
        return NULL;
    /* count the args */
    va_start(vargs, name);
    stack = objargs_mkstack(small_stack, Py_ARRAY_LENGTH(small_stack),
                            vargs, &nargs);
    va_end(vargs);
    if (stack == NULL) {
        Py_DECREF(callable);
        return NULL;
    }
    result = _PyObject_FastCall(callable, stack, nargs);
    Py_DECREF(callable);
    if (stack != small_stack) {
        PyMem_Free(stack);
    }
    return result;
}

PyObject *
PyObject_CallFunctionObjArgs(PyObject *callable, ...)
{
    PyObject *small_stack[5];
    PyObject **stack;
    Py_ssize_t nargs;
    PyObject *result;
    va_list vargs;
    if (callable == NULL) {
        return null_error();
    }
    /* count the args */
    va_start(vargs, callable);
    stack = objargs_mkstack(small_stack, Py_ARRAY_LENGTH(small_stack),
                            vargs, &nargs);
    va_end(vargs);
    if (stack == NULL) {
        return NULL;
    }
    result = _PyObject_FastCall(callable, stack, nargs);
    if (stack != small_stack) {
        PyMem_Free(stack);
    }
    return result;
}

/* abstract_get_bases() has logically 4 return states:
 *
 * 1. getattr(cls, '__bases__') could raise an AttributeError
 * 2. getattr(cls, '__bases__') could raise some other exception
 * 3. getattr(cls, '__bases__') could return a tuple
 * 4. getattr(cls, '__bases__') could return something other than a tuple
 *
 * Only state #3 is a non-error state and only it returns a non-NULL object
 * (it returns the retrieved tuple).
 *
 * Any raised AttributeErrors are masked by clearing the exception and
 * returning NULL.  If an object other than a tuple comes out of __bases__,
 * then again, the return value is NULL.  So yes, these two situations
 * produce exactly the same results: NULL is returned and no error is set.
 *
 * If some exception other than AttributeError is raised, then NULL is also
 * returned, but the exception is not cleared.  That's because we want the
 * exception to be propagated along.
 *
 * Callers are expected to test for PyErr_Occurred() when the return value
 * is NULL to decide whether a valid exception should be propagated or not.
 * When there's no exception to propagate, it's customary for the caller to
 * set a TypeError.
 */
static PyObject *
abstract_get_bases(PyObject *cls)
{
    _Py_IDENTIFIER(__bases__);
    PyObject *bases;
    Py_ALLOW_RECURSION
    bases = _PyObject_GetAttrId(cls, &PyId___bases__);
    Py_END_ALLOW_RECURSION
    if (bases == NULL) {
        if (PyErr_ExceptionMatches(PyExc_AttributeError))
            PyErr_Clear();
        return NULL;
    }
    if (!PyTuple_Check(bases)) {
        Py_DECREF(bases);
        return NULL;
    }
    return bases;
}

static int
abstract_issubclass(PyObject *derived, PyObject *cls)
{
    PyObject *bases = NULL;
    Py_ssize_t i, n;
    int r = 0;
    while (1) {
        if (derived == cls)
            return 1;
        bases = abstract_get_bases(derived);
        if (bases == NULL) {
            if (PyErr_Occurred())
                return -1;
            return 0;
        }
        n = PyTuple_GET_SIZE(bases);
        if (n == 0) {
            Py_DECREF(bases);
            return 0;
        }
        /* Avoid recursivity in the single inheritance case */
        if (n == 1) {
            derived = PyTuple_GET_ITEM(bases, 0);
            Py_DECREF(bases);
            continue;
        }
        for (i = 0; i < n; i++) {
            r = abstract_issubclass(PyTuple_GET_ITEM(bases, i), cls);
            if (r != 0)
                break;
        }
        Py_DECREF(bases);
        return r;
    }
}

static int
check_class(PyObject *cls, const char *error)
{
    PyObject *bases = abstract_get_bases(cls);
    if (bases == NULL) {
        /* Do not mask errors. */
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_TypeError, error);
        return 0;
    }
    Py_DECREF(bases);
    return -1;
}

static int
recursive_isinstance(PyObject *inst, PyObject *cls)
{
    PyObject *icls;
    int retval = 0;
    _Py_IDENTIFIER(__class__);
    if (PyType_Check(cls)) {
        retval = PyObject_TypeCheck(inst, (PyTypeObject *)cls);
        if (retval == 0) {
            PyObject *c = _PyObject_GetAttrId(inst, &PyId___class__);
            if (c == NULL) {
                if (PyErr_ExceptionMatches(PyExc_AttributeError))
                    PyErr_Clear();
                else
                    retval = -1;
            }
            else {
                if (c != (PyObject *)(inst->ob_type) &&
                    PyType_Check(c))
                    retval = PyType_IsSubtype(
                        (PyTypeObject *)c,
                        (PyTypeObject *)cls);
                Py_DECREF(c);
            }
        }
    }
    else {
        if (!check_class(cls,
            "isinstance() arg 2 must be a type or tuple of types"))
            return -1;
        icls = _PyObject_GetAttrId(inst, &PyId___class__);
        if (icls == NULL) {
            if (PyErr_ExceptionMatches(PyExc_AttributeError))
                PyErr_Clear();
            else
                retval = -1;
        }
        else {
            retval = abstract_issubclass(icls, cls);
            Py_DECREF(icls);
        }
    }
    return retval;
}

int
PyObject_IsInstance(PyObject *inst, PyObject *cls)
{
    _Py_IDENTIFIER(__instancecheck__);
    PyObject *checker;
    /* Quick test for an exact match */
    if (Py_TYPE(inst) == (PyTypeObject *)cls)
        return 1;
    /* We know what type's __instancecheck__ does. */
    if (PyType_CheckExact(cls)) {
        return recursive_isinstance(inst, cls);
    }
    if (PyTuple_Check(cls)) {
        Py_ssize_t i;
        Py_ssize_t n;
        int r = 0;
        if (Py_EnterRecursiveCall(" in __instancecheck__"))
            return -1;
        n = PyTuple_GET_SIZE(cls);
        for (i = 0; i < n; ++i) {
            PyObject *item = PyTuple_GET_ITEM(cls, i);
            r = PyObject_IsInstance(inst, item);
            if (r != 0)
                /* either found it, or got an error */
                break;
        }
        Py_LeaveRecursiveCall();
        return r;
    }
    checker = _PyObject_LookupSpecial(cls, &PyId___instancecheck__);
    if (checker != NULL) {
        PyObject *res;
        int ok = -1;
        if (Py_EnterRecursiveCall(" in __instancecheck__")) {
            Py_DECREF(checker);
            return ok;
        }
        res = PyObject_CallFunctionObjArgs(checker, inst, NULL);
        Py_LeaveRecursiveCall();
        Py_DECREF(checker);
        if (res != NULL) {
            ok = PyObject_IsTrue(res);
            Py_DECREF(res);
        }
        return ok;
    }
    else if (PyErr_Occurred())
        return -1;
    /* Probably never reached anymore. */
    return recursive_isinstance(inst, cls);
}

static  int
recursive_issubclass(PyObject *derived, PyObject *cls)
{
    if (PyType_Check(cls) && PyType_Check(derived)) {
        /* Fast path (non-recursive) */
        return PyType_IsSubtype((PyTypeObject *)derived, (PyTypeObject *)cls);
    }
    if (!check_class(derived,
                     "issubclass() arg 1 must be a class"))
        return -1;
    if (!check_class(cls,
                    "issubclass() arg 2 must be a class"
                    " or tuple of classes"))
        return -1;
    return abstract_issubclass(derived, cls);
}

int
PyObject_IsSubclass(PyObject *derived, PyObject *cls)
{
    _Py_IDENTIFIER(__subclasscheck__);
    PyObject *checker;
    /* We know what type's __subclasscheck__ does. */
    if (PyType_CheckExact(cls)) {
        /* Quick test for an exact match */
        if (derived == cls)
            return 1;
        return recursive_issubclass(derived, cls);
    }
    if (PyTuple_Check(cls)) {
        Py_ssize_t i;
        Py_ssize_t n;
        int r = 0;
        if (Py_EnterRecursiveCall(" in __subclasscheck__"))
            return -1;
        n = PyTuple_GET_SIZE(cls);
        for (i = 0; i < n; ++i) {
            PyObject *item = PyTuple_GET_ITEM(cls, i);
            r = PyObject_IsSubclass(derived, item);
            if (r != 0)
                /* either found it, or got an error */
                break;
        }
        Py_LeaveRecursiveCall();
        return r;
    }
    checker = _PyObject_LookupSpecial(cls, &PyId___subclasscheck__);
    if (checker != NULL) {
        PyObject *res;
        int ok = -1;
        if (Py_EnterRecursiveCall(" in __subclasscheck__")) {
            Py_DECREF(checker);
            return ok;
        }
        res = PyObject_CallFunctionObjArgs(checker, derived, NULL);
        Py_LeaveRecursiveCall();
        Py_DECREF(checker);
        if (res != NULL) {
            ok = PyObject_IsTrue(res);
            Py_DECREF(res);
        }
        return ok;
    }
    else if (PyErr_Occurred())
        return -1;
    /* Probably never reached anymore. */
    return recursive_issubclass(derived, cls);
}

int
_PyObject_RealIsInstance(PyObject *inst, PyObject *cls)
{
    return recursive_isinstance(inst, cls);
}

int
_PyObject_RealIsSubclass(PyObject *derived, PyObject *cls)
{
    return recursive_issubclass(derived, cls);
}

PyObject *
PyObject_GetIter(PyObject *o)
{
    PyTypeObject *t = o->ob_type;
    getiterfunc f = NULL;
    f = t->tp_iter;
    if (f == NULL) {
        if (PySequence_Check(o))
            return PySeqIter_New(o);
        return type_error("'%.200s' object is not iterable", o);
    }
    else {
        PyObject *res = (*f)(o);
        if (res != NULL && !PyIter_Check(res)) {
            PyErr_Format(PyExc_TypeError,
                         "iter() returned non-iterator "
                         "of type '%.100s'",
                         res->ob_type->tp_name);
            Py_DECREF(res);
            res = NULL;
        }
        return res;
    }
}

/* Return next item.
 * If an error occurs, return NULL.  PyErr_Occurred() will be true.
 * If the iteration terminates normally, return NULL and clear the
 * PyExc_StopIteration exception (if it was set).  PyErr_Occurred()
 * will be false.
 * Else return the next object.  PyErr_Occurred() will be false.
 */
PyObject *
PyIter_Next(PyObject *iter)
{
    PyObject *result;
    result = (*iter->ob_type->tp_iternext)(iter);
    if (result == NULL &&
        PyErr_Occurred() &&
        PyErr_ExceptionMatches(PyExc_StopIteration))
        PyErr_Clear();
    return result;
}

/*
 * Flatten a sequence of bytes() objects into a C array of
 * NULL terminated string pointers with a NULL char* terminating the array.
 * (ie: an argv or env list)
 *
 * Memory allocated for the returned list is allocated using PyMem_Malloc()
 * and MUST be freed by _Py_FreeCharPArray().
 */
char *const *
_PySequence_BytesToCharpArray(PyObject* self)
{
    char **array;
    Py_ssize_t i, argc;
    PyObject *item = NULL;
    Py_ssize_t size;
    argc = PySequence_Size(self);
    if (argc == -1)
        return NULL;
    assert(argc >= 0);
    if ((size_t)argc > (PY_SSIZE_T_MAX-sizeof(char *)) / sizeof(char *)) {
        PyErr_NoMemory();
        return NULL;
    }
    array = PyMem_Malloc((argc + 1) * sizeof(char *));
    if (array == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    for (i = 0; i < argc; ++i) {
        char *data;
        item = PySequence_GetItem(self, i);
        if (item == NULL) {
            /* NULL terminate before freeing. */
            array[i] = NULL;
            goto fail;
        }
        /* check for embedded null bytes */
        if (PyBytes_AsStringAndSize(item, &data, NULL) < 0) {
            /* NULL terminate before freeing. */
            array[i] = NULL;
            goto fail;
        }
        size = PyBytes_GET_SIZE(item) + 1;
        array[i] = PyMem_Malloc(size);
        if (!array[i]) {
            PyErr_NoMemory();
            goto fail;
        }
        memcpy(array[i], data, size);
        Py_DECREF(item);
    }
    array[argc] = NULL;
    return array;
fail:
    Py_XDECREF(item);
    _Py_FreeCharPArray(array);
    return NULL;
}

/* Free's a NULL terminated char** array of C strings. */
void
_Py_FreeCharPArray(char *const array[])
{
    Py_ssize_t i;
    for (i = 0; array[i] != NULL; ++i) {
        PyMem_Free(array[i]);
    }
    PyMem_Free((void*)array);
}
