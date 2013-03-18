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
static gint channel_encoder_appsrc_set_caps (Encoder *encoder);
static gboolean channel_source_stop_func (gpointer *user_data);
static gboolean channel_source_start_func (gpointer *user_data);
static gboolean channel_restart_func (gpointer *user_data);
static gboolean channel_encoder_stop_func (gpointer *user_data);
static gboolean channel_encoder_start_func (gpointer *user_data);
static gboolean channel_encoder_restart_func (gpointer *user_data);

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
        source->streams = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

GType
source_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (SourceClass), // class size
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)source_class_init, // class init
                NULL, // class finalize
                NULL, // class data
                sizeof (Source), //instance size
                0, // n_preallocs
                (GInstanceInitFunc)source_init, //instance_init
                NULL // value_table
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
        encoder->streams = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
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
                GST_INFO ("pipeline %s element %s state from %s to %s", g_value_get_string (&name), GST_MESSAGE_SRC_NAME (msg), gst_element_state_get_name (old), gst_element_state_get_name (new));
                if (g_strcmp0 (g_value_get_string (&name), GST_MESSAGE_SRC_NAME (msg)) == 0) {
                        GST_INFO ("pipeline %s state change to %s", g_value_get_string (&name), gst_element_state_get_name (new));
                        g_value_set_int (&state, new);
                        g_object_set_property (object, "state", &state);
                        g_value_unset (&state);
                }
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

        g_value_unset (&name);
        return TRUE;
}

static GstFlowReturn
source_appsink_callback (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        SourceStream *stream = (SourceStream *)user_data;
        EncoderStream *encoder;
        gint i;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        stream->last_heartbeat = gst_clock_get_time (stream->system_clock);
        stream->current_position = (stream->current_position + 1) % SOURCE_RING_SIZE;

        /* output running status */
        GST_DEBUG ("%s current position %d, buffer duration: %d", stream->name, stream->current_position, GST_BUFFER_DURATION(buffer));
        for (i = 0; i < stream->encoders->len; i++) {
                encoder = g_array_index (stream->encoders, gpointer, i);
                if (stream->current_position == encoder->current_position) {
                        GST_WARNING ("encoder %s audio encoder cant catch source %s output.", encoder->name, stream->name);
                }
        }

        /* out a buffer */
        if (stream->ring[stream->current_position] != NULL) {
                gst_buffer_unref (stream->ring[stream->current_position]);
        }
        stream->ring[stream->current_position] = buffer;
        stream->current_timestamp = GST_BUFFER_TIMESTAMP (buffer);
}

static gint
channel_source_extract_streams (Source *source)
{
        GRegex *regex;
        GMatchInfo *match_info;
        SourceStream *stream;

        regex = g_regex_new ("appsink name=(?<stream>[^ ]*)", G_REGEX_OPTIMIZE, 0, NULL);
        if (regex == NULL) {
                GST_ERROR ("bad regular expression");
                return -1;
        }
        g_regex_match (regex, source->pipeline_string, 0, &match_info);
        g_regex_unref (regex);

        while (g_match_info_matches (match_info)) {
                stream = (SourceStream *)g_malloc (sizeof (SourceStream));
                stream->name = g_match_info_fetch_named (match_info, "stream");
                GST_INFO ("stream found %s", stream->name);
                g_array_append_val (source->streams, stream);
                g_match_info_next (match_info, NULL);
        }
}

guint
channel_set_source (Channel *channel, gchar *pipeline_string)
{
        gint i, j;
        SourceStream *stream;

        channel->source->sync_error_times = 0;
        channel->source->name = channel->name;
        channel->source->pipeline_string = pipeline_string;
        channel->source->channel = channel;
        channel_source_extract_streams (channel->source);

        for (i = 0; i < channel->source->streams->len; i++) {
                stream = g_array_index (channel->source->streams, gpointer, i);
                stream->system_clock = channel->system_clock;
                stream->encoders = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
        }

        for (i = 0; i < channel->source->streams->len; i++) {
                stream = g_array_index (channel->source->streams, gpointer, i);
                for (j = 0; j < SOURCE_RING_SIZE; j++) {
                        stream->ring[j] = NULL;
                }
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
        gint i, j;
        SourceStream *stream;

        p = gst_parse_launch (source->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", source->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }
        gst_element_set_name (p, source->name);

        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, source);
        g_object_unref (bus);

        source->pipeline = p;

        for (i = 0; i < source->streams->len; i++) {
                stream = g_array_index (source->streams, gpointer, i);
                for (j = 0; j < SOURCE_RING_SIZE; j++) {
                        stream->ring[j] = NULL;
                }
                stream->current_position = -1;
                appsink = gst_bin_get_by_name (GST_BIN (p), stream->name); //TODO release
                if (appsink == NULL) {
                        GST_ERROR ("Get %s sink error", stream->name);
                        return -1;
                }
                gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, stream, NULL);
                gst_object_unref (appsink);
        }

        return 0;
}

