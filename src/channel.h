/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <gst/gst.h>

#define AUDIO_RING_SIZE 256 //TODO: configuration or computer from codec type.
#define VIDEO_RING_SIZE 128
#define OUTPUT_RING_SIZE (1024*348) // 348*188 = 65424 ~= 64k

typedef struct _Source   Source;
typedef struct _Encoder Encoder;
typedef struct _Channel           Channel;
typedef struct _ChannelClass      ChannelClass;

struct _Source {
        gchar *pipeline_string;
        GstElement *pipeline;
        GstClockTime last_video_heartbeat;
        GstClockTime last_audio_heartbeat;

        /* decoder produce, encoder consume */
        GstCaps *audio_caps;
        GstBuffer *audio_ring[AUDIO_RING_SIZE];
        gint current_audio_position; // decoder write position
        GstClockTime current_audio_timestamp;

        GstCaps *video_caps;
        GstBuffer *video_ring[VIDEO_RING_SIZE];
        gint current_video_position; // decoder write position
        GstClockTime current_video_timestamp;
};

struct _Encoder {
        gchar *pipeline_string;
        GstElement *pipeline;
        GstState state; /* state of the pipeline */
        GstClockTime last_video_heartbeat;
        GstClockTime last_audio_heartbeat;
        
        gint current_video_position; // encoder read position
        gboolean video_enough; /* appsrc enaugh_data signal */
        gint current_audio_position; // encoder read position
        gboolean audio_enough;

        GstBuffer *output_ring[OUTPUT_RING_SIZE];
        gint current_output_position; // encoder output position
};

struct _Channel {
        GObject parent;

        gchar *name; // same as the name in channel config file
        Source *decoder_pipeline; 
        GArray *encoder_array;

        GstClock *system_clock;
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
guint channel_add_encoder (Channel *channel, gchar *pipeline_string);
gint channel_get_decoder_appsink_caps (Channel *channel);
void channel_set_encoder_appsrc_caps (Channel *channel);
gint channel_set_decoder_pipeline_state (Channel *channel, GstState state);
gint channel_set_encoder_pipeline_state (Channel *channel, gint index, GstState state);

#endif /* __CHANNEL_H__ */
