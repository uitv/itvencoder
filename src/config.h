
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <glib-object.h>

typedef struct _Config      Config;
typedef struct _ConfigClass ConfigClass;

struct _Config {
    GObject parent;
};

struct _ConfigClass {
    GObjectClass parent;
};

#define TYPE_CONFIG           (config_get_type())
#define CONFIG(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CONFIG, Config))
#define CONFIG_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_CONFIG, ConfigClass))
#define IS_CONFIG(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CONFIG))
#define IS_CONFIG_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_CONFIG))
#define CONFIG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_CONFIG, ConfigClass))
#define config_new(...)       (g_object_new(TYPE_CONFIG, ## __VA_ARGS__, NULL))

GType config_get_type (void);

#endif /* __CONFIG_H__ */
