/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>

#include "jansson.h"

typedef struct _DecoderPipeline   DecoderPipeline;
typedef struct _EncoderPipeline   EncoderPipeline;
typedef struct _Channel           Channel;
typedef struct _ChannelClass      ChannelClass;

struct _DecoderPipeline {
        gchar *pipeline_string;
        GstElement *pipeline;
        GstClockTime last_heartbeat;
};

struct _EncoderPipeline {
        gchar           *pipeline_string;
        GstElement      *pipeline;
        GstClockTime    last_heartbeat;
};

struct _Channel {
        GObject         parent;

        gchar           *name; // same as the name in channel config file
        DecoderPipeline *decoder_pipeline; 
        GArray          *encoder_pipeline_array; 
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
guint channel_set_decoder_pipeline (Channel *channel, gchar *pipeline_string);
guint channel_add_encoder_pipeline (Channel *channel, gchar *pipeline_string);
gint channel_set_decoder_pipeline_state (Channel *channel, GstState state);
gint channel_set_encoder_pipeline_state (Channel *channel, gint index, GstState state);

#endif /* __CHANNEL_H__ */
