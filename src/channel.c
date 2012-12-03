/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include "channel.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CHANNEL_PROP_0,
        CHANNEL_PROP_NAME,
};

static void channel_class_init (ChannelClass *channelclass);
static void channel_init (Channel *channel);
static GObject *channel_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static GstFlowReturn decoder_appsink_callback (GstAppSink * elt, gpointer user_data);
static GstFlowReturn encoder_appsink_callback (GstAppSink * elt, gpointer user_data);
static void encoder_appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data);
static void encoder_appsrc_enough_data_callback (GstAppSrc *src, gpointer user_data);

static void
channel_class_init (ChannelClass *channelclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(channelclass);
        GParamSpec *param;

        GST_LOG ("channel class init.");

        g_object_class->constructor = channel_constructor;
        g_object_class->set_property = channel_set_property;
        g_object_class->get_property = channel_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of channel",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, CHANNEL_PROP_NAME, param);
}

static void
channel_init (Channel *channel)
{
        GST_LOG ("channel object init");

        channel->system_clock = gst_system_clock_obtain ();
        g_object_set (channel->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        channel->source = g_malloc (sizeof (Source)); //TODO free!
        channel->source->audio_caps = NULL;
        channel->source->video_caps = NULL;
        channel->encoder_array = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

static GObject *
channel_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        GST_LOG ("channel constructor");

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CHANNEL(obj));

        GST_LOG ("channel set property");

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                CHANNEL(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Channel *channel = CHANNEL(obj);

        GST_LOG ("channel get property");

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                g_value_set_string (value, channel->name);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
channel_get_type (void)
{
        static GType type = 0;

        GST_LOG ("channel get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ChannelClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)channel_class_init,
                NULL,
                NULL,
                sizeof (Channel),
                0,
                (GInstanceInitFunc)channel_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Channel", &info, 0);

        return type;
}

static gboolean
bus_callback (GstBus *bus, GstMessage *msg, gpointer data)
{
        gchar *debug;
        GError *error;
        GstState old, new, pending;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
                GST_INFO ("End of stream\n");
                break;
        case GST_MESSAGE_ERROR: 
                gst_message_parse_error (msg, &error, &debug);
                g_free (debug);
                GST_ERROR ("%s error: %s", (gchar *)data, error->message);
                g_error_free (error);
                break;
        case GST_MESSAGE_STATE_CHANGED:
                gst_message_parse_state_changed (msg, &old, &new, &pending);
                GST_INFO ("%s state from %s to %s", (gchar *)data, gst_element_state_get_name (old), gst_element_state_get_name (new));
                break;
        default:
                GST_INFO ("%s message: %s", (gchar *)data, GST_MESSAGE_TYPE_NAME (msg));
                break;
        }

        return TRUE;
}

static GstFlowReturn decoder_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        gchar type = ((SourceAppsinkUserData *)user_data)->type;
        Channel *channel = ((SourceAppsinkUserData *)user_data)->channel;
        guint i;

        GST_LOG ("decoder appsink callback func %c", type);

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        switch (type) {
        case 'a':
                channel->source->last_audio_heartbeat = gst_clock_get_time (channel->system_clock);
                i = channel->source->current_audio_position + 1;
                i = i % AUDIO_RING_SIZE;
                GST_LOG ("audio current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                channel->source->current_audio_position = i;
                if (channel->source->audio_ring[i] != NULL)
                        gst_buffer_unref (channel->source->audio_ring[i]);
                channel->source->audio_ring[i] = buffer;
                channel->source->current_audio_timestamp = GST_BUFFER_TIMESTAMP (buffer);
                break;
        case 'v':
                channel->source->last_video_heartbeat = gst_clock_get_time (channel->system_clock);
                i = channel->source->current_video_position + 1;
                i = i % VIDEO_RING_SIZE;
                GST_LOG ("video current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                channel->source->current_video_position = i;
                if (channel->source->video_ring[i] != NULL)
                        gst_buffer_unref (channel->source->video_ring[i]);
                channel->source->video_ring[i] = buffer;
                channel->source->current_video_timestamp = GST_BUFFER_TIMESTAMP (buffer);
                break;
        default:
                GST_ERROR ("error");
        }
}

guint
channel_set_source (Channel *channel, gchar *pipeline_string)
{
        SourceAppsinkUserData *user_data;

        GST_LOG ("channel set decoder pipeline : %s", pipeline_string);

        channel->source->pipeline_string = pipeline_string;
        channel->source->video_cb_user_data.type = 'v';
        channel->source->video_cb_user_data.channel = channel;
        channel->source->audio_cb_user_data.type = 'a';
        channel->source->audio_cb_user_data.channel = channel;
        channel->source->channel = channel;

        return 0;
}

gint
channel_source_pipeline_initialize (Source *source)
{
        GError *e = NULL;
        GstElement *appsink, *p;
        GstBus *bus;
        GstAppSinkCallbacks appsink_callbacks = {
                NULL,
                NULL,
                decoder_appsink_callback,
                NULL
        };
        gint i;
        gchar *name;

        source->current_audio_position = -1;
        for (i=0; i<AUDIO_RING_SIZE; i++)
                source->audio_ring[i] = NULL;
        source->current_video_position = -1;
        for (i=0; i<VIDEO_RING_SIZE; i++)
                source->video_ring[i] = NULL;

        p = gst_parse_launch (source->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", source->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        name = g_strdup_printf ("%s-%d", "source", source->channel->id); //FIXME
        gst_bus_add_watch (bus, bus_callback, name);

        source->pipeline = p;

        /* set video sink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "videosink"); //TODO release
        if (appsink == NULL) {
                GST_ERROR ("Get video sink error");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, &source->video_cb_user_data, NULL);
        gst_object_unref (appsink);

        /* set audio sink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "audiosink");
        if (appsink == NULL) {
                GST_ERROR ("Get audio sink error");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, &source->audio_cb_user_data, NULL);
        gst_object_unref (appsink);

        return 0;
}

gint
channel_source_pipeline_release (Source *source)
{
        gst_object_unref (source->pipeline);
        source->pipeline = NULL;
}

static
GstFlowReturn encoder_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        Encoder *encoder = (Encoder *)user_data;
        gint i;

        GST_LOG ("encoder appsink callback func");

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        i = encoder->current_output_position + 1;
        i = i % OUTPUT_RING_SIZE;
        GST_DEBUG ("output current position %d, buffer size: %d", i, GST_BUFFER_SIZE(buffer));
        encoder->current_output_position = i;
        if (encoder->output_ring[i] != NULL)
                gst_buffer_unref (encoder->output_ring[i]);
        encoder->output_ring[i] = buffer;
}

static void
encoder_appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data)
{
        gint index = ((EncoderAppsrcUserData *)user_data)->index;
        gchar type = ((EncoderAppsrcUserData *)user_data)->type;
        Channel *channel = ((EncoderAppsrcUserData *)user_data)->channel;
        Encoder *encoder;
        gint i;

        GST_LOG ("encoder %d appsrc need data callback func type %c; length %d", index, type, length);

        encoder = (Encoder *)g_array_index (channel->encoder_array, gpointer, index);
        switch (type) {
        case 'a':
                encoder->audio_enough = FALSE;
                /* next buffer */
                i = encoder->current_audio_position + 1;
                i = i % AUDIO_RING_SIZE;
                for (;;) {
                        encoder->last_audio_heartbeat = gst_clock_get_time (channel->system_clock);
                        /* insure next buffer isn't current decoder buffer */
                        if (i == channel->source->current_audio_position ||
                                channel->source->current_audio_position == -1) { /*FIXME: condition variable*/
                                GST_DEBUG ("waiting audio decoder ready");
                                g_usleep (50000); /* wiating 50ms */
                                continue;
                        }
                        if (encoder->audio_enough) {
                                GST_DEBUG ("audio enough.");
                                break;
                        }
                        GST_DEBUG ("audio encoder position %d; timestamp %" GST_TIME_FORMAT " decoder position %d",
                                   i, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (channel->source->audio_ring[i])),
                                   channel->source->current_audio_position);
                        if (gst_app_src_push_buffer (src, gst_buffer_ref(channel->source->audio_ring[i])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer audio failure.");
                                break;
                        }
                        encoder->current_audio_position = i;
                        i = (i + 1) % AUDIO_RING_SIZE;
                }
                break;
        case 'v':
                encoder->video_enough = FALSE;
                /* next buffer */
                i = encoder->current_video_position + 1;
                i = i % VIDEO_RING_SIZE;
                for (;;) {
                        encoder->last_video_heartbeat = gst_clock_get_time (channel->system_clock);
                        /* insure next buffer isn't current decoder buffer */
                        if (i == channel->source->current_video_position ||
                                channel->source->current_video_position == -1) {
                                GST_DEBUG ("waiting video decoder ready");
                                g_usleep (50000); /* waiting 50ms */
                                continue;
                        }
                        if (encoder->video_enough) {
                                GST_DEBUG ("video enough, break for need data signal.");
                                break;
                        }
                        GST_DEBUG ("video encoder position %d; timestamp %" GST_TIME_FORMAT " decoder position %d",
                                   i, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (channel->source->video_ring[i])),
                                   channel->source->current_video_position);
                        if (gst_app_src_push_buffer (src, gst_buffer_ref(channel->source->video_ring[i])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer video failure.");
                                break;
                        }
                        encoder->current_video_position = i;
                        i = (i + 1) % VIDEO_RING_SIZE;
                }
                break;
        default:
                GST_ERROR ("error");
        }
}

static void
encoder_appsrc_enough_data_callback (GstAppSrc *src, gpointer user_data)
{
        gint index = ((EncoderAppsrcUserData *)user_data)->index;
        gchar type = ((EncoderAppsrcUserData *)user_data)->type;
        Channel *channel = ((EncoderAppsrcUserData *)user_data)->channel;
        Encoder *encoder;

        GST_LOG ("encoder appsrc enough data callback func type %c", type);

        encoder = (Encoder *)g_array_index (channel->encoder_array, gpointer, index);
        switch (type) {
        case 'a':
                encoder->audio_enough = TRUE;
                break;
        case 'v':
                encoder->video_enough = TRUE;
                break;
        default:
                GST_ERROR ("error");
        }
}

guint
channel_add_encoder (Channel *channel, gchar *pipeline_string)
{
        Encoder *encoder;

        encoder = g_malloc (sizeof (Encoder)); //TODO free!
        if (encoder == NULL) {
                GST_ERROR ("g_malloc memeory error.");
                return -1;
        }

        encoder->channel = channel;
        encoder->pipeline_string = pipeline_string;
        encoder->id = channel->encoder_array->len;
        encoder->name = g_strdup_printf ("channel-%d:encoder-%d", channel->id, encoder->id);
        g_array_append_val (channel->encoder_array, encoder);

        encoder->video_cb_user_data.index = encoder->id;
        encoder->video_cb_user_data.type = 'v';
        encoder->video_cb_user_data.channel = channel;

        encoder->audio_cb_user_data.index = encoder->id;
        encoder->audio_cb_user_data.type = 'a';
        encoder->audio_cb_user_data.channel = channel;

        return 0;
}

gint
channel_encoder_pipeline_initialize (Encoder *encoder)
{
        GstElement *p, *appsrc, *appsink;
        GError *e = NULL;
        GstBus *bus;
        GstAppSrcCallbacks callbacks = {
                encoder_appsrc_need_data_callback,
                encoder_appsrc_enough_data_callback,
                NULL
        };
        GstAppSinkCallbacks encoder_appsink_callbacks = {
                NULL,
                NULL,
                encoder_appsink_callback,
                NULL
        };
        gint i;

        GST_LOG ("channel add encoder pipeline : %s", encoder->pipeline_string);

        p = gst_parse_launch (encoder->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", encoder->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        /* set bus watch callback */
        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, encoder->name);

        /* set encoder appsink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "encodersink");
        if (appsink == NULL) {
                GST_ERROR ("%s - Get encoder sink error", encoder->name);
                g_free (encoder);
                return -1;
        }
        encoder->state = GST_STATE_NULL;
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &encoder_appsink_callbacks, encoder, NULL);
        gst_object_unref (appsink);

        /* set video appsrc callback */
        appsrc = gst_bin_get_by_name (GST_BIN (p), "videosrc");
        if (appsrc == NULL) {
                GST_ERROR ("%s - Get video src error", encoder->name);
                g_free (encoder);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, &encoder->video_cb_user_data, NULL);
        gst_object_unref (appsrc);

        /* set audio appsrc callback */
        appsrc = gst_bin_get_by_name (GST_BIN (p), "audiosrc");
        if (appsrc == NULL) {
                GST_ERROR ("%s - Get audio src error", encoder->name);
                g_free (encoder);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, &encoder->audio_cb_user_data, NULL);
        gst_object_unref (appsrc);

        encoder->pipeline = p;
        encoder->current_video_position = -1;
        encoder->current_audio_position = -1;
        encoder->audio_enough = FALSE;
        encoder->video_enough = FALSE;
        encoder->current_output_position = -1;
        for (i=0; i<OUTPUT_RING_SIZE; i++)
                encoder->output_ring[i] = NULL;

        return 0;
}

gint
channel_encoder_pipeline_release (Encoder *encoder)
{
        gst_object_unref (encoder->pipeline);
        encoder->pipeline = NULL;
}

gint
channel_source_appsink_get_caps (Channel *channel)
{
        gint i;

        i = 0;
        while ((channel->source->audio_caps == NULL) || (channel->source->video_caps == NULL)) {
                if (channel->source->audio_ring[0] != NULL) {
                        channel->source->audio_caps = GST_BUFFER_CAPS (channel->source->audio_ring[0]);
                }
                
                if (channel->source->video_ring[0] != NULL) {
                        channel->source->video_caps = GST_BUFFER_CAPS (channel->source->video_ring[0]);
                }
                
                if ((channel->source->audio_caps != NULL) && (channel->source->video_caps != NULL)) {
                        break;
                } else {
                        g_usleep (100000); /* 100 ms */
                }

                if (i++ == 50) break;
        }

        if (channel->source->audio_caps != NULL) {
                GST_WARNING (gst_caps_to_string (channel->source->audio_caps));
        }
        if (channel->source->audio_caps != NULL) {
                GST_WARNING (gst_caps_to_string (channel->source->video_caps));
        }

        if (i < 50 )
                return 0;
        else
                return -1;
}

void
channel_encoder_appsrc_set_caps (Encoder *encoder)
{
        gint i;
        GstElement *appsrc;
        Channel *channel = encoder->channel;

        if (encoder->pipeline != NULL) {
                if (channel->source->video_caps != NULL) {
                        appsrc = gst_bin_get_by_name (GST_BIN (encoder->pipeline), "videosrc");
                        gst_app_src_set_caps ((GstAppSrc *)appsrc, channel->source->video_caps);
                }
                if (channel->source->audio_caps != NULL) {
                        appsrc = gst_bin_get_by_name (GST_BIN (encoder->pipeline), "audiosrc");
                        gst_app_src_set_caps ((GstAppSrc *)appsrc, channel->source->audio_caps);
                }
        }
}

