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

static EncoderOutput *
get_encoder_output (HTTPStreaming *httpstreaming, RequestData *request_data)
{
        gint channel_index, encoder_index;
        Channel *channel;

        channel_index = itvencoder_url_channel_index (request_data->uri);
        channel = g_array_index (httpstreaming->itvencoder->channel_array, gpointer, channel_index);
        encoder_index = itvencoder_url_encoder_index (request_data->uri);

        return &channel->output->encoders[encoder_index];
}

static gint
send_data (EncoderOutput *encoder_output, RequestData *request_data)
{
        RequestDataUserData *request_user_data;
        struct iovec iov[3];
        gint ret;
        gint chunk_size_str_len;

        request_user_data = request_data->user_data;
        chunk_size_str_len = strlen (request_user_data->chunk_size_str);
        iov[0].iov_base = NULL;
        iov[0].iov_len = 0;
        iov[1].iov_base = NULL;
        iov[1].iov_len = 0;
        iov[2].iov_base = "\r\n";
        iov[2].iov_len = 2;
        if (request_user_data->send_count < chunk_size_str_len) {
                iov[0].iov_base = request_user_data->chunk_size_str + request_user_data->send_count;
                iov[0].iov_len = chunk_size_str_len - request_user_data->send_count;
                iov[1].iov_base = request_user_data->current_send_position;
                iov[1].iov_len = request_user_data->chunk_size;
        } else if (request_user_data->send_count < (chunk_size_str_len + request_user_data->chunk_size)) {
                iov[1].iov_base = request_user_data->current_send_position + (request_user_data->send_count - chunk_size_str_len);
                iov[1].iov_len = request_user_data->chunk_size - (request_user_data->send_count - chunk_size_str_len);
        } else if (request_user_data->send_count > (chunk_size_str_len + request_user_data->chunk_size)) {
                iov[2].iov_base = "\n";
                iov[2].iov_len = 1;
        }
        GST_ERROR ("%llu:%d; %llu:%d; %llu:%d", iov[0].iov_base, iov[0].iov_len, iov[1].iov_base, iov[1].iov_len, iov[2].iov_base, iov[2].iov_len);
        ret = writev (request_data->sock, iov, 3);
        if (ret == -1) {
                GST_ERROR ("write error %s sock %d", g_strerror (errno), request_data->sock);
        } else {
                GST_ERROR ("send position %llu, send : %d ", request_user_data->current_send_position, ret);
        }

        return ret;
}

