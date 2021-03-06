/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>
#include <semaphore.h>
#include "log.h"

#define SOURCE_RING_SIZE 250
#define STREAM_NAME_LEN 32
#define SHM_SIZE 16*1024*1024

#define ITVENCODER_LOGO \
"     _ _________      ________                     _                \n" \
"    (_)__   __\\ \\    / /  ____|                   | |             \n" \
"     _   | |   \\ \\  / /| |__   _ __   ___ ___   __| | ___ _ __    \n" \
"    | |  | |    \\ \\/ / |  __| | '_ \\ / _ / _ \\ / _` |/ _ \\ '__|\n" \
"    | |  | |     \\  /  | |____| | | | (_| (_) | (_| |  __/ |       \n" \
"    |_|  |_|      \\/   |______|_| |_|\\___\\___/ \\__,_|\\___|_|   "

typedef struct _Source Source;
typedef struct _SourceClass SourceClass;
typedef struct _Encoder Encoder;
typedef struct _EncoderClass EncoderClass;
typedef struct _EncoderOutput EncoderOutput;
typedef struct _ChannelOutput ChannelOutput;
typedef struct _EncoderConsumption EncoderConsumption;
typedef struct _ChannelConsumption ChannelConsumption;
typedef struct _Channel Channel;
typedef struct _ChannelClass ChannelClass;

enum StreamType {
        ST_UNKNOWN,
        ST_VIDEO,
        ST_AUDIO,
        ST_SUBTITLE
};

typedef struct _Link {
        GstElement *src;
        GstElement *sink;
        gchar *src_name;
        gchar *sink_name;
        gchar *src_pad_name;
        gchar *sink_pad_name;
} Link;

typedef struct _Bin {
        gchar *name;
        GSList *elements;
        GstElement *first;
        GstElement *last;
        GSList *links;
        Link *previous;
        Link *next;
        gulong signal_id;
        GstStructure *configure;
} Bin;

typedef struct _SourceStream {
        gchar *name;
        gchar *streamcaps;
        guint64 *type;
        GstBuffer *ring[SOURCE_RING_SIZE];
        gint current_position; // source output position
        GstClockTime *current_timestamp;
        GstClock *system_clock;
        GstClockTime *last_heartbeat;
        GArray *encoders;
} SourceStream;

struct _Source {
        GObject parent;

        gchar *name;
        GstState state; /* state of the pipeline */
        Channel *channel;
        GstStructure *configure;
        GSList *bins;
        GstElement *pipeline;

        GArray *streams;
};

struct _SourceClass {
        GObjectClass parent;
};

#define TYPE_SOURCE           (source_get_type())
#define SOURCE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SOURCE, Source))
#define SOURCE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_SOURCE, SourceClass))
#define IS_SOURCE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SOURCE))
#define IS_SOURCE_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_SOURCE))
#define SOURCE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_SOURCE, SourceClass))
#define source_new(...)       (g_object_new(TYPE_SOURCE, ## __VA_ARGS__, NULL))

GType source_get_type (void);

typedef struct _EncoderStream {
        gchar *name;
        gint current_position; // encoder position
        GstClock *system_clock;
        GstClockTime *current_timestamp;
        GstClockTime *last_heartbeat;
        gchar *source_name;
        SourceStream *source;
} EncoderStream;

struct _Encoder {
        GObject parent;

        gchar *name;
        GstState state; /* state of the pipeline */
        GstClockTime *output_heartbeat;
        Channel *channel;
        GstStructure *configure;
        GSList *bins;
        GstElement *pipeline;
        gint id;
        
        GArray *streams;

        sem_t *mutex; // access of encoder output should be exclusive.
        gchar *cache_addr;
        guint64 cache_size; // total output packet counts
        guint64 *total_count; // total output packet counts
        guint64 *head_addr;
        guint64 *tail_addr;
        guint64 *last_rap_addr;
};

struct _EncoderClass {
        GObjectClass parent;
};

#define TYPE_ENCODER           (encoder_get_type())
#define ENCODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ENCODER, Encoder))
#define ENCODER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_ENCODER, EncoderClass))
#define IS_ENCODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ENCODER))
#define IS_ENCODER_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_ENCODER))
#define ENCODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_ENCODER, EncoderClass))
#define encoder_new(...)       (g_object_new(TYPE_ENCODER, ## __VA_ARGS__, NULL))

GType encoder_get_type (void);

struct _EncoderOutput {
        gchar name[STREAM_NAME_LEN];
        gint64 stream_count;
        GstClockTime *heartbeat;
        struct _EncoderStreamState {
                gchar name[STREAM_NAME_LEN];
                guint64 type;
                GstClockTime current_timestamp;
                GstClockTime last_heartbeat;
        } *streams;
        gchar *cache_addr;
        guint64 cache_size;
        guint64 *total_count; // total output packet counts
        guint64 *head_addr;
        guint64 *tail_addr;
        guint64 *last_rap_addr; // last random access point address
};

struct _ChannelOutput {
        guint64 *state;
        struct _SourceState {
                /*  sync error cause sync_error_times inc, 
                 *  sync normal cause sync_error_times reset to zero,
                 *  sync_error_times == 5 cause the channel restart.
                 */
                guint64 sync_error_times;
                gint64 stream_count;
                struct _SourceStreamState {
                        gchar name[STREAM_NAME_LEN];
                        /* enum StreamType */
                        guint64 type;
                        GstClockTime current_timestamp;
                        GstClockTime last_heartbeat;
                } *streams;
        } source;
        gint64 encoder_count;
        EncoderOutput *encoders;
};

struct _Channel {
        GObject parent;

        GstClock *system_clock;
        gint id;
        gchar *name; // same as the name in channel config file
        gboolean enable;
        GMutex *configure_mutex;
        GstStructure *configure;
        gint sscount, shm_size;
        GArray *escountlist;
        ChannelOutput *output; // Interface for producing.
        Source *source; 
        GArray *encoder_array;
        gint64 age; // (re)start times of the channel.

        pid_t worker_pid;

        guint64 last_utime; // last process user time.
        guint64 last_stime; // last process system time.
        guint64 last_ctime; // last process cpu time.
        guint64 start_ctime; // cpu time at process start.
};

struct _ChannelClass {
        GObjectClass parent;
};

#define TYPE_CHANNEL           (channel_get_type())
#define CHANNEL(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CHANNEL, Channel))
#define CHANNEL_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_CHANNEL, ChannelClass))
#define IS_CHANNEL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CHANNEL))
#define IS_CHANNEL_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_CHANNEL))
#define CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_CHANNEL, ChannelClass))
#define channel_new(...)       (g_object_new(TYPE_CHANNEL, ## __VA_ARGS__, NULL))

GType channel_get_type (void);
gint channel_setup (Channel *channel, gboolean daemon);
gint channel_reset (Channel *channel);
gint channel_start (Channel *channel, gboolean daemon);
gint channel_stop (Channel *channel, gint sig);

#endif /* __CHANNEL_H__ */
