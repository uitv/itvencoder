/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __HTTPMGMT_H__
#define __HTTPMGMT_H__

#include <gst/gst.h>

#include "config.h"
#include "configure.h"
#include "itvencoder.h"
#include "httpserver.h"

typedef struct _HTTPMgmt      HTTPMgmt;
typedef struct _HTTPMgmtClass HTTPMgmtClass;

struct _HTTPMgmt {
        GObject parent;

        GstClock *system_clock;
        ITVEncoder *itvencoder;
        HTTPServer *httpserver; /* management via http */
};

struct _HTTPMgmtClass {
        GObjectClass parent;
};

#define TYPE_HTTPMGMT           (httpmgmt_get_type())
#define HTTPMGMT(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_HTTPMGMT, HTTPMgmt))
#define HTTPMGMT_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_HTTPMGMT, HTTPMgmtClass))
#define IS_HTTPMGMT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_HTTPMGMT))
#define IS_HTTPMGMT_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_HTTPMGMT))
#define HTTPMGMT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_HTTPMGMT, HTTPMgmtClass))
#define httpmgmt_new(...)       (g_object_new(TYPE_HTTPMGMT, ## __VA_ARGS__, NULL))

GType httpmgmt_get_type (void);
gint httpmgmt_start (HTTPMgmt *httpmgmt);

#endif /* __HTTPMGMT_H__ */
