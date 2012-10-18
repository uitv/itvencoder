/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include <string.h>
#include "httpserver.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

static void httpserver_class_init (HTTPServerClass *httpserverclass);
static void httpserver_init (HTTPServer *httpserver);
static void *request_dispatcher (enum mg_event event, struct mg_connection *conn);

static void *request_dispatcher (enum mg_event event, struct mg_connection *conn)
{
        const struct mg_request_info *request_info = mg_get_request_info(conn);

        if (event == MG_NEW_REQUEST) {
                mg_printf(conn,
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Server: iencoder/0.0.1\r\n"
                          "Content-Length: %d\r\n"
                          "\r\n"
                          "%s",
                          strlen (request_info->uri), request_info->uri);
                return "";
        } else {
                return NULL;
        }
}

static void
httpserver_class_init (HTTPServerClass *httpserverclass)
{
}

static void
httpserver_init (HTTPServer *httpserver)
{
        const char *options[] = {"listening_ports", "8000", "enable_keep_alive", "yes", NULL};

        httpserver->ctx = mg_start(&request_dispatcher, NULL, options);
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

