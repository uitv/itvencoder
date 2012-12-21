INCLUDES = $(gstreamer_CFLAGS) $(gstreamerapp_CFLAGS)

bin_PROGRAMS = itvencoder

itvencoder_LDADD = $(gstreamer_LIBS) $(gstreamerapp_LIBS) libjansson.a

itvencoder_SOURCES = main.c itvencoder.c httpserver.c channel.c config.c log.c

