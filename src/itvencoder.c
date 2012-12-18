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
static Encoder * get_encoder (gchar *uri, ITVEncoder *itvencoder);
static Channel * get_channel (gchar *uri, ITVEncoder *itvencoder);
static GstClockTime request_dispatcher (gpointer data, gpointer user_data);

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
        gchar *name;

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

        itvencoder->grand = g_rand_new ();

        // load config
        itvencoder->config = config_new ("config_file_path", "/etc/itvencoder/itvencoder.conf", NULL);
        if (config_load_config_file(itvencoder->config) != 0) {
                GST_ERROR ("Channel config files load error");
                exit (0);
        }

        // initialize channels
        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                name = g_strdup_printf ("channel-%d", i);
                channel = channel_new ("name", name, NULL);
                channel->id = i;
                g_free (name);
                pipeline_string = config_get_pipeline_string (channel_config, "decoder-pipeline");
                if (pipeline_string == NULL) {
                        GST_ERROR ("no source pipeline string error");
                        exit (-1); //TODO : exit or return?
                }
                if (channel_set_source (channel, pipeline_string) != 0) {
                        GST_ERROR ("Set source pipeline error.");
                        exit (-1);
                }
                for (;;) {
                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-1");
                        if (pipeline_string == NULL) {
                                GST_ERROR ("One encoder pipeline is must");
                                exit (-1); //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 1 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-2");
                        if (pipeline_string == NULL) {
                                GST_INFO ("One encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 2 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-3");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Two encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 3 error");
                                exit (-1);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-4");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Three encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 4 error");
                                exit (-1);
                        }

                        GST_INFO ("Four encoder pipelines found.");
                        break;
                }
                GST_INFO ("parse channel %s, encoder channel number %d.",
                           channel_config->config_path,
                           channel->encoder_array->len);
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
        GstClockTime now;
        GstClockTimeDiff time_diff;
        GstClockReturn ret;
        ITVEncoder *itvencoder = (ITVEncoder *)user_data;
        Channel *channel;
        gint i, j;

        GST_INFO ("itvencoder channel monitor");

        for (i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                if (channel->source->state != GST_STATE_PLAYING) {
                        continue;
                }
                time_diff = GST_CLOCK_DIFF (channel->source->current_video_timestamp, channel->source->current_audio_timestamp);
                if ((time_diff > SYNC_THRESHHOLD) || (time_diff < - SYNC_THRESHHOLD)) {
                        GST_ERROR ("channel %s audio and video sync error %lld", channel->name, time_diff);
                        channel->source->sync_error_times += 1;
                        if (channel->source->sync_error_times == 3) {
                                GST_ERROR ("sync error times %d, restart channel %s", channel->source->sync_error_times, channel->name);
                                channel_restart (channel);
                                channel->source->sync_error_times = 0;
                        }
                } else {
                        channel->source->sync_error_times = 0;
                        GST_INFO ("%s source video timestamp %" GST_TIME_FORMAT,
                                  channel->name,
                                  GST_TIME_ARGS (channel->source->current_video_timestamp));
                        GST_INFO ("%s source audio timestamp %" GST_TIME_FORMAT,
                                  channel->name,
                                  GST_TIME_ARGS (channel->source->current_audio_timestamp));
                }

                now = gst_clock_get_time (itvencoder->system_clock);
                time_diff = GST_CLOCK_DIFF (now, channel->source->last_video_heartbeat);
                if ((time_diff > HEARTBEAT_THRESHHOLD) || (time_diff < - HEARTBEAT_THRESHHOLD)) {
                        GST_ERROR ("video source heart beat error %lld, restart channel %s", time_diff, channel->name);
                        channel_restart (channel); 
                } else {
                        GST_INFO ("%s source video heart beat %" GST_TIME_FORMAT,
                                  channel->name,
                                  GST_TIME_ARGS (channel->source->last_video_heartbeat));
                }

                now = gst_clock_get_time (itvencoder->system_clock);
                time_diff = GST_CLOCK_DIFF (now, channel->source->last_audio_heartbeat);
                if ((time_diff > HEARTBEAT_THRESHHOLD) || (time_diff < - HEARTBEAT_THRESHHOLD)) {
                        GST_ERROR ("audio source heart beat error %lld, restart channel %s", time_diff, channel->name);
                        channel_restart (channel);
                } else {
                        GST_INFO ("%s source audio heart beat %" GST_TIME_FORMAT,
                                  channel->name,
                                  GST_TIME_ARGS (channel->source->last_audio_heartbeat));
                }

                for (j=0; j<channel->encoder_array->len; j++) {
                        Encoder *encoder = g_array_index (channel->encoder_array, gpointer, j);
                        if (encoder->state != GST_STATE_PLAYING) {
                                continue;
                        }
                        now = gst_clock_get_time (itvencoder->system_clock);
                        time_diff = GST_CLOCK_DIFF (now, encoder->last_video_heartbeat);
                        if ((time_diff > HEARTBEAT_THRESHHOLD) || (time_diff < - HEARTBEAT_THRESHHOLD)) {
                                GST_ERROR ("endcoder video heart beat error %lld, restart endcoder %s", time_diff, encoder->name);
                                channel_encoder_restart (encoder);
                                continue;
                        } else {
                                GST_INFO ("%s video heart beat %" GST_TIME_FORMAT,
                                          encoder->name,
                                          GST_TIME_ARGS (encoder->last_video_heartbeat));
                        }

                        now = gst_clock_get_time (itvencoder->system_clock);
                        time_diff = GST_CLOCK_DIFF (now, encoder->last_audio_heartbeat);
                        if ((time_diff > HEARTBEAT_THRESHHOLD) || (time_diff < - HEARTBEAT_THRESHHOLD)) {
                                GST_ERROR ("endcoder audio heart beat error %lld, restart endcoder %s", time_diff, encoder->name);
                                channel_encoder_restart (encoder);
                        } else {
                                GST_INFO ("%s audio heart beat %" GST_TIME_FORMAT,
                                          encoder->name,
                                          GST_TIME_ARGS (encoder->last_audio_heartbeat));
                        }
                }
        }

        httpserver_report_request_data (itvencoder->httpserver);

        now = gst_clock_get_time (itvencoder->system_clock);
        nextid = gst_clock_new_single_shot_id (itvencoder->system_clock, now + 2000 * GST_MSECOND); // FIXME: id should be released
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
                        channel->encoder_array->len,
                        channel->source->pipeline_string);
                channel_source_start (channel->source);
                for (j=0; j<channel->encoder_array->len; j++) {
                        Encoder *encoder = g_array_index (channel->encoder_array, gpointer, j);
                        GST_INFO ("\nchannel encoder pipeline string is %s", encoder->pipeline_string);
                        channel_encoder_start (encoder);
                }
        }
        httpserver_start (itvencoder->httpserver, request_dispatcher, itvencoder);

        return 0;
}

