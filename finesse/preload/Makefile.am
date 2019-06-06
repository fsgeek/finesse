include $(top_srcdir)/mk/c.am
include $(top_srcdir)/mk/rpcgen.am

SUBDIRS  = .
SUBDIRS += test

lib_LTLIBRARIES  = libnicfs.la
noinst_LTLIBRARIES = libnictypes.la

libnicfs_includedir = $(includedir)/nicfs
libnicfs_includedir += $(topdir)/libfuse
libnicfs_include_HEADERS = nicfs.h

libnicfs_la_SOURCES = nicfsinit.c
# libnicfs_la_SOURCES += nicfs-internal.h
libnicfs_la_SOURCES += nicopenclose.c
libnicfs_la_SOURCES += nicreadwrite.c
libnicfs_la_SOURCES += nicfdmgr.c
libnicfs_la_SOURCES += nicfscomm.c
libnicfs_la_SOURCES += nicunlink.c
libnicfs_la_SOURCES += nicsearch.c


libnicfs_la_CPPFLAGS  = $(AM_CPPFLAGS)
libnicfs_la_CPPFLAGS += -I$(top_srcdir)/libfuse/lib


libnicfs_la_LIBADD  = -lpthread
libnicfs_la_LIBADD += $(top_builddir)/crt/libcrt.la
libnicfs_la_LIBADD += $(top_builddir)/nvring/libnvring.la
libnicfs_la_LIBADD += -lrt

nodist_libnictypes_la_SOURCES  = nictypes.h
nodist_libnictypes_la_SOURCES += nictypes_xdr.c

libnictypes_la_CFLAGS  = $(AM_CFLAGS)
libnictypes_la_CFLAGS += $(RPCGEN_CFLAGS)

EXTRA_DIST = nictypes.x
BUILT_SOURCES = $(nodist_libnictypes_la_SOURCES)

CLEANFILES  = *~
CLEANFILES += $(BUILT_SOURCES) 
