include $(top_srcdir)/mk/c.am

test_progs  = testfinesse


noinst_PROGRAMS  = $(test_progs)

TESTS  = $(test_progs)

testfinesse_SOURCES  = testfinesse.c

LDADD  = $(top_builddir)/finesse/libfinesseproto.la
LDADD += -lrt
LDADD += -ldl
LDADD += -luuid
LDADD += -lprotobuf-c
LDADD += $(top_builddir)/munit/libmunit.la

# testfinesse_LDFLAGS  = -static-libasan

testfinesse_CPPFLAGS  = $(AM_CPPFLAGS)
# testfinesse_CPPFLAGS += -I$(top_srcdir)/libfuse/lib


CLEANFILES = *~
