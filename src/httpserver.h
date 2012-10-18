/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <gst/gst.h>
#include "mongoose.h"

typedef struct _HTTPServer      HTTPServer;
typedef struct _HTTPServerClass HTTPServerClass;

struct _HTTPServer {
    GObject parent;
    struct mg_context *ctx;
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

#endif /* __HTTPSERVER_H__ */
