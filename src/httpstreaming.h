/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __HTTPSTREAMING_H__
#define __HTTPSTREAMING_H__

#include <gst/gst.h>
#include "config.h"
#include "channel.h"
#include "httpserver.h"
#include "itvencoder.h"

typedef struct _RequestDataUserData {
        gchar *current_rap_addr;
        gchar *current_send_position;
        gint chunk_size;
        gchar *chunk_size_str;
        gint chunk_size_str_len;
        gint send_count;
        gpointer encoder_output;
} RequestDataUserData;

typedef struct _HTTPStreaming      HTTPStreaming;
typedef struct _HTTPStreamingClass HTTPStreamingClass;

struct _HTTPStreaming {
        GObject parent;
    
        ITVEncoder *itvencoder;
        HTTPServer *httpserver; /* streaming via http */
};

struct _HTTPStreamingClass {
        GObjectClass parent;
};

#define TYPE_HTTPSTREAMING           (httpstreaming_get_type())
#define HTTPSTREAMING(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_HTTPSTREAMING, HTTPStreaming))
#define HTTPSTREAMING_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_HTTPSTREAMING, HTTPStreamingClass))
#define IS_HTTPSTREAMING(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_HTTPSTREAMING))
#define IS_HTTPSTREAMING_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_HTTPSTREAMING))
#define HTTPSTREAMING_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_HTTPSTREAMING, HTTPStreamingClass))
#define httpstreaming_new(...)       (g_object_new(TYPE_HTTPSTREAMING, ## __VA_ARGS__, NULL))

GType httpstreaming_get_type (void);
gint httpstreaming_start (HTTPStreaming *httpstreaming, gint maxthreads);

#endif /* __HTTPSTREAMING_H__ */
