CFLAGS += $(shell pkg-config --cflags gstreamer-0.10)
CFLAGS += -I../src
CFLAGS += -I.
CFLAGS += -g
LDFLAGS += $(shell pkg-config --libs gstreamer-0.10)
LDFLAGS += -lgstapp-0.10
STATIC_JANSSON = libjansson.a

itvencoder_deppend = itvencoder.c \
                     ../src/itvencoder.c \
		     ../src/itvencoder.h \
		     ../src/config.h \
		     ../src/config.c \
		     ../src/channel.c \
		     ../src/channel.h \
		     ../src/httpserver.c \
		     ../src/httpserver.h \
		     ../src/log.c \
		     ../src/log.h

itvencoder_src = itvencoder.c \
                 ../src/config.c \
		 ../src/itvencoder.c \
		 ../src/channel.c \
		 ../src/httpserver.c \
		 ../src/log.c

all: itvencoder

itvencoder: $(itvencoder_deppend)
	gcc $(CFLAGS) $(LDFLAGS) $(itvencoder_src) $(STATIC_JANSSON) -o itvencoder

config: config-test.c ../src/config.c ../src/config.h
	gcc $(CFLAGS) $(LDFLAGS) config-test.c ../src/config.c $(STATIC_JANSSON) -o config

clean:
	rm itvencoder config
