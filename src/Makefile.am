
sbin_PROGRAMS = tvdaemon
tvdaemon_SOURCES = tvdaemon.cpp
tvdaemon_CXXFLAGS = -DTVDAEMON_HTML=\"${datadir}/tvdaemon/html\" -D__STDC_CONSTANT_MACROS -I$(top_srcdir)/include -I$(top_srcdir)/v4l-utils/lib/include
tvdaemon_LDFLAGS = ../lib/libtvdaemon.la
tvdaemon_LDADD = ${LIBCCRTP_LIBS}

