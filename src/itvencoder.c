/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include <string.h>
#include "itvencoder.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

static void itvencoder_class_init (ITVEncoderClass *itvencoderclass);
static void itvencoder_init (ITVEncoder *itvencoder);
static GTimeVal itvencoder_get_start_time_func (ITVEncoder *itvencoder);
static gboolean itvencoder_channel_monitor (GstClock *clock, GstClockTime time, GstClockID id, gpointer user_data);
static GstClockTime request_dispatcher (gpointer data, gpointer user_data);
static EncoderPipeline * get_encoder (gchar *uri, ITVEncoder *itvencoder);

static void
itvencoder_class_init (ITVEncoderClass *itvencoderclass)
{
}

static void
itvencoder_init (ITVEncoder *itvencoder)
{
        ChannelConfig *channel_config;
        Channel *channel;
        gchar *pipeline_string;
        GstClockID id;
        GstClockTime t;
        GstClockReturn ret;
        guint i;

        GST_LOG ("itvencoder_init");

        itvencoder->system_clock = gst_system_clock_obtain ();
        g_object_set (itvencoder->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        itvencoder->start_time = gst_clock_get_time (itvencoder->system_clock);
        t = gst_clock_get_time (itvencoder->system_clock)  + 5000 * GST_MSECOND;
        id = gst_clock_new_single_shot_id (itvencoder->system_clock, t); // FIXME: id should be released
        ret = gst_clock_id_wait_async (id, itvencoder_channel_monitor, itvencoder);
        if (ret != GST_CLOCK_OK) {
                GST_WARNING ("Register itvencoder monitor failure");
                exit (-1);
        }

        // load config
        itvencoder->config = config_new ("config_file_path", "itvencoder.conf", NULL);
        config_load_config_file(itvencoder->config);

        // initialize channels
        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                channel = channel_new ("name", channel_config->name, NULL);
                pipeline_string = config_get_pipeline_string (channel_config, "decoder-pipeline");
                if (pipeline_string == NULL) {
                        GST_ERROR ("no decoder pipeline error");
                        exit (-1); //TODO : exit or return?
                }
                if (channel_set_decoder_pipeline (channel, pipeline_string) != 0) {
                        GST_ERROR ("Set decoder pipeline error.");
                        exit (-1);
                }
                for (;;) {
                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-1");
                        if (pipeline_string == NULL) {
                                GST_ERROR ("One encoder pipeline is must");
                                exit (-1); //TODO : exit or return?
                        }
                        if (channel_add_encoder_pipeline (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 1 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-2");
                        if (pipeline_string == NULL) {
                                GST_INFO ("One encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder_pipeline (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 2 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-3");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Two encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder_pipeline (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 3 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-4");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Three encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder_pipeline (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 4 error");
                                exit (-1);
                        }

                        GST_INFO ("Four encoder pipelines found.");
                        break;
                }
                GST_INFO ("parse channel %s, name is %s, encoder channel number %d.",
                           channel_config->config_path,
                           channel_config->name,
                           channel->encoder_pipeline_array->len);
                g_array_append_val (itvencoder->channel_array, channel);
        }
        itvencoder->httpserver = httpserver_new ("port", 20129, NULL);
}

GType
itvencoder_get_type (void)
{
        static GType type = 0;

        GST_LOG ("itvencoder get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ITVEncoderClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc) itvencoder_class_init,
                NULL,
                NULL,
                sizeof (ITVEncoder),
                0,
                (GInstanceInitFunc) itvencoder_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "ITVEncoder", &info, 0);

        return type;
}

static gboolean
itvencoder_channel_monitor (GstClock *clock, GstClockTime time, GstClockID id, gpointer user_data)
{
        GstClockID nextid;
        GstClockTime t;
        GstClockReturn ret;
        ITVEncoder *itvencoder = (ITVEncoder *)user_data;
        Channel *channel;
        gint i, j;

        GST_INFO ("itvencoder channel monitor");

        for (i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                GST_INFO ("%s decoder pipeline video last timestamp %llu", channel->name, channel->decoder_pipeline->current_video_timestamp);
                GST_INFO ("%s decoder pipeline audio last timestamp %llu", channel->name, channel->decoder_pipeline->current_audio_timestamp);
                GST_INFO ("%s decoder pipeline video last heart beat %llu", channel->name, channel->decoder_pipeline->last_video_heartbeat);
                GST_INFO ("%s decoder pipeline audio last heart beat %llu", channel->name, channel->decoder_pipeline->last_audio_heartbeat);
                for (j=0; j<channel->encoder_pipeline_array->len; j++) {
                        EncoderPipeline *encoder_pipeline = g_array_index (channel->encoder_pipeline_array, gpointer, j);
                        GST_INFO ("%s encoder pipeline video last heart beat %llu", channel->name, encoder_pipeline->last_video_heartbeat);
                        GST_INFO ("%s encoder pipeline audio last heart beat %llu", channel->name, encoder_pipeline->last_audio_heartbeat);
                }
        }

        t = gst_clock_get_time (itvencoder->system_clock)  + 2000 * GST_MSECOND;
        nextid = gst_clock_new_single_shot_id (itvencoder->system_clock, t); // FIXME: id should be released
        ret = gst_clock_id_wait_async (nextid, itvencoder_channel_monitor, itvencoder);
        if (ret != GST_CLOCK_OK) {
                GST_WARNING ("Register itvencoder monitor failure");
                return FALSE;
        }

        return TRUE;
}

GstClockTime
itvencoder_get_start_time (ITVEncoder *itvencoder)
{
        GST_LOG ("itvencoder get start time");

        return itvencoder->start_time;
}

gint
itvencoder_start (ITVEncoder *itvencoder)
{
        int i, j;
        Channel *channel;

        for (i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                GST_INFO ("\nchannel %s has %d encoder pipeline.>>>>>>>>>>>>>>>>>>>>>>>>>\nchannel decoder pipeline string is %s",
                        channel->name,
                        channel->encoder_pipeline_array->len,
                        channel->decoder_pipeline->pipeline_string);
                channel_set_decoder_pipeline_state (channel, GST_STATE_PLAYING);
                for (j=0; j<channel->encoder_pipeline_array->len; j++) {
                        EncoderPipeline *encoder_pipeline = g_array_index (channel->encoder_pipeline_array, gpointer, j);
                        GST_INFO ("\nchannel encoder pipeline string is %s", encoder_pipeline->pipeline_string);
                        channel_set_encoder_pipeline_state (channel, j, GST_STATE_PLAYING);
                }
        }
        httpserver_start (itvencoder->httpserver, request_dispatcher, itvencoder);

        return 0;
}

static EncoderPipeline *
get_encoder (gchar *uri, ITVEncoder *itvencoder)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        EncoderPipeline *encoder = NULL;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < itvencoder->channel_array->len) {
                        channel = g_array_index (itvencoder->channel_array, gpointer, atoi (c));
                        e = g_match_info_fetch_named (match_info, "encoder");
                        if (atoi (e) < channel->encoder_pipeline_array->len) {
                                GST_DEBUG ("http get request, channel is %s, encoder is %s", c, e);
                                encoder = g_array_index (channel->encoder_pipeline_array, gpointer, atoi (e));
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

typedef struct _RequestDataUserData {
        gint current_send_position;
        gpointer encoder;
} RequestDataUserData;

/**
 * request_dispatcher:
 * @data: RequestData type pointer
 * @user_data: itvencoder type pointer
 *
 * Process http request.
 *
 * Returns: positive value if have not completed the processing, for example live streaming.
 *      0 if have completed the processing.
 */
static GstClockTime
request_dispatcher (gpointer data, gpointer user_data)
{
        RequestData *request_data = data;
        ITVEncoder *itvencoder = user_data;
        gchar *buf;
        int i = 0, j;
        EncoderPipeline *encoder;
        GstBuffer *buffer;
        RequestDataUserData *request_user_data;
        gchar *size = "524\r\n", *end = "\r\n";
        struct iovec iov[9];

        GST_LOG ("hello");

        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_WARNING ("socket is %d uri is %s", request_data->sock, request_data->uri);
                switch (request_data->uri[1]) {
                case 'c': /* uri is /channel..., maybe request for encoder streaming */
                        encoder = get_encoder (request_data->uri, itvencoder);
                        if (encoder == NULL) {
                                buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }
                        request_user_data = (RequestDataUserData *)g_malloc (sizeof (RequestDataUserData));
                        GST_ERROR ("request_user_data %lld", request_user_data);
                        request_user_data->encoder = encoder;
                        while (encoder->current_output_position <= 7) {
                                g_usleep (50); /*FIXME*/
                        }
                        request_user_data->current_send_position = encoder->current_output_position - 7; /*real time*/
                        request_user_data->current_send_position = (request_user_data->current_send_position / 7) * 7;
                        request_data->user_data = request_user_data;
                        buf = g_strdup_printf (http_chunked, ENCODER_NAME, ENCODER_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return gst_clock_get_time (itvencoder->system_clock)  + GST_MSECOND; // 50ms
                default:
                        buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                }
        case HTTP_CONTINUE:
                //GST_ERROR ("http continue, socket is %d uri is %s", request_data->sock, request_data->uri);
                request_user_data = request_data->user_data;
                encoder = request_user_data->encoder;
                i = request_user_data->current_send_position;
                i %= OUTPUT_RING_SIZE;
                //GST_ERROR ("i--> %d current output position %d", i, encoder->current_output_position);
                for (;;) {
                        if ((i / 7) == ((encoder->current_output_position) / 7)) { //catch up, stop write
                                //GST_ERROR ("waiting a while??");
                                break;
                        }
                        iov[0].iov_base = size;
                        iov[0].iov_len = 5;
                        iov[1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i]);
                        iov[1].iov_len = 188;
                        iov[2].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+1]);
                        iov[2].iov_len = 188;
                        iov[3].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+2]);
                        iov[3].iov_len = 188;
                        iov[4].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+3]);
                        iov[4].iov_len = 188;
                        iov[5].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+4]);
                        iov[5].iov_len = 188;
                        iov[6].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+5]);
                        iov[6].iov_len = 188;
                        iov[7].iov_base = GST_BUFFER_DATA (encoder->output_ring[i+6]);
                        iov[7].iov_len = 188;
                        iov[8].iov_base = end;
                        iov[8].iov_len = 2;
                        //GST_ERROR ("write data, sock %d", request_data->sock);
                        writev (request_data->sock, iov, 9);
                        i = (i + 7) % OUTPUT_RING_SIZE;
                        request_user_data->current_send_position = i;
                }
                return gst_clock_get_time (itvencoder->system_clock)  + 50 * GST_MSECOND; // 50ms;
        case HTTP_FINISH:
                GST_ERROR ("finish status user_data %lld----------------------", request_data->user_data);
                g_free (request_data->user_data);
                request_data->user_data = NULL;
                return 0;
        default:
                GST_ERROR ("Unknown status %d", request_data->status);
                buf = g_strdup_printf (http_400, ENCODER_NAME, ENCODER_VERSION);
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
                return 0;
        }
}

