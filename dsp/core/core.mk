#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += DSP_CORE

DSP_CORE_ARTIFACTS += DSP_CORE_A
DSP_CORE = $(DSP_CORE_A_DEPS) $(DSP_CORE_A)
DSP_CORE_A = o/$(MODE)/dsp/core/core.a
DSP_CORE_A_FILES := $(wildcard dsp/core/*)
DSP_CORE_A_HDRS = $(filter %.h,$(DSP_CORE_A_FILES))
DSP_CORE_A_SRCS_S = $(filter %.S,$(DSP_CORE_A_FILES))
DSP_CORE_A_SRCS_C = $(filter %.c,$(DSP_CORE_A_FILES))

DSP_CORE_A_SRCS =				\
	$(DSP_CORE_A_SRCS_S)			\
	$(DSP_CORE_A_SRCS_C)

DSP_CORE_A_OBJS =				\
	$(DSP_CORE_A_SRCS_S:%.S=o/$(MODE)/%.o)	\
	$(DSP_CORE_A_SRCS_C:%.c=o/$(MODE)/%.o)

DSP_CORE_A_CHECKS =				\
	$(DSP_CORE_A).pkg			\
	$(DSP_CORE_A_HDRS:%=o/$(MODE)/%.ok)

DSP_CORE_A_DIRECTDEPS =				\
	LIBC_INTRIN				\
	LIBC_MEM				\
	LIBC_NEXGEN32E				\
	LIBC_STR				\
	LIBC_TINYMATH				\
	LIBC_STUBS

DSP_CORE_A_DEPS :=				\
	$(call uniq,$(foreach x,$(DSP_CORE_A_DIRECTDEPS),$($(x))))

$(DSP_CORE_A):	dsp/core/			\
		$(DSP_CORE_A).pkg		\
		$(DSP_CORE_A_OBJS)

$(DSP_CORE_A).pkg:				\
		$(DSP_CORE_A_OBJS)		\
		$(foreach x,$(DSP_CORE_A_DIRECTDEPS),$($(x)_A).pkg)

o/$(MODE)/dsp/core/dct.o			\
o/$(MODE)/dsp/core/c1331.o			\
o/$(MODE)/dsp/core/magikarp.o			\
o/$(MODE)/dsp/core/c93654369.o			\
o/$(MODE)/dsp/core/float2short.o		\
o/$(MODE)/dsp/core/scalevolume.o:		\
		OVERRIDE_CFLAGS +=		\
			$(MATHEMATICAL)

o/$(MODE)/dsp/core/alaw.o:			\
		CC = clang

o/tiny/dsp/core/scalevolume.o:			\
		OVERRIDE_CFLAGS +=		\
			-Os

o/$(MODE)/dsp/core/det3.o:			\
		OVERRIDE_CFLAGS +=		\
			-ffast-math

DSP_CORE_LIBS = $(foreach x,$(DSP_CORE_ARTIFACTS),$($(x)))
DSP_CORE_SRCS = $(foreach x,$(DSP_CORE_ARTIFACTS),$($(x)_SRCS))
DSP_CORE_HDRS = $(foreach x,$(DSP_CORE_ARTIFACTS),$($(x)_HDRS))
DSP_CORE_CHECKS = $(foreach x,$(DSP_CORE_ARTIFACTS),$($(x)_CHECKS))
DSP_CORE_OBJS = $(foreach x,$(DSP_CORE_ARTIFACTS),$($(x)_OBJS))
$(DSP_CORE_OBJS): $(BUILD_FILES) dsp/core/core.mk

.PHONY: o/$(MODE)/dsp/core
o/$(MODE)/dsp/core: $(DSP_CORE_CHECKS)
