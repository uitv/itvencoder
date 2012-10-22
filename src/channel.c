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
static GstFlowReturn decoder_appsink_callback_func (GstAppSink * elt, gpointer user_data);
static GstFlowReturn encoder_appsink_callback_func (GstAppSink * elt, gpointer user_data);
static void encoder_appsrc_need_data_callback_func (GstAppSrc *src, guint length, gpointer user_data);
static void encoder_appsrc_enough_data_callback_func (GstAppSrc *src, gpointer user_data);

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

        channel->decoder_pipeline = g_malloc (sizeof (DecoderPipeline)); //TODO free!
        channel->decoder_pipeline->current_audio_position = -1;
        for (gint i=0; i<AUDIO_RING_SIZE; i++)
                channel->decoder_pipeline->audio_ring[i] = NULL;
        channel->decoder_pipeline->current_video_position = -1;
        for (gint i=0; i<VIDEO_RING_SIZE; i++)
                channel->decoder_pipeline->video_ring[i] = NULL;

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

static gboolean
bus_callback (GstBus *bus, GstMessage *msg, gpointer data)
{
        gchar *debug;
        GError *error;
        GstState old, new, pending;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
                GST_LOG ("End of stream\n");
                break;
        case GST_MESSAGE_ERROR: 
                gst_message_parse_error (msg, &error, &debug);
                g_free (debug);
                GST_ERROR ("%s error: %s", (gchar *)data, error->message);
                g_error_free (error);
                break;
        case GST_MESSAGE_STATE_CHANGED:
                gst_message_parse_state_changed (msg, &old, &new, &pending);
                GST_DEBUG ("%s state from %d to %d", (gchar *)data, old, new);
                break;
        default:
                GST_DEBUG ("%s message: %s", (gchar *)data, GST_MESSAGE_TYPE_NAME (msg));
                break;
        }

        return TRUE;
}

typedef struct _UserData {
        gchar type;
        Channel *channel;
} UserData;

static GstFlowReturn decoder_appsink_callback_func (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        gchar type = ((UserData *)user_data)->type;
        Channel *channel = ((UserData *)user_data)->channel;
        guint i;

        GST_LOG ("decoder appsink callback func %c", type);

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        switch (type) {
        case 'a':
                i = channel->decoder_pipeline->current_audio_position + 1;
                i = i % AUDIO_RING_SIZE;
                GST_LOG ("audio current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                channel->decoder_pipeline->current_audio_position = i;
                if (channel->decoder_pipeline->audio_ring[i] != NULL)
                        gst_buffer_unref (channel->decoder_pipeline->audio_ring[i]);
                channel->decoder_pipeline->audio_ring[i] = buffer;
                break;
        case 'v':
                i = channel->decoder_pipeline->current_video_position + 1;
                i = i % VIDEO_RING_SIZE;
                GST_LOG ("video current position %d, buffer duration: %d", i, GST_BUFFER_DURATION(buffer));
                channel->decoder_pipeline->current_video_position = i;
                if (channel->decoder_pipeline->video_ring[i] != NULL)
                        gst_buffer_unref (channel->decoder_pipeline->video_ring[i]);
                channel->decoder_pipeline->video_ring[i] = buffer;
                break;
        default:
                GST_ERROR ("error");
        }
}

guint
channel_set_decoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GError *e = NULL;
        GstElement *appsink, *p;
        GstBus *bus;
        GstAppSinkCallbacks appsink_callbacks = {
                NULL,
                NULL,
                decoder_appsink_callback_func,
                NULL
        };
        UserData *user_data;

        GST_LOG ("channel set decoder pipeline : %s", pipeline_string);

        p = gst_parse_launch (pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, "decoderpipeline");

        channel->decoder_pipeline->pipeline = p;
        channel->decoder_pipeline->pipeline_string = pipeline_string;

        /* set video sink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "videosink"); //TODO release
        if (appsink == NULL) {
                GST_ERROR ("Get video sink error");
                return -1;
        }
        user_data  = (UserData *)g_malloc (sizeof (UserData));
        user_data->type = 'v';
        user_data->channel = channel;
        gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, user_data, NULL);
        gst_object_unref (appsink);

        /* set audio sink callback */
        appsink = gst_bin_get_by_name (GST_BIN (p), "audiosink");
        if (appsink == NULL) {
                GST_ERROR ("Get audio sink error");
                return -1;
        }
        user_data  = (UserData *)g_malloc (sizeof (UserData));
        user_data->type = 'a';
        user_data->channel = channel;
        gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, user_data, NULL);
        gst_object_unref (appsink);

        return 0;
}

