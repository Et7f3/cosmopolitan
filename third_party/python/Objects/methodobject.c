/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=4 sts=4 sw=4 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/boolobject.h"
#include "third_party/python/Include/ceval.h"
#include "third_party/python/Include/descrobject.h"
#include "third_party/python/Include/dictobject.h"
#include "third_party/python/Include/methodobject.h"
#include "third_party/python/Include/modsupport.h"
#include "third_party/python/Include/moduleobject.h"
#include "third_party/python/Include/objimpl.h"
#include "third_party/python/Include/pyhash.h"
#include "third_party/python/Include/structmember.h"
/* clang-format off */

/* Free list for method objects to safe malloc/free overhead
 * The m_self element is used to chain the objects.
 */
static PyCFunctionObject *free_list = NULL;
static int numfree = 0;
#ifndef PyCFunction_MAXFREELIST
#define PyCFunction_MAXFREELIST 256
#endif

/* undefine macro trampoline to PyCFunction_NewEx */
#undef PyCFunction_New

PyObject *
PyCFunction_New(PyMethodDef *ml, PyObject *self)
{
    return PyCFunction_NewEx(ml, self, NULL);
}

PyObject *
PyCFunction_NewEx(PyMethodDef *ml, PyObject *self, PyObject *module)
{
    PyCFunctionObject *op;
    op = free_list;
    if (op != NULL) {
        free_list = (PyCFunctionObject *)(op->m_self);
        (void)PyObject_INIT(op, &PyCFunction_Type);
        numfree--;
    }
    else {
        op = PyObject_GC_New(PyCFunctionObject, &PyCFunction_Type);
        if (op == NULL)
            return NULL;
    }
    op->m_weakreflist = NULL;
    op->m_ml = ml;
    Py_XINCREF(self);
    op->m_self = self;
    Py_XINCREF(module);
    op->m_module = module;
    _PyObject_GC_TRACK(op);
    return (PyObject *)op;
}

