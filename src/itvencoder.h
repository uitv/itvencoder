/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __ITVENCODER_H__
#define __ITVENCODER_H__

#include <gst/gst.h>

#include "config.h"
#include "channel.h"

typedef struct _ITVEncoder      ITVEncoder;
typedef struct _ITVEncoderClass ITVEncoderClass;

struct _ITVEncoder {
        GObject         parent;
    
        guint           serve_port;

        guint           percent_cpu_usage;
        guint           memory_usage;
        guint           memory_total;

        GTimeVal        start_time;

        Config          *config;
        guint           total_channel_number;
        guint           working_channel_number; 
        GArray          *channel_array;
};

struct _ITVEncoderClass {
        GObjectClass    parent;
};

#define TYPE_ITVENCODER           (itvencoder_get_type())
#define ITVENCODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ITVENCODER, ITVEncoder))
#define ITVENCODER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_ITVENCODER, ITVEncoderClass))
#define IS_ITVENCODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ITVENCODER))
#define IS_ITVENCODER_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_ITVENCODER))
#define ITVENCODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_ITVENCODER, ITVEncoderClass))
#define itvencoder_new(...)       (g_object_new(TYPE_ITVENCODER, ## __VA_ARGS__, NULL))

GType itvencoder_get_type (void);
GTimeVal itvencoder_get_start_time (ITVEncoder *itvencoder);

#endif /* __ITVENCODER_H__ */