gint
channel_source_pipeline_release (Source *source)
{
        gint i, j;
        SourceStream *stream;

        for (i = 0; i < source->streams->len; i++) {
                stream = g_array_index (source->streams, gpointer, i);
                for (j = 0; j < SOURCE_RING_SIZE; j++) {
                        if (stream->ring[j] != NULL) {
                                gst_buffer_unref (stream->ring[j]);
                                stream->ring[j] = NULL;
                        }
                }
        }

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
        i = i % ENCODER_RING_SIZE;
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
        EncoderStream *stream = (EncoderStream *)user_data;

        stream->current_position = (stream->current_position + 1) % SOURCE_RING_SIZE;
        for (;;) {
                stream->last_heartbeat = gst_clock_get_time (stream->system_clock);
                /* insure next buffer isn't current buffer */
                if (stream->current_position == stream->source->current_position ||
                        stream->source->current_position == -1) { /*FIXME: condition variable*/
                        GST_DEBUG ("waiting %s source ready", stream->name);
                        g_usleep (50000); /* wiating 50ms */
                        continue;
                }
                GST_DEBUG ("%s encoder position %d; timestamp %" GST_TIME_FORMAT " source position %d",
                        stream->name,   
                        stream->current_position,
                        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (stream->source->ring[stream->current_position])),
                        stream->source->current_position);
                if (gst_app_src_push_buffer (src, gst_buffer_ref (stream->source->ring[stream->current_position])) != GST_FLOW_OK) {
                        GST_ERROR ("%s, gst_app_src_push_buffer failure.", stream->name);
                }
                break;
        }
}

static gint
channel_encoder_extract_streams (Encoder *encoder)
{
        GRegex *regex;
        GMatchInfo *match_info;
        EncoderStream *stream;

        regex = g_regex_new ("appsrc name=(?<stream>[^ ]*)", G_REGEX_OPTIMIZE, 0, NULL);
        if (regex == NULL) {
                GST_ERROR ("bad regular expression");
                return -1;
        }
        g_regex_match (regex, encoder->pipeline_string, 0, &match_info);
        g_regex_unref (regex);

        while (g_match_info_matches (match_info)) {
                stream = (EncoderStream *)g_malloc (sizeof (EncoderStream));
                stream->name = g_match_info_fetch_named (match_info, "stream");
                GST_INFO ("stream found %s", stream->name);
                g_array_append_val (encoder->streams, stream);
                g_match_info_next (match_info, NULL);
        }
}

