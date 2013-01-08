lib_LTLIBRARIES = libtvdaemon.la
libtvdaemon_la_SOURCES = TVDaemon.cpp \
			  ConfigObject.cpp \
			  Utils.cpp \
			  Source.cpp \
			  Adapter.cpp \
			  Frontend.cpp	    \
			  Frontend_DVBS.cpp \
			  Frontend_DVBC.cpp \
			  Frontend_DVBT.cpp \
			  Frontend_ATSC.cpp \
			  Transponder.cpp \
			  Transponder_DVBS.cpp \
			  Transponder_DVBC.cpp \
			  Transponder_DVBT.cpp \
			  Transponder_ATSC.cpp \
			  Service.cpp \
			  Port.cpp \
			  Channel.cpp \
			  Stream.cpp \
			  SocketHandler.cpp \
			  HTTPServer.cpp \
			  Recorder.cpp \
			  Matroska.cpp \
			  RingBuffer.cpp \
			  Frame.cpp \
			  Thread.cpp \
			  Log.cpp

libtvdaemon_la_LIBADD = -ludev -lconfig++ -lrt -lpthread -ljson ../v4l-utils/lib/libdvbv5/libdvbv5.la -lmatroska

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/v4l-utils/lib/include
