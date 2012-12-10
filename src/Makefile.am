INCLUDES = $(gstreamer_CFLAGS)

bin_PROGRAMS = itvencoder

itvencoder_LDADD = $(gstreamer_LIBS) libjansson.a -lgstapp-0.10

itvencoder_SOURCES = main.c itvencoder.c httpserver.c channel.c config.c log.c

