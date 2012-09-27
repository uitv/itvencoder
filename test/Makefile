CFLAGS += $(shell pkg-config --cflags gstreamer-0.10)
CFLAGS += -I../src
CFLAGS += -I../third/jansson-2.4/src
CFLAGS += -std=c99 -g
LDFLAGS += $(shell pkg-config --libs gstreamer-0.10)
LDFLAGS += -lgstapp-0.10

all: itvencoder decoderpipeline config

itvencoder:
	gcc $(CFLAGS) $(LDFLAGS) itvencoder-test.c ../src/itvencoder.c -o itvencoder

decoderpipeline:
	gcc $(CFLAGS) $(LDFLAGS) decoderpipeline-test.c ../src/decoderpipeline.c -o decoderpipeline

config: config-test.c ../src/config.c ../src/config.h
	gcc $(CFLAGS) $(LDFLAGS) config-test.c ../src/config.c -o config

clean:
	rm itvencoder decoderpipeline config
