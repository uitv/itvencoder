/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <gst/gst.h>
#include "version.h"

typedef struct _Log      Log;
typedef struct _LogClass LogClass;

struct _Log {
        GObject parent;

        gchar *log_path;
        gint log_hd;
};

struct _LogClass {
        GObjectClass parent;
};

#define TYPE_LOG           (log_get_type())
#define LOG(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LOG, Log))
#define LOG_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_LOG, LogClass))
#define IS_LOG(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_LOG))
#define IS_LOG_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_LOG))
#define LOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_LOG, LogClass))
#define log_new(...)       (g_object_new(TYPE_LOG, ## __VA_ARGS__, NULL))

GType log_get_type (void);

gint log_set_log_handler (Log *log);
gint log_reopen (Log *log);

#endif /* __LOG_H__ */