static Channel *
get_channel (gchar *uri, ITVEncoder *itvencoder)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c;
        Channel *channel;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < itvencoder->channel_array->len) {
                        channel = g_array_index (itvencoder->channel_array, gpointer, atoi (c));
                } 
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return channel;
}

static Encoder *
get_encoder (gchar *uri, ITVEncoder *itvencoder)
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
                if (atoi (c) < itvencoder->channel_array->len) {
                        channel = g_array_index (itvencoder->channel_array, gpointer, atoi (c));
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

typedef struct _RequestDataUserData {
        gint current_send_position;
        gint last_send_count;
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
        int i = 0, j, ret;
        Encoder *encoder;
        Channel *channel;
        GstBuffer *buffer;
        RequestDataUserData *request_user_data;
        gchar *size = "ff90\r\n", *end = "\r\n";
        struct iovec iov[350];

        GST_LOG ("hello");

        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_INFO ("socket is %d uri is %s", request_data->sock, request_data->uri);
                switch (request_data->uri[1]) {
                case 'c': /* uri is /channel..., maybe request for encoder streaming */
                        encoder = get_encoder (request_data->uri, itvencoder);
                        if (encoder == NULL) {
                                channel = get_channel (request_data->uri, itvencoder);
                                if (channel == NULL) {
                                        buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                } else if (request_data->parameters[0] == 's') {
                                        GST_WARNING ("Stop source");
                                        for (i=0; i<channel->encoder_array->len; i++) {
                                                channel_encoder_stop (g_array_index (channel->encoder_array, gpointer, i));
                                        }
                                        if (channel_source_stop (channel->source) == 0) {
                                                buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else if (request_data->parameters[0] == 'p') {
                                        GST_WARNING ("Start source");
                                        if (channel_source_start (channel->source) == 0) {
                                                buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else if (request_data->parameters[0] == 'r') {
                                        GST_WARNING ("Restart source");
                                        if (channel_restart (channel) == 0) {
                                                buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else {
                                        buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                }
                        }
                        if ((request_data->parameters[0] == '\0') || /* default operator is play */
                            (request_data->parameters[0] == 'b')) { /* ?bitrate= */
                                GST_INFO ("Play command");
                                if (encoder->state != GST_STATE_PLAYING) {
                                        buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                }
                                request_user_data = (RequestDataUserData *)g_malloc (sizeof (RequestDataUserData));//FIXME
                                if (request_user_data == NULL) {
                                        GST_ERROR ("Internal Server Error, g_malloc for request_user_data failure.");
                                        buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                }
                                request_user_data->last_send_count = 0;
                                request_user_data->encoder = encoder;
                                request_user_data->current_send_position = encoder->current_output_position - 348; /*real time*/
                                request_user_data->current_send_position = (request_user_data->current_send_position / 348) * 348;
                                request_data->user_data = request_user_data;
                                buf = g_strdup_printf (http_chunked, ENCODER_NAME, ENCODER_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return gst_clock_get_time (itvencoder->system_clock)  + GST_MSECOND; // 50ms
                        } else if (request_data->parameters[0] == 's') {
                                GST_WARNING ("Stop endcoder");
                                if (channel_encoder_stop (encoder) == 0) {
                                        buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                } else {
                                        buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                }
                                return 0;
                        } else if (request_data->parameters[0] == 'p') {
                                GST_WARNING ("Start endcoder");
                                if (channel_encoder_start (encoder) ==0) {
                                        buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                } else {
                                        buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                }
                                return 0;
                        } else if (request_data->parameters[0] == 'r') {
                                GST_WARNING ("Restart endcoder");
                                if (channel_encoder_restart (encoder) ==0) {
                                        buf = g_strdup_printf (http_200, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                } else {
                                        buf = g_strdup_printf (http_500, ENCODER_NAME, ENCODER_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                }
                                return 0;
                        }
                case 'i': /* uri is /itvencoder/..... */
                        buf = g_strdup_printf (itvencoder_ver, ENCODER_NAME, ENCODER_VERSION, sizeof (ENCODER_NAME) + sizeof (ENCODER_VERSION) + 1, ENCODER_NAME, ENCODER_VERSION); 
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0; 
                default:
                        buf = g_strdup_printf (http_404, ENCODER_NAME, ENCODER_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                }
        case HTTP_CONTINUE:
                request_user_data = request_data->user_data;
                encoder = request_user_data->encoder;
                
                if (encoder->state != GST_STATE_PLAYING) {
                        /* encoder pipeline's state is not playing, return 0 */
                        return 0;
                }

                i = request_user_data->current_send_position;
                i %= OUTPUT_RING_SIZE;
                for (;;) {
                        if ((i / 348) == ((encoder->current_output_position) / 348)) { //catch up, stop write
                                break;
                        }
                        if (request_user_data->last_send_count == 0) {
                                iov[0].iov_base = size;
                                iov[0].iov_len = 6;
                                for (j=0; j<348; j++) {
                                        iov[j + 1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i + j]);
                                        iov[j + 1].iov_len = 188;
                                }
                                iov[349].iov_base = end;
                                iov[349].iov_len = 2;
                        } else {
                                if (request_user_data->last_send_count < 6) {
                                        iov[0].iov_base = size + request_user_data->last_send_count;
                                        iov[0].iov_len = 6 - request_user_data->last_send_count;
                                } else {
                                        iov[0].iov_base = NULL;
                                        iov[0].iov_len = 0;
                                }
                                for (j=0; j<348; j++) {
                                        if (request_user_data->last_send_count < (6 + j * 188)) {
                                                /* this buffer has not been send */
                                                iov[j + 1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i + j]);
                                                iov[j + 1].iov_len = 188;
                                        } else if (request_user_data->last_send_count < (6 + (j + 1) * 188)) {
                                                /* this buffer has been send partialy */
                                                iov[j + 1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i + j]) + (request_user_data->last_send_count - 6) % 188;
                                                iov[j + 1].iov_len = 188 - (request_user_data->last_send_count - 6) % 188;
                                        } else {
                                                /* this buffer has been send */
                                                iov[j + 1].iov_base = NULL;
                                                iov[j + 1].iov_len = 0;
                                        }
                                }
                                if (request_user_data->last_send_count <= 65430) {
                                        iov[349].iov_base = end;
                                        iov[349].iov_len = 2;
                                } else {
                                        iov[349].iov_base = end + (request_user_data->last_send_count - 65430);
                                        iov[349].iov_len = 65432 - request_user_data->last_send_count;
                                }
                        }
                        ret = writev (request_data->sock, iov, 350);
                        if (request_user_data->last_send_count + ret < 65432) {
                                request_user_data->last_send_count += ret;
                                return GST_CLOCK_TIME_NONE; // 50ms;
                        } else {
                                request_user_data->last_send_count = 0;
                                i = (i + 348) % OUTPUT_RING_SIZE;
                                request_user_data->current_send_position = i;
                        }
                }
                return gst_clock_get_time (itvencoder->system_clock) + 10 * GST_MSECOND + g_rand_int_range (itvencoder->grand, 1, 1000000);
        case HTTP_FINISH:
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