static
GstFlowReturn encoder_appsink_callback_func (GstAppSink * elt, gpointer user_data)
{
        GstBuffer *buffer;
        EncoderPipeline *encoder_pipeline = (EncoderPipeline *)user_data;
        GSList *socket_list;
        gint socket;

        GST_LOG ("encoder appsink callback func");

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));
        socket_list = encoder_pipeline->httprequest_socket_list;
        while (socket_list != NULL) {
                socket = GPOINTER_TO_INT(socket_list->data);
                write (socket, "bc\r\n", 4);
                write (socket, GST_BUFFER_DATA(buffer), 188);
                write (socket, "\r\n", 2);
                socket_list = g_slist_next (socket_list);
        } 
        gst_buffer_unref (buffer);
}

typedef struct _EncoderAppsrcUserData {
        gint index;
        gchar type;
        Channel *channel;
} EncoderAppsrcUserData;

static void
encoder_appsrc_need_data_callback_func (GstAppSrc *src, guint length, gpointer user_data)
{
        gint index = ((EncoderAppsrcUserData *)user_data)->index;
        gchar type = ((EncoderAppsrcUserData *)user_data)->type;
        Channel *channel = ((EncoderAppsrcUserData *)user_data)->channel;
        EncoderPipeline *encoder_pipeline;
        gint i;

        GST_LOG ("encoder %d appsrc need data callback func type %c; length %d", index, type, length);

        encoder_pipeline = (EncoderPipeline *)g_array_index (channel->encoder_pipeline_array, gpointer, index);
        switch (type) {
        case 'a':
                encoder_pipeline->audio_enough = FALSE;
                /* next buffer */
                i = encoder_pipeline->current_audio_position + 1;
                i = i % AUDIO_RING_SIZE;
                for (;;) {
                        /* insure next buffer isn't current decoder buffer */
                        if (i == channel->decoder_pipeline->current_audio_position ||
                                channel->decoder_pipeline->current_audio_position == -1) {
                                GST_LOG ("waiting audio decoder ready");
                                g_usleep (50000); /* wiating 50ms */
                                continue;
                        }
                        if (encoder_pipeline->audio_enough) {
                                GST_LOG ("audio enough.");
                                break;
                        }
                        GST_LOG (
                                "audio encoder position %d; decoder position %d",
                                i,
                                channel->decoder_pipeline->current_audio_position
                        );
                        if (gst_app_src_push_buffer (src, gst_buffer_ref(channel->decoder_pipeline->audio_ring[i])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer audio failure.");
                                break;
                        }
                        encoder_pipeline->current_audio_position = i;
                        i = (i + 1) % AUDIO_RING_SIZE;
                }
                break;
        case 'v':
                encoder_pipeline->video_enough = FALSE;
                /* next buffer */
                i = encoder_pipeline->current_video_position + 1;
                i = i % VIDEO_RING_SIZE;
                for (;;) {
                        /* insure next buffer isn't current decoder buffer */
                        if (i == channel->decoder_pipeline->current_video_position ||
                                channel->decoder_pipeline->current_video_position == -1) {
                                GST_DEBUG ("waiting video decoder ready");
                                g_usleep (50000); /* waiting 50ms */
                                continue;
                        }
                        if (encoder_pipeline->video_enough) {
                                GST_DEBUG ("video enough, break for need data signal.");
                                break;
                        }
                        GST_DEBUG (
                                "video encoder position %d; decoder position %d",
                                i,
                                channel->decoder_pipeline->current_video_position
                        );
                        if (gst_app_src_push_buffer (src, gst_buffer_ref(channel->decoder_pipeline->video_ring[i])) != GST_FLOW_OK) {
                                GST_ERROR ("gst_app_src_push_buffer video failure.");
                                break;
                        }
                        encoder_pipeline->current_video_position = i;
                        i = (i + 1) % VIDEO_RING_SIZE;
                }
                break;
        default:
                GST_ERROR ("error");
        }
}

static void
encoder_appsrc_enough_data_callback_func (GstAppSrc *src, gpointer user_data)
{
        gint index = ((EncoderAppsrcUserData *)user_data)->index;
        gchar type = ((EncoderAppsrcUserData *)user_data)->type;
        Channel *channel = ((EncoderAppsrcUserData *)user_data)->channel;
        EncoderPipeline *encoder_pipeline;

        GST_LOG ("encoder appsrc enough data callback func type %c", type);

        encoder_pipeline = (EncoderPipeline *)g_array_index (channel->encoder_pipeline_array, gpointer, index);
        switch (type) {
        case 'a':
                encoder_pipeline->audio_enough = TRUE;
                break;
        case 'v':
                encoder_pipeline->video_enough = TRUE;
                break;
        default:
                GST_ERROR ("error");
        }
}

guint
channel_add_encoder_pipeline (Channel *channel, gchar *pipeline_string)
{
        GstElement *p, *appsrc, *appsink;
        GError *e = NULL;
        GstBus *bus;
        EncoderPipeline *encoder_pipeline;
        GstAppSrcCallbacks callbacks = {
                encoder_appsrc_need_data_callback_func,
                encoder_appsrc_enough_data_callback_func,
                NULL
        };
        GstAppSinkCallbacks encoder_appsink_callbacks = {
                NULL,
                NULL,
                encoder_appsink_callback_func,
                NULL
        };
        EncoderAppsrcUserData *user_data;

        GST_LOG ("channel add encoder pipeline : %s", pipeline_string);

        p = gst_parse_launch (pipeline_string, &e);
        if (e != NULL) {
                GST_ERROR ("Error parsing pipeline %s: %s", pipeline_string, e->message);
                g_error_free (e);
                return -1;
        }

        bus = gst_pipeline_get_bus (GST_PIPELINE (p));
        gst_bus_add_watch (bus, bus_callback, "encoderpipeline");

        appsink = gst_bin_get_by_name (GST_BIN (p), "encodersink");
        if (appsink == NULL) {
                GST_ERROR ("Channel %s, Intialize %s - Get encoder sink error", channel->name, pipeline_string);
                g_free (encoder_pipeline);
                return -1;
        }
        encoder_pipeline = g_malloc (sizeof (EncoderPipeline)); //TODO free!
        if (encoder_pipeline == NULL) {
                GST_ERROR ("g_malloc memeory error.");
                return -1;
        }
        gst_app_sink_set_callbacks (GST_APP_SINK(appsink), &encoder_appsink_callbacks, encoder_pipeline, NULL);
        gst_object_unref (appsink);

        appsrc = gst_bin_get_by_name (GST_BIN (p), "videosrc");
        if (appsrc == NULL) {
                GST_ERROR ("Get video src error");
                g_free (encoder_pipeline);
                return -1;
        }
        user_data  = (EncoderAppsrcUserData *)g_malloc (sizeof (EncoderAppsrcUserData));
        user_data->index = channel->encoder_pipeline_array->len;
        user_data->type = 'v';
        user_data->channel = channel;
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, user_data, NULL);
        gst_object_unref (appsrc);

        appsrc = gst_bin_get_by_name (GST_BIN (p), "audiosrc");
        if (appsrc == NULL) {
                GST_ERROR ("Get audio src error");
                g_free (encoder_pipeline);
                return -1;
        }
        user_data  = (EncoderAppsrcUserData *)g_malloc (sizeof (EncoderAppsrcUserData));
        user_data->index = channel->encoder_pipeline_array->len;
        user_data->type = 'a';
        user_data->channel = channel;
        gst_app_src_set_callbacks (GST_APP_SRC (appsrc), &callbacks, user_data, NULL);
        gst_object_unref (appsrc);

        encoder_pipeline->pipeline_string = pipeline_string;
        encoder_pipeline->pipeline = p;
        encoder_pipeline->current_video_position = -1;
        encoder_pipeline->current_audio_position = -1;
        encoder_pipeline->audio_enough = FALSE;
        encoder_pipeline->video_enough = FALSE;
        encoder_pipeline->httprequest_socket_list = NULL;
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
