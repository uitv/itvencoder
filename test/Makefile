CFLAGS += $(shell pkg-config --cflags gstreamer-0.10)
CFLAGS += -I../src
CFLAGS += -I../third/jansson-2.4/src
CFLAGS += -I../third/valenok-mongoose-0804ce7
CFLAGS += -std=c99 -g
LDFLAGS += $(shell pkg-config --libs gstreamer-0.10)
LDFLAGS += -lgstapp-0.10
STATIC_JANSSON = ../third/jansson-2.4/src/.libs/libjansson.a

itvencoder_deppend = itvencoder.c \
                     ../src/itvencoder.c \
		     ../src/itvencoder.h \
		     ../src/config.h \
		     ../src/config.c \
		     ../src/channel.c \
		     ../src/channel.h \
		     ../src/httpserver.c \
		     ../src/httpserver.h \
		     ../third/valenok-mongoose-0804ce7/mongoose.c \
		     ../third/valenok-mongoose-0804ce7/mongoose.h

itvencoder_src = itvencoder.c \
                 ../src/config.c \
		 ../src/itvencoder.c \
		 ../src/channel.c \
		 ../src/httpserver.c \
		 ../third/valenok-mongoose-0804ce7/mongoose.c

all: itvencoder config

itvencoder: $(itvencoder_deppend)
	gcc $(CFLAGS) $(LDFLAGS) $(itvencoder_src) $(STATIC_JANSSON) -o itvencoder

config: config-test.c ../src/config.c ../src/config.h
	gcc $(CFLAGS) $(LDFLAGS) config-test.c ../src/config.c $(STATIC_JANSSON) -o config

clean:
	rm itvencoder config
