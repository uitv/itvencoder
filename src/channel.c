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

enum {
        SOURCE_PROP_0,
        SOURCE_PROP_NAME,
        SOURCE_PROP_STATE
};

enum {
        ENCODER_PROP_0,
        ENCODER_PROP_NAME,
        ENCODER_PROP_STATE
};

static void source_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void source_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void encoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void encoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static GstFlowReturn source_appsink_callback (GstAppSink * elt, gpointer user_data);
static GstFlowReturn encoder_appsink_callback (GstAppSink * elt, gpointer user_data);
static void encoder_appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data);
static gint channel_source_appsink_get_caps (Channel *channel);
static void channel_encoder_appsrc_set_caps (Encoder *encoder);

/* Source class */
static void
source_class_init (SourceClass *sourceclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(sourceclass);
        GParamSpec *param;

        g_object_class->set_property = source_set_property;
        g_object_class->get_property = source_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of source",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, SOURCE_PROP_NAME, param);

        param = g_param_spec_int (
                "state",
                "statef",
                "state",
                GST_STATE_VOID_PENDING,
                GST_STATE_PLAYING,
                GST_STATE_VOID_PENDING,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, SOURCE_PROP_STATE, param);
}

static void
source_init (Source *source)
{
        source->audio_caps = NULL;
        source->video_caps = NULL;
        source->sync_error_times = 0;
        source->name = "source-0";
}

GType
source_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (SourceClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)source_class_init,
                NULL,
                NULL,
                sizeof (Source),
                0,
                (GInstanceInitFunc)source_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Source", &info, 0);

        return type;
}

