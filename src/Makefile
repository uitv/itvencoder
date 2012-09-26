CFLAGS += $(shell pkg-config --cflags gstreamer-0.10)
CFLAGS += -Imongoose -std=c99 -g
LDFLAGS += $(shell pkg-config --libs gstreamer-0.10)
LDFLAGS += -lgstapp-0.10

all:
	gcc $(CFLAGS) $(LDFLAGS) mongoose/mongoose.c appsink.c itvencoder.c -o appsink

clean:
	rm appsink