guint
channel_add_encoder (Channel *channel, gchar *pipeline_string)
{
        Encoder *encoder;
        SourceStream *source;
        EncoderStream *stream;
        gint i, j;

        encoder = encoder_new (0, NULL); //TODO free!

        encoder->channel = channel;
        encoder->pipeline_string = pipeline_string;
        encoder->id = channel->encoder_array->len;
        encoder->name = g_strdup_printf ("%s:encoder-%d", channel->name, encoder->id);
        channel_encoder_extract_streams (encoder);
        g_array_append_val (channel->encoder_array, encoder);

        for (i = 0; i < encoder->streams->len; i++) {
                stream = g_array_index (encoder->streams, gpointer, i);
                for (j = 0; j < encoder->channel->source->streams->len; j++) {
                        source = g_array_index (encoder->channel->source->streams, gpointer, j);
                        if (g_strcmp0 (source->name, stream->name) == 0) {
                                stream->source = source;
                                g_array_append_val (source->encoders, stream);
                                break;
                        }
                }
                if (stream->source == NULL) {
                        GST_ERROR ("%s: cant find source.", stream->name);
                        return -1;
                }
        }

        for (i=0; i<ENCODER_RING_SIZE; i++) {
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
        SourceStream *source;
        EncoderStream *stream;
        gint i, j;

        p = gst_parse_launch (encoder->pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", encoder->pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }
        gst_element_set_name (p, encoder->name);

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

        for (i = 0; i < encoder->streams->len; i++) {
                stream = g_array_index (encoder->streams, gpointer, i);
                stream->current_position = -1;
                stream->system_clock = encoder->channel->system_clock;
                for (j = 0; j < encoder->channel->source->streams->len; j++) {
                        source = g_array_index (encoder->channel->source->streams, gpointer, j);
                        if (g_strcmp0 (source->name, stream->name) == 0) {
                                stream->source = source;
                                g_array_append_val (source->encoders, stream);
                                break;
                        }
                }
                if (stream->source == NULL) {
                        GST_ERROR ("%s: cant find source.", stream->name);
                        return -1;
                }
                appsrc = gst_bin_get_by_name (GST_BIN (p), stream->name);
                if (appsrc == NULL) {
                        GST_ERROR ("%s - Get stream %s error", encoder->name, stream->name);
                        return -1;
                }
                gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, stream, NULL);
                gst_object_unref (appsrc);
        }

        encoder->pipeline = p;
        encoder->current_output_position = -1;
        encoder->output_count = 0;

        return 0;
}

gint
channel_encoder_pipeline_release (Encoder *encoder)
{
        gint i;

        for (i=0; i<ENCODER_RING_SIZE; i++) {
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
channel_encoder_appsrc_set_caps (Encoder *encoder)
{
        gint i;
        GstElement *appsrc;
        EncoderStream *stream;
        GstCaps *caps;

        for (i = 0; i < encoder->streams->len; i++) {
                stream = g_array_index (encoder->streams, gpointer, i);
                while (stream->source->ring[0] == NULL) {
                        GST_WARNING ("encoder %s source stream %s is not ready. waiting...", encoder->name, stream->name);
                        g_usleep (1000000);
                        continue;
                }
                caps = GST_BUFFER_CAPS (stream->source->ring[0]);
                appsrc = gst_bin_get_by_name (GST_BIN (encoder->pipeline), stream->name);
                gst_app_src_set_caps ((GstAppSrc *)appsrc, caps);
        }
}

static gboolean
channel_source_stop_func (gpointer *user_data)
{
        Source *source = (Source *)user_data;
        Channel *channel = source->channel;
        Encoder *encoder;
        gint i;

        if (!g_mutex_trylock (channel->operate_mutex)) {
                GST_WARNING ("Try lock channel %s restart lock failure!", channel->name);
                return TRUE;
        }

        for (i = 0; i < channel->encoder_array->len; i++) {
                encoder = g_array_index (channel->encoder_array, gpointer, i);
                encoder->state = GST_STATE_VOID_PENDING;
                gst_element_set_state (encoder->pipeline, GST_STATE_NULL);
                channel_encoder_pipeline_release (encoder);
        }

        source->state = GST_STATE_VOID_PENDING;
        gst_element_set_state (source->pipeline, GST_STATE_NULL);
        channel_source_pipeline_release (source);

        g_mutex_unlock (channel->operate_mutex);

        return FALSE;
}

gint
channel_source_stop (Source *source)
{
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_source_stop_func, (gpointer) source, NULL);
        return 0;
}

static gboolean
channel_source_start_func (gpointer *user_data)
{
        Source *source = (Source *)user_data;
        Channel *channel = source->channel;
        Encoder *encoder;
        gint i;

        if (!g_mutex_trylock (channel->operate_mutex)) {
                GST_WARNING ("start source %s, try lock channel %s failure!", source->name, channel->name);
                g_usleep (1000000);
                return TRUE;
        }

        if (channel_source_pipeline_initialize (source) != 0) {
                return TRUE;
        }

        gst_element_set_state (source->pipeline, GST_STATE_PLAYING);

        g_mutex_unlock (channel->operate_mutex);

        return FALSE;
}

gint
channel_source_start (Source *source)
{
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_source_start_func, (gpointer) source, NULL);
        return 0;
}

static gboolean
channel_restart_func (gpointer *user_data)
{//FIXME
        Channel *channel = (Channel *)user_data;
        Encoder *encoder;
        gint i;

        if (!g_mutex_trylock (channel->operate_mutex)) {
                GST_WARNING ("Try lock channel %s restart lock failure!", channel->name);
                return TRUE;
        }

        /* stop encoders */
        for (i=0; i<channel->encoder_array->len; i++) {
                encoder = g_array_index (channel->encoder_array, gpointer, i);
                encoder->state = GST_STATE_VOID_PENDING;
                gst_element_set_state (encoder->pipeline, GST_STATE_NULL);
                channel_encoder_pipeline_release (encoder);
        }

        /* stop source */
        channel->source->state = GST_STATE_VOID_PENDING;
        gst_element_set_state (channel->source->pipeline, GST_STATE_NULL);
        channel_source_pipeline_release (channel->source);

        /* start source */
        channel_source_pipeline_initialize (channel->source);
        gst_element_set_state (channel->source->pipeline, GST_STATE_PLAYING);

        /* start encoders */
        for (i=0; i<channel->encoder_array->len; i++) {
                encoder = g_array_index (channel->encoder_array, gpointer, i);
                channel_encoder_pipeline_initialize (encoder);
                channel_encoder_appsrc_set_caps (encoder);
                gst_element_set_state (encoder->pipeline, GST_STATE_PLAYING);
        }

        g_mutex_unlock (channel->operate_mutex);

        return FALSE;
}

gint
channel_restart (Channel *channel)
{
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_restart_func, (gpointer) channel, NULL);
        return 0;
}

