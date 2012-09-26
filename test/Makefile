CFLAGS += $(shell pkg-config --cflags gstreamer-0.10)
CFLAGS += -I../src -std=c99 -g
LDFLAGS += $(shell pkg-config --libs gstreamer-0.10)
LDFLAGS += -lgstapp-0.10

all: itvencoder decoderpipeline

itvencoder:
	gcc $(CFLAGS) $(LDFLAGS) itvencoder-test.c ../src/itvencoder.c -o itvencoder

decoderpipeline:
	gcc $(CFLAGS) $(LDFLAGS) decoderpipeline-test.c ../src/decoderpipeline.c -o decoderpipeline

clean:
	rm itvencoder decoderpipeline
