INCLUDES = $(gstreamer_CFLAGS) $(gstreamerapp_CFLAGS) -Isrc/bitstream

bin_PROGRAMS = itvencoder

itvencoder_LDADD = $(gstreamer_LIBS) $(gstreamerapp_LIBS) 

itvencoder_SOURCES = main.c itvencoder.c httpserver.c channel.c log.c httpstreaming.c httpmgmt.c configure.c bitstream/mediainfo.c

