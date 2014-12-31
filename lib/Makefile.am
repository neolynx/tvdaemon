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
			  MPEGTS.cpp \
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
			  CAMClient.cpp \
			  CAMClientHandler.cpp \
			  Daemon.cpp \
			  Avahi_Client.cpp

libtvdaemon_la_LDFLAGS = -ludev ${LIBCONFIGXX_LIBS} -lrt -lpthread ${LIBJSONXX_LIBS} -ldvbv5 \
			${LIBCCRTP_LIBS} \
			../tsdecrypt/libtsdecrypt.la
libtvdaemon_la_CXXFLAGS = -D__STDC_CONSTANT_MACROS -I ../v4l-utils/lib/include

#			-lmatroska ${LIBAVFORMAT_LIBS} ${LIBAVCODEC_LIBS}


