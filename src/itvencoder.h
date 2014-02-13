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

#define SYNC_THRESHHOLD 3000000000 /* 1000ms */
#define HEARTBEAT_THRESHHOLD 7000000000 /* 2000ms */
#define ENCODER_OUTPUT_HEARTBEAT_THRESHHOLD 30000000000 /* 30s */

#define LOG_SIZE 64*1024*1024
#define LOG_ROTATE 2048

typedef struct _ITVEncoder      ITVEncoder;
typedef struct _ITVEncoderClass ITVEncoderClass;

struct _ITVEncoder {
        GObject parent;
    
        /* run as daemon */
        gboolean daemon;
        GstClock *system_clock;
        GstClockTime start_time;

        gchar *configure_file;
        GMutex *configure_mutex; //GRWLock configure_rwlock;
        Configure *configure;
        gchar *log_dir;
        GArray *channel_array;
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
gint itvencoder_reload_configure (ITVEncoder *itvencoder);
gint itvencoder_channel_initialize (ITVEncoder *itvencoder);
gint itvencoder_channel_start (ITVEncoder *itvencoder, gint index);
gint itvencoder_channel_stop (ITVEncoder *itvencoder, gint index, gint sig);
gint itvencoder_stop (ITVEncoder *itvencoder);
gint itvencoder_url_channel_index (gchar *url);
gint itvencoder_url_encoder_index (gchar *url);

#endif /* __ITVENCODER_H__ */