static GstClockTime
send_chunk (EncoderOutput *encoder_output, RequestData *request_data)
{
        RequestDataUserData *request_user_data;
        gchar *current_gop_end_addr;
        gint32 current_gop_size, ret;

        request_user_data = request_data->user_data;

        /* if last chunk has not completely send, try again. */
        if (request_user_data->send_count < request_user_data->chunk_size + strlen (request_user_data->chunk_size_str) + 2) {
                GST_ERROR ("resend");
                ret = send_data (encoder_output, request_data);
                if (ret == -1) {
                        return GST_CLOCK_TIME_NONE;
                } 
                request_user_data->send_count += ret;
                request_data->bytes_send += ret;
                if (request_user_data->send_count == request_user_data->chunk_size + strlen (request_user_data->chunk_size_str) + 2) {
                        /* send complete. */
                        memcpy (&current_gop_size, request_user_data->current_rap_addr + 8, 4);
                        current_gop_end_addr = request_user_data->current_rap_addr + current_gop_size;
                        request_user_data->current_send_position += request_user_data->send_count - strlen (request_user_data->chunk_size_str) - 2;
                        if (request_user_data->current_send_position == current_gop_end_addr) {
                                request_user_data->current_rap_addr = current_gop_end_addr + 1;
                        }
                }
                return gst_util_get_timestamp () + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
        }

        /* send next chunk. */
        if (request_user_data->current_rap_addr == encoder_output->last_rap_addr) {
                GST_ERROR ("current gop size is 0 output: %llu: send:%llu.", encoder_output->tail_addr, request_user_data->current_send_position);
                if (encoder_output->tail_addr == request_user_data->current_send_position) {
                        /* no more data. */
                        return gst_util_get_timestamp () + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
                }
                /* current uncompleted gop. */
                g_free (request_user_data->chunk_size_str);
                if (encoder_output->tail_addr < encoder_output->cache_end_addr) {
                        /* send to tail. */
                        request_user_data->chunk_size = encoder_output->tail_addr - request_user_data->current_send_position;
                } else {
                        request_user_data->chunk_size = encoder_output->cache_addr - request_user_data->current_send_position;
                }
                GST_ERROR ("chunk size is %d.", request_user_data->chunk_size);
                request_user_data->chunk_size_str = g_strdup_printf("%x\r\n", request_user_data->chunk_size);
                request_user_data->send_count = 0;
                ret = send_data (encoder_output, request_data);
                if (ret == -1) {
                        return GST_CLOCK_TIME_NONE;
                } 
                request_user_data->send_count += ret;
                if (ret + request_user_data->send_count == request_user_data->chunk_size + strlen (request_user_data->chunk_size_str) + 2) {
                        /* send complete. */
                        request_user_data->current_send_position += request_user_data->send_count - strlen (request_user_data->chunk_size_str) - 2;
                } 
                return gst_util_get_timestamp () + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
        } else {
                memcpy (&current_gop_size, request_user_data->current_rap_addr + 8, 4);
                current_gop_end_addr = request_user_data->current_rap_addr + current_gop_size;
                GST_ERROR ("current gop size is %d.", current_gop_size);
                g_free (request_user_data->chunk_size_str);
                if ((current_gop_end_addr - request_user_data->current_send_position) > 16384) {
                        /* send 64k. */
                        request_user_data->chunk_size = 16384;
                } else if (encoder_output->cache_end_addr > current_gop_end_addr) {
                        /* send to gop end. */
                        request_user_data->chunk_size = current_gop_end_addr - request_user_data->current_send_position;
                } else {
                        /* send to cache end. */
                        request_user_data->chunk_size = encoder_output->cache_addr - request_user_data->current_send_position;
                }
                GST_ERROR ("chunk size is %d.", request_user_data->chunk_size);
                request_user_data->chunk_size_str = g_strdup_printf("%x\r\n", request_user_data->chunk_size);
                request_user_data->send_count = 0;
                ret = send_data (encoder_output, request_data);
                if (ret == -1) {
                        return GST_CLOCK_TIME_NONE;
                }
                request_user_data->send_count += ret;
                request_data->bytes_send += ret;
                if (ret + request_user_data->send_count == request_user_data->chunk_size + strlen (request_user_data->chunk_size_str) + 2) {
                        /* send complete. */
                        request_user_data->current_send_position += request_user_data->send_count - strlen (request_user_data->chunk_size_str) - 2;
                } 
                if (request_user_data->current_send_position == current_gop_end_addr) {
                        request_user_data->current_rap_addr = current_gop_end_addr + 1;
                }
                GST_ERROR ("current position : %llu", request_user_data->current_send_position);
                return gst_util_get_timestamp () + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
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

        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_INFO ("new request arrived, socket is %d, uri is %s", request_data->sock, request_data->uri);
                encoder = channel_get_encoder (request_data->uri, httpstreaming->itvencoder->channel_array);
                encoder_output = get_encoder_output (httpstreaming, request_data);
                if (encoder_output == NULL) {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                } else if ((request_data->parameters[0] == '\0') || /* default operator is play */
                           (request_data->parameters[0] == 'b')) { /* ?bitrate= */
                        GST_INFO ("Play command");
#if 0
                        if (encoder->state != GST_STATE_PLAYING) {
                                GST_ERROR ("Play encoder it's status is not PLAYING.");
                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }
                        if (encoder->current_output_position == -1) {
                                GST_ERROR ("Play encoder it's output position is -1.");
                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }
                        if (*(encoder->total_count) < ENCODER_RING_SIZE) {
                                GST_ERROR ("Caching, please wait a while.");
                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }
#endif
                        request_user_data = (RequestDataUserData *)g_malloc (sizeof (RequestDataUserData));//FIXME
                        if (request_user_data == NULL) {
                                GST_ERROR ("Internal Server Error, g_malloc for request_user_data failure.");
                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                write (request_data->sock, buf, strlen (buf));
                                g_free (buf);
                                return 0;
                        }
                        request_user_data->chunk_size = 0;
                        request_user_data->send_count = 2;
                        request_user_data->chunk_size_str = g_strdup ("");
                        request_user_data->encoder_output = encoder_output;
                        //request_user_data->encoder = encoder;
                        /* should send a IDR with pat and pmt first */
                        request_user_data->current_rap_addr = encoder_output->last_rap_addr;
                        request_user_data->current_send_position = encoder_output->last_rap_addr + 12;//(encoder->current_output_position + 25) % ENCODER_RING_SIZE;
                        //while (GST_BUFFER_FLAG_IS_SET (encoder->output_ring[request_user_data->current_send_position], GST_BUFFER_FLAG_DELTA_UNIT)) {
                        //        request_user_data->current_send_position = (request_user_data->current_send_position + 1) % ENCODER_RING_SIZE;
                        //}
                        request_data->user_data = request_user_data;
                        request_data->bytes_send = 0;
                        buf = g_strdup_printf (http_chunked, PACKAGE_NAME, PACKAGE_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return gst_util_get_timestamp () + GST_MSECOND;
                }
        case HTTP_CONTINUE:
                request_user_data = request_data->user_data;
                encoder_output = request_user_data->encoder_output;
                if (request_user_data->current_send_position == encoder_output->tail_addr) {
                        /* no more stream, wait 10ms */
                        GST_ERROR ("current send position is the same as tail addr, current: %llu, tail: %llu", request_user_data->current_send_position, encoder_output->tail_addr);
                        return gst_util_get_timestamp () + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
                }

                return send_chunk (encoder_output, request_data);
#if 0
                encoder = request_user_data->encoder;
                
                if (encoder->state != GST_STATE_PLAYING) {
                        /* encoder pipeline's state is not playing, return 0 */
                        return 0;
                }

                i = request_user_data->current_send_position;
                i %= ENCODER_RING_SIZE;
                for (;;) {
                        if (i == encoder->current_output_position) { // catch up encoder output.
                                break;
                        }
                        chunksize = g_strdup_printf("%x\r\n", GST_BUFFER_SIZE (encoder->output_ring[i]));
                        if (request_user_data->last_send_count < strlen (chunksize)) {
                                iov[0].iov_base = chunksize + request_user_data->last_send_count;
                                iov[0].iov_len = strlen (chunksize) - request_user_data->last_send_count;
                                iov[1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i]);
                                iov[1].iov_len = GST_BUFFER_SIZE (encoder->output_ring[i]);
                                iov[2].iov_base = "\r\n";
                                iov[2].iov_len = 2;
                        } else if (request_user_data->last_send_count < (strlen (chunksize) + GST_BUFFER_SIZE (encoder->output_ring[i]))) {
                                iov[0].iov_base = NULL;
                                iov[0].iov_len = 0;
                                iov[1].iov_base = GST_BUFFER_DATA (encoder->output_ring[i]) + (request_user_data->last_send_count - strlen (chunksize));
                                iov[1].iov_len = GST_BUFFER_SIZE (encoder->output_ring[i]) - (request_user_data->last_send_count - strlen (chunksize));
                                iov[2].iov_base = "\r\n";
                                iov[2].iov_len = 2;
                        } else if (request_user_data->last_send_count == (strlen (chunksize) + GST_BUFFER_SIZE (encoder->output_ring[i]))) {
                                iov[0].iov_base = NULL;
                                iov[0].iov_len = 0;
                                iov[1].iov_base = NULL;
                                iov[1].iov_len = 0;
                                iov[2].iov_base = "\r\n";
                                iov[2].iov_len = 2;
                        } else if (request_user_data->last_send_count > (strlen (chunksize) + GST_BUFFER_SIZE (encoder->output_ring[i]))) {
                                iov[0].iov_base = NULL;
                                iov[0].iov_len = 0;
                                iov[1].iov_base = NULL;
                                iov[1].iov_len = 0;
                                iov[2].iov_base = "\n";
                                iov[2].iov_len = 1;
                        }
                        ret = writev (request_data->sock, iov, 3);
                        if (ret == -1) {
                                GST_DEBUG ("write error %s sock %d", g_strerror (errno), request_data->sock);
                                g_free (chunksize);
                                return GST_CLOCK_TIME_NONE;
                        } else if (ret < (iov[0].iov_len + iov[1].iov_len + iov[2].iov_len)) {
                                request_user_data->last_send_count += ret;
                                request_data->bytes_send += ret;
                                g_free (chunksize);
                                return gst_clock_get_time (httpstreaming->itvencoder->system_clock) + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
                        }
                        request_data->bytes_send += ret;
                        g_free (chunksize);
                        request_user_data->last_send_count = 0;
                        i = (i + 1) % ENCODER_RING_SIZE;
                        request_user_data->current_send_position = i;
                }
                return gst_clock_get_time (httpstreaming->itvencoder->system_clock) + 10 * GST_MSECOND + g_random_int_range (1, 1000000);
#endif
        case HTTP_FINISH:
                g_free (request_data->user_data);
                request_data->user_data = NULL;
                return 0;
        default:
                GST_ERROR ("Unknown status %d", request_data->status);
                buf = g_strdup_printf (http_400, PACKAGE_NAME, PACKAGE_VERSION);
                write (request_data->sock, buf, strlen (buf));
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

