include $(top_srcdir)/mk/c.am

test_progs  = testnicfs


noinst_PROGRAMS  = $(test_progs)

TESTS  = $(test_progs)

testnicfs_SOURCES  = testnicfs.c
testnicfs_SOURCES += pathdata.c

LDADD  = -lrt
LDADD += $(top_builddir)/munit/libmunit.la
LDADD += $(top_builddir)/nvring/libnvring.la
LDADD += $(top_builddir)/nicfs/libnicfs.la
LDADD += -ldl
LDADD += -luuid
LDADD += -lrt

testnicfs_CPPFLAGS  = $(AM_CPPFLAGS)
testnicfs_CPPFLAGS += -I$(top_srcdir)/libfuse/lib


CLEANFILES = *~
