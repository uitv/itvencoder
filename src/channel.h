/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>

#include "jansson.h"

typedef struct _DecoderPipeline   DecoderPipeline;
typedef struct _Channel           Channel;
typedef struct _ChannelClass      ChannelClass;

struct _DecoderPipeline {
        gchar           *format; // sprintf fmt
        GArray          *parameter_array;
};

struct _Channel {
        GObject         parent;

        gchar           *name; // same as the name in channel config file
        json_t          *config;
        DecoderPipeline *decoder_pipeline; 
};

struct _ChannelClass {
        GObjectClass parent;

        gint (*channel_parse_config_func)(Channel *channel);
};

#define TYPE_CHANNEL           (channel_get_type())
#define CHANNEL(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CHANNEL, Channel))
#define CHANNEL_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_CHANNEL, ChannelClass))
#define IS_CHANNEL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CHANNEL))
#define IS_CHANNEL_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_CHANNEL))
#define CHANNEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_CHANNEL, ChannelClass))
#define channel_new(...)       (g_object_new(TYPE_CHANNEL, ## __VA_ARGS__, NULL))

GType channel_get_type (void);
gint channel_parse_config (Channel *channel);

#endif /* __CHANNEL_H__ */
