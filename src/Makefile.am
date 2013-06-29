
sbin_PROGRAMS = tvdaemon
tvdaemon_SOURCES = tvdaemon.cpp
tvdaemon_CXXFLAGS = -DTVDAEMON_HTML=\"${datadir}/tvdaemon/html\"
tvdaemon_LDFLAGS = ../lib/libtvdaemon.la -lpthread
tvdaemon_LDADD = ../v4l-utils/lib/libdvbv5/libdvbv5.la

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/v4l-utils/lib/include