static void
source_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_SOURCE(obj));

        switch(prop_id) {
        case SOURCE_PROP_NAME:
                SOURCE(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case SOURCE_PROP_STATE:
                SOURCE(obj)->state= g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
source_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Source *source = SOURCE(obj);

        switch(prop_id) {
        case SOURCE_PROP_NAME:
                g_value_set_string (value, source->name);
                break;
        case SOURCE_PROP_STATE:
                g_value_set_int (value, source->state);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

/* Encoder class */
static void
encoder_class_init (EncoderClass *encoderclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(encoderclass);
        GParamSpec *param;

        g_object_class->set_property = encoder_set_property;
        g_object_class->get_property = encoder_get_property;

        param = g_param_spec_string (
                "name",
                "name",
                "name of encoder",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ENCODER_PROP_NAME, param);

        param = g_param_spec_int (
                "state",
                "statef",
                "state",
                GST_STATE_VOID_PENDING,
                GST_STATE_PLAYING,
                GST_STATE_VOID_PENDING,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ENCODER_PROP_STATE, param);
}

static void
encoder_init (Encoder *encoder)
{
}

GType
encoder_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (EncoderClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)encoder_class_init,
                NULL,
                NULL,
                sizeof (Encoder),
                0,
                (GInstanceInitFunc)encoder_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Encoder", &info, 0);

        return type;
}

static void
encoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_ENCODER(obj));

        switch(prop_id) {
        case ENCODER_PROP_NAME:
                ENCODER(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case ENCODER_PROP_STATE:
                ENCODER(obj)->state= g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
encoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Encoder *encoder = ENCODER(obj);

        switch(prop_id) {
        case ENCODER_PROP_NAME:
                g_value_set_string (value, encoder->name);
                break;
        case ENCODER_PROP_STATE:
                g_value_set_int (value, encoder->state);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

/* channel class */
static void
channel_class_init (ChannelClass *channelclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(channelclass);
        GParamSpec *param;

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
        channel->system_clock = gst_system_clock_obtain ();
        channel->operate_mutex = g_mutex_new ();
        g_object_set (channel->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        channel->source = source_new (0, NULL); // TODO free!
        channel->encoder_array = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

static void
channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CHANNEL(obj));

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

static void
print_one_tag (const GstTagList * list, const gchar * tag, gpointer user_data)
{
        int i, num;

        num = gst_tag_list_get_tag_size (list, tag);

        for (i = 0; i < num; ++i) {
                const GValue *val;

                /* Note: when looking for specific tags, use the g_tag_list_get_xyz() API,
                * we only use the GValue approach here because it is more generic */
                val = gst_tag_list_get_value_index (list, tag, i);
                if (G_VALUE_HOLDS_STRING (val)) {
                        GST_INFO ("%20s : %s", tag, g_value_get_string (val));
                } else if (G_VALUE_HOLDS_UINT (val)) {
                        GST_INFO ("%20s : %u", tag, g_value_get_uint (val));
                } else if (G_VALUE_HOLDS_DOUBLE (val)) {
                        GST_INFO ("%20s : %g", tag, g_value_get_double (val));
                } else if (G_VALUE_HOLDS_BOOLEAN (val)) {
                        GST_INFO ("%20s : %s", tag, (g_value_get_boolean (val)) ? "true" : "false");
                } else if (GST_VALUE_HOLDS_BUFFER (val)) {
                        GST_INFO ("%20s : buffer of size %u", tag, GST_BUFFER_SIZE (gst_value_get_buffer (val)));
                } else if (GST_VALUE_HOLDS_DATE (val)) {
                        GST_INFO ("%20s : date (year=%u,...)", tag, g_date_get_year (gst_value_get_date (val)));
                } else {
                        GST_INFO ("%20s : tag of type '%s'", tag, G_VALUE_TYPE_NAME (val));
                }
        }
}

static gboolean
bus_callback (GstBus *bus, GstMessage *msg, gpointer user_data)
{
        gchar *debug;
        GError *error;
        GstState old, new, pending;
        GstStreamStatusType type;
        GstClock *clock;
        GstTagList *tags;
        GObject *object = user_data;
        GValue state = { 0, }, name = { 0, };

        
        g_value_init (&name, G_TYPE_STRING);
        g_object_get_property (object, "name", &name);

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
                GST_INFO ("End of stream\n");
                break;
        case GST_MESSAGE_TAG:
                GST_INFO ("TAG");
                gst_message_parse_tag (msg, &tags);
                gst_tag_list_foreach (tags, print_one_tag, NULL);
                break;
        case GST_MESSAGE_ERROR: 
                gst_message_parse_error (msg, &error, &debug);
                g_free (debug);
                GST_WARNING ("%s error: %s", g_value_get_string (&name), error->message);
                g_error_free (error);
                break;
        case GST_MESSAGE_STATE_CHANGED:
                g_value_init (&state, G_TYPE_INT);
                gst_message_parse_state_changed (msg, &old, &new, &pending);
                GST_INFO ("%s state from %s to %s", g_value_get_string (&name), gst_element_state_get_name (old), gst_element_state_get_name (new));
                g_value_set_int (&state, new);
                g_object_set_property (object, "state", &state);
                break;
        case GST_MESSAGE_STREAM_STATUS:
                gst_message_parse_stream_status (msg, &type, NULL);
                GST_INFO ("stream status %d", type);
                break;
        case GST_MESSAGE_NEW_CLOCK:
                gst_message_parse_new_clock (msg, &clock);
                GST_INFO ("New source clock %s", GST_OBJECT_NAME (clock));
                break;
        case GST_MESSAGE_ASYNC_DONE:
                GST_INFO ("source %s message: %s", g_value_get_string (&name), GST_MESSAGE_TYPE_NAME (msg));
                //gst_bin_recalculate_latency (GST_BIN (encoder->pipeline));
                break;
        default:
                GST_INFO ("%s message: %s", g_value_get_string (&name), GST_MESSAGE_TYPE_NAME (msg));
        }

        g_value_unset (&state);
        g_value_unset (&name);
        return TRUE;
}

static GstFlowReturn source_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        gchar type = ((SourceAppsinkUserData *)user_data)->type;
        Channel *channel = ((SourceAppsinkUserData *)user_data)->channel;
        Encoder *encoder;
        guint i, j;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        switch (type) {
        case 'a':
                channel->source->last_audio_heartbeat = gst_clock_get_time (channel->system_clock);
                i = channel->source->current_audio_position + 1;
                i = i % AUDIO_RING_SIZE;
                GST_LOG ("audio current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                for (j=0; j<channel->encoder_array->len; j++) {
                        encoder = g_array_index (channel->encoder_array, gpointer, j);
                        if (i == encoder->current_audio_position) {
                                GST_WARNING ("encoder %s audio encoder cant catch source output speed", encoder->name);
                        }
                }
                channel->source->current_audio_position = i;
                if (channel->source->audio_ring[i] != NULL) {
                        gst_buffer_unref (channel->source->audio_ring[i]);
                }
                channel->source->audio_ring[i] = buffer;
                channel->source->current_audio_timestamp = GST_BUFFER_TIMESTAMP (buffer);
                break;
        case 'v':
                channel->source->last_video_heartbeat = gst_clock_get_time (channel->system_clock);
                i = channel->source->current_video_position + 1;
                i = i % VIDEO_RING_SIZE;
                GST_LOG ("video current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                for (j=0; j<channel->encoder_array->len; j++) {
                        encoder = g_array_index (channel->encoder_array, gpointer, j);
                        if (i == encoder->current_video_position) {
                                GST_WARNING ("encoder %s video encoder cant catch source output speed", encoder->name);
                        }
                }
                channel->source->current_video_position = i;
                if (channel->source->video_ring[i] != NULL) {
                        gst_buffer_unref (channel->source->video_ring[i]);
                }
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
        gint i;

        channel->source->pipeline_string = pipeline_string;
        channel->source->video_cb_user_data.type = 'v';
        channel->source->video_cb_user_data.channel = channel;
        channel->source->audio_cb_user_data.type = 'a';
        channel->source->audio_cb_user_data.channel = channel;
        channel->source->channel = channel;

        for (i=0; i<AUDIO_RING_SIZE; i++) {
                channel->source->audio_ring[i] = NULL;
        }

        for (i=0; i<VIDEO_RING_SIZE; i++) {
                channel->source->video_ring[i] = NULL;
        }

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
                source_appsink_callback,
                NULL
        };

        p = gst_parse_launch (source->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", source->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, source);
        g_object_unref (bus);

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

        source->current_audio_position = -1;
        source->current_video_position = -1;

        source->audio_caps = NULL;
        source->video_caps = NULL;

        return 0;
}

gint
channel_source_pipeline_release (Source *source)
{
        gint i;

        for (i=0; i<AUDIO_RING_SIZE; i++) {
                if (source->audio_ring[i] != NULL) {
                        gst_buffer_unref (source->audio_ring[i]);
                        source->audio_ring[i] = NULL;
                }
        }
        for (i=0; i<VIDEO_RING_SIZE; i++) {
                if (source->video_ring[i] != NULL) {
                        gst_buffer_unref (source->video_ring[i]);
                        source->video_ring[i] = NULL;
                }
        }
#if 0
        if (source->audio_caps != NULL) {
                g_object_unref (source->audio_caps);
                source->audio_caps = NULL;
        }

        if (source->video_caps != NULL) {
                g_object_unref (source->video_caps);
                source->video_caps = NULL;
        }
#endif
        gst_object_unref (source->pipeline);
        source->pipeline = NULL;
}

static
GstFlowReturn encoder_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        Encoder *encoder = (Encoder *)user_data;
        gint i;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        i = encoder->current_output_position + 1;
        i = i % OUTPUT_RING_SIZE;
        encoder->current_output_position = i;
        if (encoder->output_ring[i] != NULL) {
                gst_buffer_unref (encoder->output_ring[i]);
        }
        encoder->output_ring[i] = buffer;
        encoder->output_count++;
}

static void
encoder_appsrc_need_data_callback (GstAppSrc *src, guint length, gpointer user_data)
{
        gint index = ((EncoderAppsrcUserData *)user_data)->index;
        gchar type = ((EncoderAppsrcUserData *)user_data)->type;
        Channel *channel = ((EncoderAppsrcUserData *)user_data)->channel;
        Encoder *encoder;

        encoder = (Encoder *)g_array_index (channel->encoder_array, gpointer, index);
        switch (type) {
        case 'a':
                /* next buffer */
                encoder->current_audio_position = (encoder->current_audio_position + 1) % AUDIO_RING_SIZE;
                for (;;) {
                        encoder->last_audio_heartbeat = gst_clock_get_time (channel->system_clock);
                        /* insure next buffer isn't current buffer */
                        if (encoder->current_audio_position == channel->source->current_audio_position ||
                                channel->source->current_audio_position == -1) { /*FIXME: condition variable*/
                                GST_DEBUG ("waiting audio source ready");
                                g_usleep (50000); /* wiating 50ms */
                                continue;
                        }
                        GST_DEBUG ("audio encoder position %d; timestamp %" GST_TIME_FORMAT " source position %d",
                                   encoder->current_audio_position,
                                   GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (channel->source->audio_ring[encoder->current_audio_position])),
                                   channel->source->current_audio_position);
                        if (gst_app_src_push_buffer (src, gst_buffer_ref (channel->source->audio_ring[encoder->current_audio_position])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer audio failure.");
                                break;
                        }
                        break;
                }
                break;
        case 'v':
                /* next buffer */
                encoder->current_video_position = (encoder->current_video_position + 1) % VIDEO_RING_SIZE;
                for (;;) {
                        encoder->last_video_heartbeat = gst_clock_get_time (channel->system_clock);
                        /* insure next buffer isn't current buffer */
                        if (encoder->current_video_position == channel->source->current_video_position ||
                                channel->source->current_video_position == -1) {
                                GST_DEBUG ("waiting video source ready");
                                g_usleep (50000); /* waiting 50ms */
                                continue;
                        }
                        GST_DEBUG ("video encoder position %d; timestamp %" GST_TIME_FORMAT " source position %d",
                                   encoder->current_video_position,
                                   GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (channel->source->video_ring[encoder->current_video_position])),
                                   channel->source->current_video_position);
                        if (gst_app_src_push_buffer (src, gst_buffer_ref(channel->source->video_ring[encoder->current_video_position])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer video failure.");
                                break;
                        }
                        break;
                }
                break;
        default:
                GST_ERROR ("error");
        }
}

guint
channel_add_encoder (Channel *channel, gchar *pipeline_string)
{
        Encoder *encoder;
        gint i;

        encoder = encoder_new (0, NULL); //TODO free!

        encoder->channel = channel;
        encoder->pipeline_string = pipeline_string;
        encoder->id = channel->encoder_array->len;
        encoder->name = g_strdup_printf ("%s:encoder-%d", channel->name, encoder->id);
        g_array_append_val (channel->encoder_array, encoder);

        encoder->video_cb_user_data.index = encoder->id;
        encoder->video_cb_user_data.type = 'v';
        encoder->video_cb_user_data.channel = channel;

        encoder->audio_cb_user_data.index = encoder->id;
        encoder->audio_cb_user_data.type = 'a';
        encoder->audio_cb_user_data.channel = channel;

        for (i=0; i<OUTPUT_RING_SIZE; i++) {
                encoder->output_ring[i] = NULL;
        }

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
                NULL,
                NULL
        };
        GstAppSinkCallbacks encoder_appsink_callbacks = {
                NULL,
                NULL,
                encoder_appsink_callback,
                NULL
        };

        p = gst_parse_launch (encoder->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", encoder->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        /* set bus watch callback */
        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, encoder);
        g_object_unref (bus);

        /* set encoder appsink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "encodersink");
        if (appsink == NULL) {
                GST_ERROR ("%s - Get encoder sink error", encoder->name);
                return -1;
        }
        encoder->state = GST_STATE_NULL;
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &encoder_appsink_callbacks, encoder, NULL);
        gst_object_unref (appsink);

        /* set video appsrc callback */
        appsrc = gst_bin_get_by_name (GST_BIN (p), "videosrc");
        if (appsrc == NULL) {
                GST_ERROR ("%s - Get video src error", encoder->name);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, &encoder->video_cb_user_data, NULL);
        gst_object_unref (appsrc);

        /* set audio appsrc callback */
        appsrc = gst_bin_get_by_name (GST_BIN (p), "audiosrc");
        if (appsrc == NULL) {
                GST_ERROR ("%s - Get audio src error", encoder->name);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, &encoder->audio_cb_user_data, NULL);
        gst_object_unref (appsrc);

        encoder->pipeline = p;
        encoder->current_video_position = -1;
        encoder->current_audio_position = -1;
        encoder->current_output_position = -1;
        encoder->output_count = 0;

        return 0;
}

gint
channel_encoder_pipeline_release (Encoder *encoder)
{
        gint i;

        for (i=0; i<OUTPUT_RING_SIZE; i++) {
                if (encoder->output_ring[i] != NULL) {
                        gst_buffer_unref (encoder->output_ring[i]);
                        encoder->output_ring[i] = NULL;
                }
        }
        gst_object_unref (encoder->pipeline);
        encoder->pipeline = NULL;

        return 0;
}

static gint
channel_source_appsink_get_caps (Channel *channel)
{
        gint i;
        gchar *caps;

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
                caps = gst_caps_to_string (channel->source->audio_caps);
                GST_WARNING (caps);
                g_free (caps);
        }
        if (channel->source->video_caps != NULL) {
                caps = gst_caps_to_string (channel->source->video_caps);
                GST_WARNING (caps);
                g_free (caps);
        }

        if (i < 50 )
                return 0;
        else
                return 1;
}

static void
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

gint
channel_source_stop (Source *source)
{
        Channel *channel = source->channel;

        gst_element_set_state (source->pipeline, GST_STATE_NULL);
        channel_source_pipeline_release (source);

        return 0;
}

gint
channel_source_start (Source *source)
{
        Channel *channel = source->channel;
        gint i;

        if (channel_source_pipeline_initialize (source) != 0) {
                return -1;
        }

        gst_element_set_state (source->pipeline, GST_STATE_PLAYING);
        if (channel_source_appsink_get_caps (source->channel) != 0) {
                GST_ERROR ("Get source caps failure!");
                channel_source_pipeline_release (source);
                return -1;
        }

        return 0;
}

gint
channel_restart (Channel *channel)
{
        Encoder *encoder;
        gint i;

        for (i=0; i<channel->encoder_array->len; i++) {
                encoder = g_array_index (channel->encoder_array, gpointer, i);
                channel_encoder_stop (encoder);
        }
        channel_source_stop (channel->source);
        channel_source_start (channel->source);
        for (i=0; i<channel->encoder_array->len; i++) {
                encoder = g_array_index (channel->encoder_array, gpointer, i);
                channel_encoder_start (encoder);
        }

        return 0;
}

gint
channel_encoder_stop (Encoder *encoder)
{//FIXME more check is must
        gst_element_set_state (encoder->pipeline, GST_STATE_NULL);
        channel_encoder_pipeline_release (encoder);

        return 0;
}

gint
channel_encoder_start (Encoder *encoder)
{//FIXME more check is must
        channel_encoder_pipeline_initialize (encoder);
        channel_encoder_appsrc_set_caps (encoder);
        gst_element_set_state (encoder->pipeline, GST_STATE_PLAYING);

        return 0;
}

gint
channel_encoder_restart (Encoder *encoder)
{
        channel_encoder_stop (encoder);
        channel_encoder_start (encoder);

        return 0;
}

