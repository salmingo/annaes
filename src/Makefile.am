bin_PROGRAMS=annaes
annaes_SOURCES=daemon.cpp GLog.cpp IOServiceKeep.cpp tcpasio.cpp MessageQueue.cpp \
               ATimeSpace.cpp NTPClient.cpp AsciiProtocol.cpp GeneralControl.cpp annaes.cpp

annaes_LDFLAGS = -L/usr/local/lib
BOOST_LIBS = -lboost_system -lboost_thread-mt -lboost_chrono  -lboost_date_time -lboost_filesystem
annaes_LDADD = -lm -lpthread -lcurl ${BOOST_LIBS}
