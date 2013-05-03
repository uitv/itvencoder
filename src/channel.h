/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>

#define SOURCE_RING_SIZE 1500
#define ENCODER_RING_SIZE (4*250)

typedef struct _Source Source;
typedef struct _SourceClass SourceClass;
typedef struct _Encoder Encoder;
typedef struct _EncoderClass EncoderClass;
typedef struct _Channel Channel;
typedef struct _ChannelClass ChannelClass;

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
} Bin;

typedef struct _SourceStream {
        gchar *name;
        GstBuffer *ring[SOURCE_RING_SIZE];
        gint current_position; // source output position
        GstClockTime current_timestamp;
        GstClock *system_clock;
        GstClockTime last_heartbeat;
        GArray *encoders;
} SourceStream;

struct _Source {
        GObject parent;

        gchar *name;
        GstState state; /* state of the pipeline */
        Channel *channel;
        gchar *pipeline_string;
        GstStructure *configure;
        GSList *bins;
        GstElement *pipeline;

        /*  sync error cause sync_error_times inc, 
         *  sync normal cause sync_error_times reset to zero,
         *  sync_error_times == 5 cause the channel restart.
         * */
        gint sync_error_times;

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
        GstClockTime last_heartbeat;
        SourceStream *source;
} EncoderStream;

struct _Encoder {
        GObject parent;

        gchar *name;
        GstState state; /* state of the pipeline */
        Channel *channel;
        gchar *pipeline_string;
        GstStructure *configure;
        GstElement *pipeline;
        gint id;
        
        GArray *streams;

        GstBuffer *output_ring[ENCODER_RING_SIZE];
        gint current_output_position; // encoder output position
        guint64 output_count; // total output packet counts
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

struct _Channel {
        GObject parent;

        gint id;
        gchar *name; // same as the name in channel config file
        GstStructure *configure;
        Source *source; 
        GArray *encoder_array;

        GstClock *system_clock;
        /* lock befor start, stop or restart encoder, source or channel. */
        GMutex *operate_mutex; 
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
gboolean channel_initialize (Channel *channel, GstStructure *configure);
gboolean channel_start (Channel *channel);

guint channel_set_source (Channel *channel, gchar *pipeline_string);
guint channel_add_encoder (Channel *channel, gchar *pipeline_string);
gint channel_source_stop (Source *source);
gint channel_source_start (Source *source);
gint channel_restart (Channel *channel);
gint channel_encoder_stop (Encoder *encoder);
gint channel_encoder_start (Encoder *encoder);
gint channel_encoder_restart (Encoder *encoder);
Encoder * channel_get_encoder (gchar *uri, GArray *channel);
Channel * channel_get_channel (gchar *uri, GArray *channel);

#endif /* __CHANNEL_H__ */
