/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <gst/gst.h>
#include <string.h>

#include "channel.h"
#include "httpstreaming.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        HTTPSTREAMING_PROP_0,
        HTTPSTREAMING_PROP_ITVENCODER,
};

static void httpstreaming_class_init (HTTPStreamingClass *httpstreamingclass);
static void httpstreaming_init (HTTPStreaming *httpstreaming);
static GObject *httpstreaming_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpstreaming_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpstreaming_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);

static void
httpstreaming_class_init (HTTPStreamingClass *httpstreamingclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (httpstreamingclass);
        GParamSpec *param;

        g_object_class->constructor = httpstreaming_constructor;
        g_object_class->set_property = httpstreaming_set_property;
        g_object_class->get_property = httpstreaming_get_property;

        param = g_param_spec_pointer (
                "itvencoder",
                "itvencoder",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPSTREAMING_PROP_ITVENCODER, param);
}

static void
httpstreaming_init (HTTPStreaming *httpstreaming)
{
}

static GObject *
httpstreaming_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
httpstreaming_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_HTTPSTREAMING(obj));

        switch(prop_id) {
        case HTTPSTREAMING_PROP_ITVENCODER:
                HTTPSTREAMING(obj)->itvencoder = (ITVEncoder *)g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
httpstreaming_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        HTTPStreaming  *httpstreaming = HTTPSTREAMING(obj);

        switch(prop_id) {
        case HTTPSTREAMING_PROP_ITVENCODER:
                g_value_set_pointer (value, httpstreaming->itvencoder);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
httpstreaming_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (HTTPStreamingClass), // class size.
                NULL, // base initializer
                NULL, // base finalizer
                (GClassInitFunc) httpstreaming_class_init, // class init.
                NULL, // class finalize.
                NULL, // class data.
                sizeof (HTTPStreaming),
                0, // instance size.
                (GInstanceInitFunc) httpstreaming_init, // instance init.
                NULL // value table.
        };
        type = g_type_register_static (G_TYPE_OBJECT, "HTTPStreaming", &info, 0);

        return type;
}

static Channel *
get_channel (HTTPStreaming *httpstreaming, RequestData *request_data)
{
        gint channel_index;
        Channel *channel;

        channel_index = itvencoder_url_channel_index (request_data->uri);
        channel = g_array_index (httpstreaming->itvencoder->channel_array, gpointer, channel_index);

        return channel;
}

static Encoder *
get_encoder (gchar *uri, GArray *channels)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        Encoder *encoder = NULL;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+).*", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                if (atoi (c) < channels->len) {
                        channel = g_array_index (channels, gpointer, atoi (c));
                        e = g_match_info_fetch_named (match_info, "encoder");
                        if (atoi (e) < channel->encoder_array->len) {
                                GST_DEBUG ("http get request, channel is %s, encoder is %s", c, e);
                                encoder = g_array_index (channel->encoder_array, gpointer, atoi (e));
                        } else {
                                GST_ERROR ("Out range encoder %s, len is %d.", e, channel->encoder_array->len);
                        }
                        g_free (e);
                } else {
                        GST_ERROR ("Out range channel %s.", c);
                }
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return encoder;
}

static EncoderOutput *
get_encoder_output (HTTPStreaming *httpstreaming, RequestData *request_data)
{
        gint channel_index, encoder_index;
        Channel *channel;

        channel_index = itvencoder_url_channel_index (request_data->uri);
        if (channel_index >= httpstreaming->itvencoder->channel_array->len) {
                GST_ERROR ("channel not fount: %s.", request_data->uri);
                return NULL;
        }
        channel = g_array_index (httpstreaming->itvencoder->channel_array, gpointer, channel_index);
        encoder_index = itvencoder_url_encoder_index (request_data->uri);
        if (encoder_index >= channel->encoder_array->len) {
                GST_ERROR ("encoder not fount: %s.", request_data->uri);
                return NULL;
        }

        return &channel->output->encoders[encoder_index];
}

