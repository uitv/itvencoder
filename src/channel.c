/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

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
static GstFlowReturn appsink_callback_func (GstAppSink * elt, gpointer user_data);

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

static GstFlowReturn appsink_callback_func (GstAppSink * elt, gpointer user_data)
{
        GST_LOG ("appsink callback func");
}

guint
channel_set_decoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GError *e = NULL;
        GstElement *appsink, *p;
        GstAppSinkCallbacks videosink_callbacks = {NULL, NULL, appsink_callback_func, NULL};
        GstAppSinkCallbacks audiosink_callbacks = {NULL, NULL, appsink_callback_func, NULL};

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
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &videosink_callbacks, NULL, NULL);
        gst_object_unref (appsink);

        appsink = gst_bin_get_by_name (GST_BIN (p), "audiosink");
        if (appsink == NULL) {
                GST_ERROR ("Get audio sink error");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &audiosink_callbacks, NULL, NULL);
        gst_object_unref (appsink);

        return 0;
}

guint
channel_add_encoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GstElement *p;
        GError *e = NULL;
        EncoderPipeline *encoder_pipeline;

        GST_LOG ("channel add encoder pipeline : %s", pipeline_string);

        p = gst_parse_launch (pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        encoder_pipeline = g_malloc (sizeof (EncoderPipeline)); //TODO free!
        if (encoder_pipeline == NULL) {
                GST_ERROR ("g_malloc memeory error.");
                return -1;
        }

        encoder_pipeline->pipeline_string = pipeline_string;
        encoder_pipeline->encodersink = gst_bin_get_by_name (GST_BIN (p), "encodersink");
        if (encoder_pipeline->encodersink == NULL) {
                GST_ERROR ("Channel %s, Intialize %s - Get encoder sink error", channel->name, pipeline_string);
                g_free (encoder_pipeline);
                return -1;
        }

        encoder_pipeline->videosrc = gst_bin_get_by_name (GST_BIN (p), "videosrc");
        if (encoder_pipeline->videosrc == NULL) {
                GST_ERROR ("Get video src error");
                g_free (encoder_pipeline);
                return -1;
        }

        encoder_pipeline->audiosrc = gst_bin_get_by_name (GST_BIN (p), "audiosrc");
        if (encoder_pipeline->audiosrc == NULL) {
                GST_ERROR ("Get audio src error");
                g_free (encoder_pipeline);
                return -1;
        }

        encoder_pipeline->pipeline = p;
        g_array_append_val (channel->encoder_pipeline_array, encoder_pipeline);

        return 0;
}

gint channel_set_decoder_pipeline_state (Channel *channel, GstState state)
{
        GST_LOG ("set decoder pipeline state");

        gst_element_set_state (channel->decoder_pipeline->pipeline, state);

        return 0;
}
