/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

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
static gint extract_channel_index (gchar *uri);
static gint extract_encoder_index (gchar *uri);

static
gint extract_channel_index (gchar *uri)
{
        GRegex *regex;
        GMatchInfo *match_info;
        gchar *channel;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                channel = g_match_info_fetch_named (match_info, "channel");
                GST_DEBUG ("channel is %s", channel);
                return atoi (channel);
        }
        return -1;
}

static
gint extract_encoder_index (gchar *uri)
{
        GRegex *regex;
        GMatchInfo *match_info;
        gchar *encoder;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+)/encoder/(?<encoder>[0-9]+)$", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                encoder = g_match_info_fetch_named (match_info, "encoder");
                GST_DEBUG ("encoder is %s", encoder);
                return atoi (encoder);
        }
        return -1;
}

static void *request_dispatcher (enum mg_event event, struct mg_connection *conn)
{
        const struct mg_request_info *request_info = mg_get_request_info(conn);
        ITVEncoder *itvencoder = (ITVEncoder *)mg_get_user_data (conn);
        Channel *channel;
        gint c, e;

        channel = g_array_index (itvencoder->channel_array, gpointer, 1);
        gchar *s = channel->decoder_pipeline->pipeline_string;
        if (event == MG_NEW_REQUEST) {
                switch (request_info->uri[1]) {
                case 'c':
                        e = extract_encoder_index (request_info->uri),
                        c = extract_channel_index (request_info->uri);
                        if (c == -1 || e == -1) {
                                GST_WARNING ("bad channel encoder path");
                        } else {
                                GST_DEBUG ("channel: %d; encoder: %d", c, e);
                        }
                        break;
                }
                mg_printf(conn,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Server: iencoder/0.0.1\r\n"
                          "Content-Length: %d\r\n"
                          "\r\n"
                          "%s\n%s",
                          strlen (request_info->uri) + strlen (s) + 1,
                          request_info->uri,
                          s);
                return "";
        } else {
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
