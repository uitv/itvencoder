/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __ITVENCODER_H__
#define __ITVENCODER_H__

#include <gst/gst.h>

#include "config.h"

#include "configure.h"
#include "channel.h"
#include "httpstreaming.h"

#define SYNC_THRESHHOLD 3000000000 /* 1000ms */
#define HEARTBEAT_THRESHHOLD 7000000000 /* 2000ms */

typedef struct _ITVEncoder      ITVEncoder;
typedef struct _ITVEncoderClass ITVEncoderClass;

struct _ITVEncoder {
        GObject parent;
    
        GstClock *system_clock;
        GstClockTime start_time;
        guint64 last_utime; // last process user time.
        guint64 last_stime; // last process system time.
        guint64 last_ctime; // last process cpu time.
        guint64 start_ctime; // cpu time at process start.

//        Config *config;
        GstStructure *configure;
        GArray *channel_array;
        HTTPStreaming *httpstreaming; /* streaming via http */
};

struct _ITVEncoderClass {
        GObjectClass parent;
};

#define TYPE_ITVENCODER           (itvencoder_get_type())
#define ITVENCODER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ITVENCODER, ITVEncoder))
#define ITVENCODER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_ITVENCODER, ITVEncoderClass))
#define IS_ITVENCODER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ITVENCODER))
#define IS_ITVENCODER_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_ITVENCODER))
#define ITVENCODER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_ITVENCODER, ITVEncoderClass))
#define itvencoder_new(...)       (g_object_new(TYPE_ITVENCODER, ## __VA_ARGS__, NULL))

GType itvencoder_get_type (void);
GstClockTime itvencoder_get_start_time (ITVEncoder *itvencoder);
gboolean itvencoder_load_configure (ITVEncoder *itvencoder, GstStructure *configure);
gboolean itvencoder_channel_initialize (ITVEncoder *itvencoder, gchar *name);
gboolean itvencoder_channel_start (ITVEncoder *itvencoder, gchar *name);
gint itvencoder_start (ITVEncoder *itvencoder);

#endif /* __ITVENCODER_H__ */
