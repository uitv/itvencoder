
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <glib-object.h>
#include "config.h"

static void config_class_init(ConfigClass *configclass);
static void config_init(Config *config);

static void
config_class_init(ConfigClass *configclass)
{
}

static void
config_init(Config *config)
{
}

GType
config_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ConfigClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)config_class_init,
                NULL,
                NULL,
                sizeof(Config),
                0,
                (GInstanceInitFunc)config_init,
                NULL
        };
        type = g_type_register_static(G_TYPE_OBJECT, "Config", &info, 0);

        return type;
}

