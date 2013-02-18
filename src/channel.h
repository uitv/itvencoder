/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>

#define AUDIO_RING_SIZE 3000
#define VIDEO_RING_SIZE 1500
#define OUTPUT_RING_SIZE (4*250)

typedef struct _Source Source;
typedef struct _Encoder Encoder;
typedef struct _Channel Channel;
typedef struct _ChannelClass ChannelClass;

typedef struct _BusCallbackUserData {
        gchar type;
        gpointer user_data;
} BusCallbackUserData;

typedef struct _SourceAppsinkUserData {
        gchar type;
        Channel *channel;
} SourceAppsinkUserData;

typedef struct _EncoderAppsrcUserData {
        gint index;
        gchar type;
        Channel *channel;
} EncoderAppsrcUserData;

struct _Source {
        Channel *channel;
        BusCallbackUserData bus_cb_user_data;
        gchar *pipeline_string;
        GstElement *pipeline;
        GstState state; /* state of the pipeline */

        /*  sync error cause sync_error_times inc, 
         *  sync normal cause sync_error_times reset to zero,
         *  sync_error_times == 5 cause the channel restart.
         * */
        gint sync_error_times;

        /* source produce, encoder consume */
        SourceAppsinkUserData audio_cb_user_data;
        GstCaps *audio_caps;
        GstBuffer *audio_ring[AUDIO_RING_SIZE];
        gint current_audio_position; // source write position
        GstClockTime current_audio_timestamp;
        GstClockTime last_audio_heartbeat;

        SourceAppsinkUserData video_cb_user_data;
        GstCaps *video_caps;
        GstBuffer *video_ring[VIDEO_RING_SIZE];
        gint current_video_position; // source write position
        GstClockTime current_video_timestamp;
        GstClockTime last_video_heartbeat;
};

struct _Encoder {
        Channel *channel;
        BusCallbackUserData bus_cb_user_data;
        gchar *pipeline_string;
        GstElement *pipeline;
        gint id;
        gchar *name;
        GstState state; /* state of the pipeline */
        
        EncoderAppsrcUserData video_cb_user_data; /* video appsrc callback user_data */
        gint current_video_position; // encoder read position
        gboolean video_enough; /* appsrc enaugh_data signal */
        GstClockTime last_video_heartbeat;

        EncoderAppsrcUserData audio_cb_user_data; /* audio appsrc callback user_data */
        gint current_audio_position; // encoder read position
        gboolean audio_enough;
        GstClockTime last_audio_heartbeat;

        GstBuffer *output_ring[OUTPUT_RING_SIZE];
        gint current_output_position; // encoder output position
        guint64 output_count; // total output packet counts
};

struct _Channel {
        GObject parent;

        gint id;
        gchar *name; // same as the name in channel config file
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
guint channel_set_source (Channel *channel, gchar *pipeline_string);
guint channel_add_encoder (Channel *channel, gchar *pipeline_string);
gint channel_source_stop (Source *source);
gint channel_source_start (Source *source);
gint channel_restart (Channel *channel);
gint channel_encoder_stop (Encoder *encoder);
gint channel_encoder_start (Encoder *encoder);
gint channel_encoder_restart (Encoder *encoder);

#endif /* __CHANNEL_H__ */