PyCFunction
PyCFunction_GetFunction(PyObject *op)
{
    if (!PyCFunction_Check(op)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    return PyCFunction_GET_FUNCTION(op);
}

PyObject *
PyCFunction_GetSelf(PyObject *op)
{
    if (!PyCFunction_Check(op)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    return PyCFunction_GET_SELF(op);
}

int
PyCFunction_GetFlags(PyObject *op)
{
    if (!PyCFunction_Check(op)) {
        PyErr_BadInternalCall();
        return -1;
    }
    return PyCFunction_GET_FLAGS(op);
}

PyObject *
PyCFunction_Call(PyObject *func, PyObject *args, PyObject *kwds)
{
    PyCFunctionObject* f = (PyCFunctionObject*)func;
    PyCFunction meth = PyCFunction_GET_FUNCTION(func);
    PyObject *self = PyCFunction_GET_SELF(func);
    PyObject *arg, *res;
    Py_ssize_t size;
    int flags;

    /* PyCFunction_Call() must not be called with an exception set,
       because it may clear it (directly or indirectly) and so the
       caller loses its exception */
    assert(!PyErr_Occurred());

    flags = PyCFunction_GET_FLAGS(func) & ~(METH_CLASS | METH_STATIC | METH_COEXIST);

    if (flags == (METH_VARARGS | METH_KEYWORDS)) {
        res = (*(PyCFunctionWithKeywords)meth)(self, args, kwds);
    }
    else if (flags == METH_FASTCALL) {
        PyObject **stack = &PyTuple_GET_ITEM(args, 0);
        Py_ssize_t nargs = PyTuple_GET_SIZE(args);
        res = _PyCFunction_FastCallDict(func, stack, nargs, kwds);
    }
    else {
        if (kwds != NULL && PyDict_Size(kwds) != 0) {
            PyErr_Format(PyExc_TypeError, "%.200s() takes no keyword arguments",
                         f->m_ml->ml_name);
            return NULL;
        }

        switch (flags) {
        case METH_VARARGS:
            res = (*meth)(self, args);
            break;

        case METH_NOARGS:
            size = PyTuple_GET_SIZE(args);
            if (size != 0) {
                PyErr_Format(PyExc_TypeError,
                    "%.200s() takes no arguments (%zd given)",
                    f->m_ml->ml_name, size);
                return NULL;
            }

            res = (*meth)(self, NULL);
            break;

        case METH_O:
            size = PyTuple_GET_SIZE(args);
            if (size != 1) {
                PyErr_Format(PyExc_TypeError,
                    "%.200s() takes exactly one argument (%zd given)",
                    f->m_ml->ml_name, size);
                return NULL;
            }

            arg = PyTuple_GET_ITEM(args, 0);
            res = (*meth)(self, arg);
            break;

        default:
            PyErr_SetString(PyExc_SystemError,
                            "Bad call flags in PyCFunction_Call. "
                            "METH_OLDARGS is no longer supported!");
            return NULL;
        }
    }

    return _Py_CheckFunctionResult(func, res, NULL);
}

PyObject *
_PyCFunction_FastCallDict(PyObject *func_obj, PyObject **args, Py_ssize_t nargs,
                          PyObject *kwargs)
{
    PyCFunctionObject *func = (PyCFunctionObject*)func_obj;
    PyCFunction meth = PyCFunction_GET_FUNCTION(func);
    PyObject *self = PyCFunction_GET_SELF(func);
    PyObject *result;
    int flags;

    assert(PyCFunction_Check(func));
    assert(func != NULL);
    assert(nargs >= 0);
    assert(nargs == 0 || args != NULL);
    assert(kwargs == NULL || PyDict_Check(kwargs));

    /* _PyCFunction_FastCallDict() must not be called with an exception set,
       because it may clear it (directly or indirectly) and so the
       caller loses its exception */
    assert(!PyErr_Occurred());

    flags = PyCFunction_GET_FLAGS(func) & ~(METH_CLASS | METH_STATIC | METH_COEXIST);

    switch (flags)
    {
    case METH_NOARGS:
        if (kwargs != NULL && PyDict_Size(kwargs) != 0) {
            PyErr_Format(PyExc_TypeError, "%.200s() takes no keyword arguments",
                         func->m_ml->ml_name);
            return NULL;
        }

        if (nargs != 0) {
            PyErr_Format(PyExc_TypeError,
                "%.200s() takes no arguments (%zd given)",
                func->m_ml->ml_name, nargs);
            return NULL;
        }

        result = (*meth) (self, NULL);
        break;

    case METH_O:
        if (kwargs != NULL && PyDict_Size(kwargs) != 0) {
            PyErr_Format(PyExc_TypeError, "%.200s() takes no keyword arguments",
                         func->m_ml->ml_name);
            return NULL;
        }

        if (nargs != 1) {
            PyErr_Format(PyExc_TypeError,
                "%.200s() takes exactly one argument (%zd given)",
                func->m_ml->ml_name, nargs);
            return NULL;
        }

        result = (*meth) (self, args[0]);
        break;

    case METH_VARARGS:
    case METH_VARARGS | METH_KEYWORDS:
    {
        /* Slow-path: create a temporary tuple */
        Py_ssize_t i;
        PyObject *item, *tuple;

        if (!(flags & METH_KEYWORDS) && kwargs != NULL && PyDict_Size(kwargs) != 0) {
            PyErr_Format(PyExc_TypeError,
                         "%.200s() takes no keyword arguments",
                         func->m_ml->ml_name);
            return NULL;
        }

        /* [jart] inlined _PyStack_AsTuple b/c profiling */
        if (!(tuple = PyTuple_New(nargs))) return 0;
        for (i = 0; i < nargs; i++) {
            item = args[i];
            Py_INCREF(item);
            PyTuple_SET_ITEM(tuple, i, item);
        }

        if (flags & METH_KEYWORDS) {
            result = (*(PyCFunctionWithKeywords)meth) (self, tuple, kwargs);
        }
        else {
            result = (*meth) (self, tuple);
        }
        Py_DECREF(tuple);
        break;
    }

    case METH_FASTCALL:
    {
        PyObject **stack;
        PyObject *kwnames;
        _PyCFunctionFast fastmeth = (_PyCFunctionFast)meth;

        if (_PyStack_UnpackDict(args, nargs, kwargs, &stack, &kwnames) < 0) {
            return NULL;
        }

        result = (*fastmeth) (self, stack, nargs, kwnames);
        if (stack != args) {
            PyMem_Free(stack);
        }
        Py_XDECREF(kwnames);
        break;
    }

    default:
        PyErr_SetString(PyExc_SystemError,
                        "Bad call flags in PyCFunction_Call. "
                        "METH_OLDARGS is no longer supported!");
        return NULL;
    }

    result = _Py_CheckFunctionResult(func_obj, result, NULL);

    return result;
}

PyObject *
_PyCFunction_FastCallKeywords(PyObject *func, PyObject **stack,
                              Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *kwdict, *result;
    Py_ssize_t nkwargs = (kwnames == NULL) ? 0 : PyTuple_GET_SIZE(kwnames);

    assert(PyCFunction_Check(func));
    assert(nargs >= 0);
    assert(kwnames == NULL || PyTuple_CheckExact(kwnames));
    assert((nargs == 0 && nkwargs == 0) || stack != NULL);
    /* kwnames must only contains str strings, no subclass, and all keys must
       be unique */

    if (nkwargs > 0) {
        kwdict = _PyStack_AsDict(stack + nargs, kwnames);
        if (kwdict == NULL) {
            return NULL;
        }
    }
    else {
        kwdict = NULL;
    }

    result = _PyCFunction_FastCallDict(func, stack, nargs, kwdict);
    Py_XDECREF(kwdict);
    return result;
}

/* Methods (the standard built-in methods, that is) */

static void
meth_dealloc(PyCFunctionObject *m)
{
    _PyObject_GC_UNTRACK(m);
    if (m->m_weakreflist != NULL) {
        PyObject_ClearWeakRefs((PyObject*) m);
    }
    Py_XDECREF(m->m_self);
    Py_XDECREF(m->m_module);
    if (numfree < PyCFunction_MAXFREELIST) {
        m->m_self = (PyObject *)free_list;
        free_list = m;
        numfree++;
    }
    else {
        PyObject_GC_Del(m);
    }
}

static PyObject *
meth_reduce(PyCFunctionObject *m)
{
    _Py_IDENTIFIER(getattr);

    if (m->m_self == NULL || PyModule_Check(m->m_self))
        return PyUnicode_FromString(m->m_ml->ml_name);

    return Py_BuildValue("N(Os)", _PyEval_GetBuiltinId(&PyId_getattr),
                         m->m_self, m->m_ml->ml_name);
}

static PyMethodDef meth_methods[] = {
    {"__reduce__", (PyCFunction)meth_reduce, METH_NOARGS, NULL},
    {NULL, NULL}
};

static PyObject *
meth_get__text_signature__(PyCFunctionObject *m, void *closure)
{
    return _PyType_GetTextSignatureFromInternalDoc(m->m_ml->ml_name, m->m_ml->ml_doc);
}

static PyObject *
meth_get__doc__(PyCFunctionObject *m, void *closure)
{
    return _PyType_GetDocFromInternalDoc(m->m_ml->ml_name, m->m_ml->ml_doc);
}

static PyObject *
meth_get__name__(PyCFunctionObject *m, void *closure)
{
    return PyUnicode_FromString(m->m_ml->ml_name);
}

static PyObject *
meth_get__qualname__(PyCFunctionObject *m, void *closure)
{
    /* If __self__ is a module or NULL, return m.__name__
       (e.g. len.__qualname__ == 'len')

       If __self__ is a type, return m.__self__.__qualname__ + '.' + m.__name__
       (e.g. dict.fromkeys.__qualname__ == 'dict.fromkeys')

       Otherwise return type(m.__self__).__qualname__ + '.' + m.__name__
       (e.g. [].append.__qualname__ == 'list.append') */
    PyObject *type, *type_qualname, *res;
    _Py_IDENTIFIER(__qualname__);

    if (m->m_self == NULL || PyModule_Check(m->m_self))
        return PyUnicode_FromString(m->m_ml->ml_name);

    type = PyType_Check(m->m_self) ? m->m_self : (PyObject*)Py_TYPE(m->m_self);

    type_qualname = _PyObject_GetAttrId(type, &PyId___qualname__);
    if (type_qualname == NULL)
        return NULL;

    if (!PyUnicode_Check(type_qualname)) {
        PyErr_SetString(PyExc_TypeError, "<method>.__class__."
                        "__qualname__ is not a unicode object");
        Py_XDECREF(type_qualname);
        return NULL;
    }

    res = PyUnicode_FromFormat("%S.%s", type_qualname, m->m_ml->ml_name);
    Py_DECREF(type_qualname);
    return res;
}

static int
meth_traverse(PyCFunctionObject *m, visitproc visit, void *arg)
{
    Py_VISIT(m->m_self);
    Py_VISIT(m->m_module);
    return 0;
}

static PyObject *
meth_get__self__(PyCFunctionObject *m, void *closure)
{
    PyObject *self;

    self = PyCFunction_GET_SELF(m);
    if (self == NULL)
        self = Py_None;
    Py_INCREF(self);
    return self;
}

static PyGetSetDef meth_getsets [] = {
    {"__doc__",  (getter)meth_get__doc__,  NULL, NULL},
    {"__name__", (getter)meth_get__name__, NULL, NULL},
    {"__qualname__", (getter)meth_get__qualname__, NULL, NULL},
    {"__self__", (getter)meth_get__self__, NULL, NULL},
    {"__text_signature__", (getter)meth_get__text_signature__, NULL, NULL},
    {0}
};

#define OFF(x) offsetof(PyCFunctionObject, x)

static PyMemberDef meth_members[] = {
    {"__module__",    T_OBJECT,     OFF(m_module), PY_WRITE_RESTRICTED},
    {NULL}
};

static PyObject *
meth_repr(PyCFunctionObject *m)
{
    if (m->m_self == NULL || PyModule_Check(m->m_self))
        return PyUnicode_FromFormat("<built-in function %s>",
                                   m->m_ml->ml_name);
    return PyUnicode_FromFormat("<built-in method %s of %s object at %p>",
                               m->m_ml->ml_name,
                               m->m_self->ob_type->tp_name,
                               m->m_self);
}

static PyObject *
meth_richcompare(PyObject *self, PyObject *other, int op)
{
    PyCFunctionObject *a, *b;
    PyObject *res;
    int eq;

    if ((op != Py_EQ && op != Py_NE) ||
        !PyCFunction_Check(self) ||
        !PyCFunction_Check(other))
    {
        Py_RETURN_NOTIMPLEMENTED;
    }
    a = (PyCFunctionObject *)self;
    b = (PyCFunctionObject *)other;
    eq = a->m_self == b->m_self;
    if (eq)
        eq = a->m_ml->ml_meth == b->m_ml->ml_meth;
    if (op == Py_EQ)
        res = eq ? Py_True : Py_False;
    else
        res = eq ? Py_False : Py_True;
    Py_INCREF(res);
    return res;
}

static Py_hash_t
meth_hash(PyCFunctionObject *a)
{
    Py_hash_t x, y;
    if (a->m_self == NULL)
        x = 0;
    else {
        x = PyObject_Hash(a->m_self);
        if (x == -1)
            return -1;
    }
    y = _Py_HashPointer((void*)(a->m_ml->ml_meth));
    if (y == -1)
        return -1;
    x ^= y;
    if (x == -1)
        x = -2;
    return x;
}


PyTypeObject PyCFunction_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "builtin_function_or_method",
    sizeof(PyCFunctionObject),
    0,
    (destructor)meth_dealloc,                   /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    (reprfunc)meth_repr,                        /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    (hashfunc)meth_hash,                        /* tp_hash */
    PyCFunction_Call,                           /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)meth_traverse,                /* tp_traverse */
    0,                                          /* tp_clear */
    meth_richcompare,                           /* tp_richcompare */
    offsetof(PyCFunctionObject, m_weakreflist), /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    meth_methods,                               /* tp_methods */
    meth_members,                               /* tp_members */
    meth_getsets,                               /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
};

/* Clear out the free list */

int
PyCFunction_ClearFreeList(void)
{
    int freelist_size = numfree;

    while (free_list) {
        PyCFunctionObject *v = free_list;
        free_list = (PyCFunctionObject *)(v->m_self);
        PyObject_GC_Del(v);
        numfree--;
    }
    assert(numfree == 0);
    return freelist_size;
}

void
PyCFunction_Fini(void)
{
    (void)PyCFunction_ClearFreeList();
}

/* Print summary info about the state of the optimized allocator */
void
_PyCFunction_DebugMallocStats(FILE *out)
{
    _PyDebugAllocatorStats(out,
                           "free PyCFunctionObject",
                           numfree, sizeof(PyCFunctionObject));
}
