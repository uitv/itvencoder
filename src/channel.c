/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include "jansson.h"
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
static GstFlowReturn decoder_appsink_callback_func (GstAppSink * elt, gpointer user_data);
static GstFlowReturn encoder_appsink_callback_func (GstAppSink * elt, gpointer user_data);
static void encoder_appsrc_need_data_callback_func (GstAppSrc *src, guint length, gpointer user_data);
static void encoder_appsrc_enough_data_callback_func (GstAppSrc *src, gpointer user_data);

static void
channel_class_init (ChannelClass *channelclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(channelclass);
        GParamSpec *config_param;

        GST_LOG ("channel class init.");

        g_object_class->constructor = channel_constructor;
        g_object_class->set_property = channel_set_property;
        g_object_class->get_property = channel_get_property;

        config_param = g_param_spec_string ("name",
                                            "name",
                                            "name of channel",
                                            "",
                                            G_PARAM_WRITABLE | G_PARAM_READABLE);
        g_object_class_install_property (g_object_class, CHANNEL_PROP_NAME, config_param);
}

static void
channel_init (Channel *channel)
{
        GST_LOG ("channel object init");

        channel->decoder_pipeline = g_malloc (sizeof (DecoderPipeline)); //TODO free!
        channel->encoder_pipeline_array = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
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

static GstFlowReturn decoder_appsink_callback_func (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;

        GST_LOG ("appsink callback func");

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        GST_LOG ("\nbuffer duration: %d", GST_BUFFER_DURATION(buffer));
        gst_buffer_unref (buffer);
}

guint
channel_set_decoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GError *e = NULL;
        GstElement *appsink, *p;
        GstAppSinkCallbacks appsink_callbacks = {NULL,
                                                 NULL,
                                                 decoder_appsink_callback_func,
                                                 NULL};

        GST_LOG ("channel set decoder pipeline : %s", pipeline_string);

        p = gst_parse_launch (pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        channel->decoder_pipeline->pipeline = p;
        channel->decoder_pipeline->pipeline_string = pipeline_string;

        appsink = gst_bin_get_by_name (GST_BIN (p), "videosink");
        if (appsink == NULL) {
                GST_ERROR ("Get video sink error");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &appsink_callbacks, NULL, NULL);
        gst_object_unref (appsink);

        appsink = gst_bin_get_by_name (GST_BIN (p), "audiosink");
        if (appsink == NULL) {
                GST_ERROR ("Get audio sink error");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &appsink_callbacks, NULL, NULL);
        gst_object_unref (appsink);

        return 0;
}

static
GstFlowReturn encoder_appsink_callback_func (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;

        GST_LOG ("appsink callback func");

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        GST_LOG ("\nbuffer duration: %d", GST_BUFFER_DURATION(buffer));
        gst_buffer_unref (buffer);
}

static void
encoder_appsrc_need_data_callback_func (GstAppSrc *src, guint length, gpointer user_data)
{
        GST_LOG ("encoder appsrc need data callback func");
}

static void
encoder_appsrc_enough_data_callback_func (GstAppSrc *src, gpointer user_data)
{
        GST_LOG ("encoder appsrc enough data callback func");
}

guint
channel_add_encoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GstElement *p, *appsrc, *appsink;
        GError *e = NULL;
        EncoderPipeline *encoder_pipeline;
        GstAppSrcCallbacks callbacks = {encoder_appsrc_need_data_callback_func,
                                        encoder_appsrc_enough_data_callback_func,
                                        NULL};
        GstAppSinkCallbacks encoder_appsink_callbacks = {NULL,
                                                         NULL,
                                                         encoder_appsink_callback_func,
                                                         NULL};

        GST_LOG ("channel add encoder pipeline : %s", pipeline_string);

        p = gst_parse_launch (pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        appsink = gst_bin_get_by_name (GST_BIN (p), "encodersink");
        if (appsink == NULL) {
                GST_ERROR ("Channel %s, Intialize %s - Get encoder sink error", channel->name, pipeline_string);
                g_free (encoder_pipeline);
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &encoder_appsink_callbacks, NULL, NULL);
        gst_object_unref (appsink);

        appsrc = gst_bin_get_by_name (GST_BIN (p), "videosrc");
        if (appsrc == NULL) {
                GST_ERROR ("Get video src error");
                g_free (encoder_pipeline);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, NULL, NULL);
        gst_object_unref (appsrc);

        appsrc = gst_bin_get_by_name (GST_BIN (p), "audiosrc");
        if (appsrc == NULL) {
                GST_ERROR ("Get audio src error");
                g_free (encoder_pipeline);
                return -1;
        }
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, NULL, NULL);
        gst_object_unref (appsrc);

        encoder_pipeline = g_malloc (sizeof (EncoderPipeline)); //TODO free!
        if (encoder_pipeline == NULL) {
                GST_ERROR ("g_malloc memeory error.");
                return -1;
        }
        encoder_pipeline->pipeline_string = pipeline_string;
        encoder_pipeline->pipeline = p;
        g_array_append_val (channel->encoder_pipeline_array, encoder_pipeline);

        return 0;
}

gint
channel_set_decoder_pipeline_state (Channel *channel, GstState state)
{
        GST_LOG ("set decoder pipeline state");

        gst_element_set_state (channel->decoder_pipeline->pipeline, state);

        return 0;
}

gint
channel_set_encoder_pipeline_state (Channel *channel, gint index, GstState state)
{
        EncoderPipeline *e;

        GST_LOG ("channel set encoder pipeline state");

        if (0 > index || index > channel->encoder_pipeline_array->len ) {
                GST_ERROR ("index exceed the count of encoder number. %d", index);
                return -1;
        }
        e = g_array_index (channel->encoder_pipeline_array, gpointer, index);
        gst_element_set_state (e->pipeline, state);

        return 0;
}
