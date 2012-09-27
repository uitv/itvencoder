
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <glib-object.h>
#include "config.h"

enum {
        CONFIG_PROP_0,
        CONFIG_PROP_CONFIG_FILE_PATH,
};

static void config_class_init(ConfigClass *configclass);
static void config_init(Config *config);
static GObject *config_constructor(GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void config_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void config_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);

static void
config_class_init(ConfigClass *configclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(configclass);
        GParamSpec *config_param;

        g_object_class->constructor = config_constructor;
        g_object_class->set_property = config_set_property;
        g_object_class->get_property = config_get_property;

        config_param = g_param_spec_string("config_file_path",
                                           "configf",
                                           "config file path",
                                           "itvencoder.conf",
                                           G_PARAM_WRITABLE | G_PARAM_READABLE);
        g_object_class_install_property(g_object_class, CONFIG_PROP_CONFIG_FILE_PATH, config_param);
}

static void
config_init(Config *config)
{
}

static GObject *
config_constructor(GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
config_set_property(GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CONFIG(obj));

        switch(prop_id) {
        case CONFIG_PROP_CONFIG_FILE_PATH:
                CONFIG(obj)->config_file_path = (gchar *)g_value_dup_string(value); //TODO: should release dup string config_file_path?
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
config_get_property(GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Config *config = CONFIG(obj);

        switch(prop_id) {
        case CONFIG_PROP_CONFIG_FILE_PATH:
                g_value_set_string(value, config->config_file_path);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
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

