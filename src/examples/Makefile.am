AM_CPPFLAGS = -I.. -I$(top_srcdir)/src
#AM_CFLAGS = @CFLAGS@ @WARN_CFLAGS@

@CODE_COVERAGE_RULES@

AM_LIBS = $(CODE_COVERAGE_LDFLAGS)
AM_CFLAGS = @CFLAGS@ $(CODE_COVERAGE_CFLAGS)

noinst_PROGRAMS =

noinst_PROGRAMS += example-1
example_1_SOURCES = example-1.c
example_1_LDADD = ../smcp/libsmcp.la

noinst_PROGRAMS += example-2
example_2_SOURCES = example-2.c
example_2_LDADD = ../smcp/libsmcp.la

noinst_PROGRAMS += example-3
example_3_SOURCES = example-3.c
example_3_LDADD = ../smcp/libsmcp.la

noinst_PROGRAMS += example-4
example_4_SOURCES = example-4.c
example_4_LDADD = ../smcp/libsmcp.la

noinst_PROGRAMS += example-multicast-request
example_multicast_request_SOURCES = example-multicast-request.c
example_multicast_request_LDADD = ../smcp/libsmcp.la

DISTCLEANFILES = .deps Makefile
