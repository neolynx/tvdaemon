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
			  Activity.cpp \
			  Activity_Record.cpp \
			  Activity_Scan.cpp \
			  Activity_UpdateEPG.cpp \
			  Event.cpp \
			  RPCObject.cpp \
			  Log.cpp \
			  StreamingHandler.cpp \
			  Activity_Stream.cpp \
			  Daemon.cpp

libtvdaemon_la_LIBADD = -ludev ${LIBCONFIGXX_LIBS} -lrt -lpthread ${LIBJSONXX_LIBS} ../v4l-utils/lib/libdvbv5/libdvbv5.la -lmatroska ${LIBCCRTP_LIBS} ${LIBAVFORMAT_LIBS} ${LIBAVCODEC_LIBS}
libtvdaemon_la_CXXFLAGS = -D__STDC_CONSTANT_MACROS

INCLUDES = -I$(top_srcdir)/v4l-utils/lib/include


