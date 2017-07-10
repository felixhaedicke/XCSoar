MUSL ?= n

ifeq ($(MUSL),y)
# Unfortunately, the musl programmers chose not to provide a predefined macro
# for compile time detection of the c library (like __GLIBC__ in glibc).
# http://wiki.musl-libc.org/wiki/FAQ#Q:_why_is_there_no_MUSL_macro_.3F
TARGET_CPPFLAGS += -DMUSL
endif