static gboolean
channel_encoder_stop_func (gpointer *user_data)
{
        Encoder *encoder = (Encoder *)user_data;

        if (!g_mutex_trylock (encoder->channel->operate_mutex)) {
                GST_WARNING ("Try lock channel %s restart lock failure!", encoder->channel->name);
                return TRUE;
        }

        encoder->state = GST_STATE_VOID_PENDING;
        gst_element_set_state (encoder->pipeline, GST_STATE_NULL);
        channel_encoder_pipeline_release (encoder);

        g_mutex_unlock (encoder->channel->operate_mutex);

        return FALSE;
}

gint
channel_encoder_stop (Encoder *encoder)
{//FIXME more check is must
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_encoder_stop_func, (gpointer) encoder, NULL);
        return 0;
}

static gboolean
channel_encoder_start_func (gpointer *user_data)
{
        Encoder *encoder = (Encoder *)user_data;

        if (!g_mutex_trylock (encoder->channel->operate_mutex)) {
                GST_WARNING ("Try lock channel %s lock failure!", encoder->channel->name);
                g_usleep (1000000);
                return TRUE;
        }

        channel_encoder_pipeline_initialize (encoder);
        channel_encoder_appsrc_set_caps (encoder);
        gst_element_set_state (encoder->pipeline, GST_STATE_PLAYING);

        g_mutex_unlock (encoder->channel->operate_mutex);

        return FALSE;
}

gint
channel_encoder_start (Encoder *encoder)
{//FIXME more check is must
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_encoder_start_func, (gpointer) encoder, NULL);
        return 0;
}

static gboolean
channel_encoder_restart_func (gpointer *user_data)
{
        Encoder *encoder = (Encoder *)user_data;

        if (!g_mutex_trylock (encoder->channel->operate_mutex)) {
                GST_WARNING ("Try lock channel %s restart lock failure!", encoder->channel->name);
                return TRUE;
        }

        /* stop encoder */
        encoder->state = GST_STATE_VOID_PENDING;
        gst_element_set_state (encoder->pipeline, GST_STATE_NULL);
        channel_encoder_pipeline_release (encoder);

        /* start encoder */
        channel_encoder_pipeline_initialize (encoder);
        channel_encoder_appsrc_set_caps (encoder);
        gst_element_set_state (encoder->pipeline, GST_STATE_PLAYING);

        g_mutex_unlock (encoder->channel->operate_mutex);

        return 0;
}

gint
channel_encoder_restart (Encoder *encoder)
{
        g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc)channel_encoder_restart_func, (gpointer) encoder, NULL);
        return 0;
}

Channel *
channel_get_channel (gchar *uri, GArray *channels)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c;
        Channel *channel;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < channels->len) {
                        channel = g_array_index (channels, gpointer, atoi (c));
                } 
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return channel;
}

Encoder *
channel_get_encoder (gchar *uri, GArray *channels)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        Encoder *encoder = NULL;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < channels->len) {
                        channel = g_array_index (channels, gpointer, atoi (c));
                        e = g_match_info_fetch_named (match_info, "encoder");
                        if (atoi (e) < channel->encoder_array->len) {
                                GST_DEBUG ("http get request, channel is %s, encoder is %s", c, e);
                                encoder = g_array_index (channel->encoder_array, gpointer, atoi (e));
                        } 
                        g_free (e);
                } 
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return encoder;
}

