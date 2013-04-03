/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__

#include <gst/gst.h>

typedef struct _Configure          Configure;
typedef struct _ConfigureClass     ConfigureClass;

typedef struct _ConfigurableVar {
        gint index;
        gchar *group;
        gchar *name;
        gchar *type;
        gchar *description;
        gchar *value;
} ConfigurableVar;

struct _Configure {
        GObject parent;

        gchar *file_path;
        
        /* raw configure file content */
        gchar *raw;

        /* raw configure file content size */
        gsize size;

        /* GstStructure type of configure data */
        GstStructure *data;

        /* Line array of configure, very configurable var should take one line */
        GArray *lines;

        /* Configuralbe var array */
        GArray *variables;
};

struct _ConfigureClass {
        GObjectClass parent;
};

#define TYPE_CONFIGURE           (configure_get_type())
#define CONFIGURE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CONFIGURE, Configure))
#define CONFIGURE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_CONFIGURE, ConfigureClass))
#define IS_CONFIGURE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CONFIGURE))
#define IS_CONFIGURE_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_CONFIGURE))
#define CONFIGURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_CONFIGURE, ConfigureClass))
#define configure_new(...)       (g_object_new(TYPE_CONFIGURE, ## __VA_ARGS__, NULL))

GType configure_get_type (void);
gint configure_load_from_file (Configure *configure);
gint configure_save_to_file (Configure *configure);
gchar* configure_get_current_var (Configure *configure);
gchar* configure_get_server_param (Configure *configure, gchar *param);
gchar* configure_get_source_pipeline (Configure *configure);

#endif /* __CONFIGURE_H__ */