static gint
send_data (EncoderOutput *encoder_output, RequestData *request_data)
{
        RequestDataUserData *request_user_data;
        struct iovec iov[3];
        gint ret;

        request_user_data = request_data->user_data;
        iov[0].iov_base = NULL;
        iov[0].iov_len = 0;
        iov[1].iov_base = NULL;
        iov[1].iov_len = 0;
        iov[2].iov_base = "\r\n";
        iov[2].iov_len = 2;
        if (request_user_data->send_count < request_user_data->chunk_size_str_len) {
                iov[0].iov_base = request_user_data->chunk_size_str + request_user_data->send_count;
                iov[0].iov_len = request_user_data->chunk_size_str_len - request_user_data->send_count;
                iov[1].iov_base = encoder_output->cache_addr + request_user_data->current_send_position;
                iov[1].iov_len = request_user_data->chunk_size;
        } else if (request_user_data->send_count < (request_user_data->chunk_size_str_len + request_user_data->chunk_size)) {
                iov[1].iov_base = encoder_output->cache_addr + request_user_data->current_send_position + (request_user_data->send_count - request_user_data->chunk_size_str_len);
                iov[1].iov_len = request_user_data->chunk_size - (request_user_data->send_count - request_user_data->chunk_size_str_len);
        } else if (request_user_data->send_count > (request_user_data->chunk_size_str_len + request_user_data->chunk_size)) {
                iov[2].iov_base = "\n";
                iov[2].iov_len = 1;
        }
        ret = writev (request_data->sock, iov, 3);
        if (ret == -1) {
                GST_DEBUG ("write error %s sock %d", g_strerror (errno), request_data->sock);
        }

        return ret;
}

/*
 * return -1 means the gop is current output gop.
 */
static gint64
get_current_gop_end (EncoderOutput *encoder_output, RequestDataUserData *request_user_data)
{
        gint32 current_gop_size, n;
        gint64 current_gop_end_addr;

        /* read gop size. */
        n = encoder_output->cache_size - request_user_data->current_rap_addr;
        if (n >= 12) {
                memcpy (&current_gop_size, encoder_output->cache_addr + request_user_data->current_rap_addr + 8, 4);
        } else if (n > 8) {
                memcpy (&current_gop_size, encoder_output->cache_addr + request_user_data->current_rap_addr + 8, n - 8);
                memcpy (&current_gop_size + n - 8, encoder_output->cache_addr, 12 - n);
        } else {
                memcpy (&current_gop_size, encoder_output->cache_addr + 8 - n, 4);
        }
        if (current_gop_size == 0) {
                /* current output gop. */
                return -1;
        }
        current_gop_end_addr = request_user_data->current_rap_addr + current_gop_size;
        if (current_gop_end_addr > encoder_output->cache_size) {
                current_gop_end_addr -= encoder_output->cache_size;
        }

        return current_gop_end_addr;
}

