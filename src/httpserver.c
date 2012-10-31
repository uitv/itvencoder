/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <fcntl.h>
#include <gst/gst.h>
#include <string.h>
#include "httpserver.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        HTTPSERVER_PROP_0,
        HTTPSERVER_PROP_ITVENCODER,
};

static void httpserver_class_init (HTTPServerClass *httpserverclass);
static void httpserver_init (HTTPServer *httpserver);
static GObject *httpserver_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpserver_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpserver_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void *request_dispatcher (enum mg_event event, struct mg_connection *conn);
static gint http_get_encoder (struct mg_connection *conn, ITVEncoder *itvencoder);

static gint
http_get_encoder (struct mg_connection *conn, ITVEncoder *itvencoder)
{
        gchar *uri;
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c, *e;
        Channel *channel;
        EncoderPipeline *encoder;
        gint ret=-1, socket;

        uri = mg_get_request_info (conn)->uri;
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
                                socket = mg_get_socket (conn);
                                encoder->httprequest_socket_list = g_slist_append (encoder->httprequest_socket_list, GINT_TO_POINTER (socket));
                                ret = 0;
                        } 
                        g_free (e);
                } 
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);
        return ret;
}

static void *request_dispatcher (enum mg_event event, struct mg_connection *conn)
{
        const struct mg_request_info *request_info = mg_get_request_info(conn);
        ITVEncoder *itvencoder = (ITVEncoder *)mg_get_user_data (conn);
        Channel *channel;
        gchar *message_404 = "No such encoder channel.";

        channel = g_array_index (itvencoder->channel_array, gpointer, 1);
        gchar *s = channel->decoder_pipeline->pipeline_string;
        switch (event) {
        case MG_NEW_REQUEST:
                switch (request_info->uri[1]) {
                case 'c': /* uri is /channel..., maybe request for encoder streaming */
                        if (http_get_encoder (conn, itvencoder) != 0) {
                                mg_printf (conn,
                                        "HTTP/1.1 404 Not Found\r\n"
                                        "Server: %s-%s\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Connection: close\r\n"
                                        "Content-Length: %d\r\n\r\n"
                                        "%s",
                                        ENCODER_NAME,
                                        ENCODER_VERSION,
                                        strlen (message_404),
                                        message_404
                                );
                        } else {
                                mg_printf(conn,
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: video/mpeg\r\n"
                                        "Server: %s-%s\r\n"
                                        "Transfer-Encoding: chunked\r\n"
                                        "\r\n",
                                        ENCODER_NAME,
                                        ENCODER_VERSION
                                );
                        }
                        return "";
                }
                mg_printf(conn,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Server: %s-%s\r\n"
                          "Content-Length: %d\r\n"
                          "\r\n"
                          "%s\n%s",
                          ENCODER_NAME,
                          ENCODER_VERSION,
                          strlen (request_info->uri) + strlen (s) + 1,
                          request_info->uri,
                          s);
                return "";
        default:
                GST_INFO ("event is %d", event);
                return NULL;
        }
}

static void
httpserver_class_init (HTTPServerClass *httpserverclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(httpserverclass);
        GParamSpec *param;

        g_object_class->constructor = httpserver_constructor;
        g_object_class->set_property = httpserver_set_property;
        g_object_class->get_property = httpserver_get_property;

        param = g_param_spec_object (
                "itvencoder",
                "itvencoderf",
                "itvencoder context",
                TYPE_ITVENCODER,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPSERVER_PROP_ITVENCODER, param);
}

static void
httpserver_init (HTTPServer *httpserver)
{
}

static GObject *
httpserver_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        GST_LOG ("httpserver constructor");

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
httpserver_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail (IS_HTTPSERVER (obj));

        GST_LOG ("httpserver set property");

        switch(prop_id) {
        case HTTPSERVER_PROP_ITVENCODER:
                HTTPSERVER(obj)->itvencoder = g_value_get_object (value); 
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
httpserver_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        HTTPServer *httpserver = HTTPSERVER (obj);

        GST_LOG ("httpserver get property");

        switch(prop_id) {
        case HTTPSERVER_PROP_ITVENCODER:
                g_value_set_object (value, httpserver->itvencoder);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
httpserver_get_type (void)
{
        static GType type = 0;

        GST_LOG ("httpserver get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (HTTPServerClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc) httpserver_class_init,
                NULL,
                NULL,
                sizeof (HTTPServer),
                0,
                (GInstanceInitFunc) httpserver_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "HTTPServer", &info, 0);

        return type;
}

gint
httpserver_start (HTTPServer *httpserver)
{
        const char *options[] = {"listening_ports", "8000", "enable_keep_alive", "yes", NULL};

        httpserver->ctx = mg_start (&request_dispatcher, httpserver->itvencoder, options);

        return 0;
}
