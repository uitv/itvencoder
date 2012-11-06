/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <netinet/in.h>
#include <gst/gst.h>
#include "mongoose.h"
#include "itvencoder.h"

typedef struct _HTTPServer      HTTPServer;
typedef struct _HTTPServerClass HTTPServerClass;

enum request_method {
        HTTP_GET,
        HTTP_PUT
};

enum http_version {
        HTTP_1_1
};

struct http_headers {
        gchar *name;
        gchar *value;
};

enum session_status {
        HTTP_CONNECTED,
        HTTP_REQUEST,
        HTTP_CONTINUE
};

#define kRequestBufferSize 1024
#define kMaxThreads 10 /* number of threads in thread pool */
#define kMaxRequests 512

typedef struct _RequestData {
        gint sock;
        struct sockaddr client_addr;
        GTimeVal birth_time;
        guint64 bytes_send;
        enum session_status status; /* live over http need keeping tcp link */
        gchar raw_request[kRequestBufferSize];
        enum request_method method;
        gchar uri[256];
        enum http_version version;
        gint num_headers;
        struct http_headers headers[64];
} RequestData;

struct _HTTPServer {
        GObject parent;

        ITVEncoder *itvencoder;
        struct mg_context *ctx;

        gint listen_port;
        gint listen_sock;
        gint epollfd;
        GThread *server_thread;
        GThreadPool *thread_pool;
        GFunc user_callback;
        gpointer user_data;
        GQueue *request_data_queue;
};

struct _HTTPServerClass {
        GObjectClass parent;
};

#define TYPE_HTTPSERVER           (httpserver_get_type())
#define HTTPSERVER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_HTTPSERVER, HTTPServer))
#define HTTPSERVER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_HTTPSERVER, HTTPServerClass))
#define IS_HTTPSERVER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_HTTPSERVER))
#define IS_HTTPSERVER_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_HTTPSERVER))
#define HTTPSERVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_HTTPSERVER, HTTPServerClass))
#define httpserver_new(...)       (g_object_new(TYPE_HTTPSERVER, ## __VA_ARGS__, NULL))

GType httpserver_get_type (void);
gint httpserver_start (HTTPServer *httpserver, GFunc user_callback, gpointer user_data);

#endif /* __HTTPSERVER_H__ */