static GstClockTime
send_chunk (EncoderOutput *encoder_output, RequestData *request_data)
{
        RequestDataUserData *request_user_data;
        gint64 current_gop_end_addr, tail_addr;
        gint32 ret;

        request_user_data = request_data->user_data;

        sem_wait (request_user_data->encoder->mutex);
        tail_addr = *(encoder_output->tail_addr);
        current_gop_end_addr = get_current_gop_end (encoder_output, request_user_data);

        if (request_user_data->send_count == request_user_data->chunk_size + request_user_data->chunk_size_str_len + 2) {
                /* completly send a chunk, prepare next. */
                request_user_data->current_send_position += request_user_data->send_count - request_user_data->chunk_size_str_len - 2;
                if (request_user_data->current_send_position == encoder_output->cache_size) {
                        request_user_data->current_send_position = 0;
                }
                g_free (request_user_data->chunk_size_str);
                request_user_data->chunk_size_str_len = 0;
                request_user_data->chunk_size = 0;
        }

        if (request_user_data->current_send_position == current_gop_end_addr) {
                /* next gop. */
                request_user_data->current_rap_addr = current_gop_end_addr;
                if (request_user_data->current_send_position + 12 < encoder_output->cache_size) {
                        request_user_data->current_send_position += 12;
                } else {
                        request_user_data->current_send_position = request_user_data->current_send_position + 12 - encoder_output->cache_size;
                }
                current_gop_end_addr = get_current_gop_end (encoder_output, request_user_data);
        }
        sem_post (request_user_data->encoder->mutex);

        if (request_user_data->chunk_size == 0) {
                if (current_gop_end_addr == -1) {
                        /* current output gop. */
                        if ((tail_addr - request_user_data->current_send_position) > 16384) {
                                request_user_data->chunk_size = 16384;
                        } else if (tail_addr > request_user_data->current_send_position) {
                                /* send to tail. */
                                request_user_data->chunk_size = tail_addr - request_user_data->current_send_position;
                        } else if (tail_addr == request_user_data->current_send_position) {
                                /* no data available, wait a while. */
                                return 10 * GST_MSECOND + g_random_int_range (1, 1000000);
                        } else if ((encoder_output->cache_size - request_user_data->current_send_position) > 16384) {
                                request_user_data->chunk_size = 16384;
                        } else {
                                request_user_data->chunk_size = encoder_output->cache_size - request_user_data->current_send_position;
                        }
                } else {
                        /* completely output gop. */
                        if ((current_gop_end_addr - request_user_data->current_send_position) > 16384) {
                                request_user_data->chunk_size = 16384;
                        } else if (current_gop_end_addr > request_user_data->current_send_position) {
                                /* send to gop end. */
                                request_user_data->chunk_size = current_gop_end_addr - request_user_data->current_send_position;
                        } else if (current_gop_end_addr == request_user_data->current_send_position) {
                                /* no data available, wait a while. */
                                return 10 * GST_MSECOND + g_random_int_range (1, 1000000); //FIXME FIXME
                        } else {
                                /* send to cache end. */
                                request_user_data->chunk_size = encoder_output->cache_size - request_user_data->current_send_position;
                        }
                }
                request_user_data->chunk_size_str = g_strdup_printf("%x\r\n", request_user_data->chunk_size);
                request_user_data->chunk_size_str_len = strlen (request_user_data->chunk_size_str);
                request_user_data->send_count = 0;
        }

        /* send data. */
        ret = send_data (encoder_output, request_data);
        if (ret == -1) {
                return GST_CLOCK_TIME_NONE;
        } else {
                request_user_data->send_count += ret;
                request_data->bytes_send += ret;
        }
        if (request_user_data->send_count == request_user_data->chunk_size + request_user_data->chunk_size_str_len + 2) {
                /* send complete, wait 10 ms. */
                return 10 * GST_MSECOND + g_random_int_range (1, 1000000);
        } else {
                /* not send complete, blocking, wait 200 ms. */
                return 200 * GST_MSECOND + g_random_int_range (1, 1000000);
        }
}

/**
 * httpserver_dispatcher:
 * @data: RequestData type pointer
 * @user_data: httpstreaming type pointer
 *
 * Process http request.
 *
 * Returns: positive value if have not completed the processing, for example live streaming.
 *      0 if have completed the processing.
 */
static GstClockTime
httpstreaming_dispatcher (gpointer data, gpointer user_data)
{
        RequestData *request_data = data;
        HTTPStreaming *httpstreaming = (HTTPStreaming *)user_data;
        gchar *buf;
        int i = 0, j, ret;
        Encoder *encoder;
        EncoderOutput *encoder_output;
        Channel *channel;
        RequestDataUserData *request_user_data;
        GstClockTime ret_clock_time;

        channel = get_channel (httpstreaming, request_data);
        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_DEBUG ("new request arrived, socket is %d, uri is %s", request_data->sock, request_data->uri);
                encoder_output = get_encoder_output (httpstreaming, request_data);
                if (encoder_output == NULL) {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                        httpserver_write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                } else if ((request_data->parameters[0] == '\0') || (request_data->parameters[0] == 'b')) {
                        /* default operator is play, ?bitrate= */
                        GST_ERROR ("Play %s.", request_data->uri);
                        request_user_data = (RequestDataUserData *)g_malloc (sizeof (RequestDataUserData));//FIXME
                        if (request_user_data == NULL) {
                                GST_ERROR ("Internal Server Error, g_malloc for request_user_data failure.");
                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                httpserver_write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }

                        if (*(encoder_output->head_addr) == *(encoder_output->tail_addr)) {
                                GST_DEBUG ("%s unready.", request_data->uri);
                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                httpserver_write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }

                        /* let send_chunk send new chunk. */
                        encoder = get_encoder (request_data->uri, httpstreaming->itvencoder->channel_array);
                        request_user_data->encoder = encoder;
                        request_user_data->chunk_size = 0;
                        request_user_data->send_count = 2;
                        request_user_data->chunk_size_str = g_strdup ("");
                        request_user_data->chunk_size_str_len = 0;
                        request_user_data->encoder_output = encoder_output;
                        request_user_data->current_rap_addr = *(encoder_output->last_rap_addr);
                        request_user_data->current_send_position = *(encoder_output->last_rap_addr) + 12;
                        request_user_data->channel_age = channel->age;
                        request_data->user_data = request_user_data;
                        request_data->bytes_send = 0;
                        buf = g_strdup_printf (http_chunked, PACKAGE_NAME, PACKAGE_VERSION);
                        httpserver_write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return gst_clock_get_time (httpstreaming->httpserver->system_clock) + GST_MSECOND;
                } else {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                        httpserver_write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                }
                break;
        case HTTP_CONTINUE:
                request_user_data = request_data->user_data;
                if ((request_user_data->channel_age != channel->age) ||
                    (*(channel->output->state) != GST_STATE_PLAYING)) {
                        g_free (request_data->user_data);
                        request_data->user_data = NULL;
                        return 0;
                }
                encoder_output = request_user_data->encoder_output;
                if (request_user_data->current_send_position == *(encoder_output->tail_addr)) {
                        /* no more stream, wait 10ms */
                        GST_DEBUG ("current:%llu == tail:%llu", request_user_data->current_send_position, encoder_output->tail_addr);
						return gst_clock_get_time (httpstreaming->httpserver->system_clock) + 500 * GST_MSECOND + g_random_int_range (1, 1000000);
                }
                ret_clock_time = send_chunk (encoder_output, request_data);
                if (ret_clock_time != GST_CLOCK_TIME_NONE)
                {
                        return ret_clock_time + gst_clock_get_time (httpstreaming->httpserver->system_clock);
                }
                else
                {
                        return GST_CLOCK_TIME_NONE;
                }
        case HTTP_FINISH:
                g_free (request_data->user_data);
                request_data->user_data = NULL;
                return 0;
        default:
                GST_ERROR ("Unknown status %d", request_data->status);
                buf = g_strdup_printf (http_400, PACKAGE_NAME, PACKAGE_VERSION);
                httpserver_write (request_data->sock, buf, strlen (buf));
                g_free (buf);
                return 0;
        }
}

gint
httpstreaming_start (HTTPStreaming *httpstreaming, gint maxthreads)
{
        GValue *value;
        GstStructure *structure;
        gchar *p, **pp;
        gint port;

        /* get streaming listen port */
        value = (GValue *)gst_structure_get_value (httpstreaming->itvencoder->configure->data, "server");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "httpstreaming");
        p = (gchar *)g_value_get_string (value);
        pp = g_strsplit (p, ":", 0);
        port = atoi (pp[1]);
        g_strfreev (pp);

        /* start http streaming */
        httpstreaming->httpserver = httpserver_new ("maxthreads", maxthreads, "port", port, NULL);
        if (httpserver_start (httpstreaming->httpserver, httpstreaming_dispatcher, httpstreaming) != 0) {
                GST_ERROR ("Start streaming httpserver error!");
                exit (0);
        }

        return 0;
}

