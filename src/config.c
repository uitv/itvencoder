
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include <glob.h>

#include "config.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CONFIG_PROP_0,
        CONFIG_PROP_CONFIG_FILE_PATH,
};

static void config_class_init (ConfigClass *configclass);
static void config_init (Config *config);
static GObject *config_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void config_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void config_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static gint config_load_config_file_func (Config *config);
static gint config_save_config_file_func (Config *config);

static void
config_class_init (ConfigClass *configclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(configclass);
        GParamSpec *config_param;

        g_object_class->constructor = config_constructor;
        g_object_class->set_property = config_set_property;
        g_object_class->get_property = config_get_property;

        configclass->config_load_config_file_func = config_load_config_file_func;
        configclass->config_save_config_file_func = config_save_config_file_func;

        config_param = g_param_spec_string ("config_file_path",
                                            "configf",
                                            "config file path",
                                            "itvencoder.conf",
                                            G_PARAM_WRITABLE | G_PARAM_READABLE);
        g_object_class_install_property (g_object_class, CONFIG_PROP_CONFIG_FILE_PATH, config_param);
}

static void
config_init (Config *config)
{
        config->config_file_path = NULL;
        config->itvencoder_config = NULL;
        config->channel_config_array = NULL;
}

static GObject *
config_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
config_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
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
config_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
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

static gint
config_load_config_file_func (Config *config)
{
        json_error_t error;
        gchar *channel_configs_pattern;
        glob_t channel_config_paths;
        ChannelConfig *channel_config;

        json_decref(config->itvencoder_config);
        config->itvencoder_config = json_load_file(config->config_file_path, 0, &error);
        if(!config->itvencoder_config) {
                GST_ERROR("%d: %s\n", error.line, error.text);
                return -1;
        }

        if (config->channel_config_array != NULL) {
                g_array_free (config->channel_config_array, FALSE);
                config->channel_config_array = NULL;
        }

        config->channel_config_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        channel_configs_pattern = (gchar *)json_string_value(json_object_get(config->itvencoder_config, "channel_configs"));
        if (glob (channel_configs_pattern, GLOB_TILDE, NULL, &channel_config_paths) != 0) {
                GST_ERROR ("Open channel config files failure.");
                return -2;
        }
        for (guint i=0; i<channel_config_paths.gl_pathc; i++) {
                GST_DEBUG ("Find channel config file: %s.", channel_config_paths.gl_pathv[i]);
                channel_config = (ChannelConfig *)g_malloc (sizeof(ChannelConfig));
                channel_config->config_path = g_strdup (channel_config_paths.gl_pathv[i]);
                channel_config->config = json_load_file(channel_config->config_path, 0, &error);
                if (!channel_config->config) { //TODO: error check, free allocated.
                        GST_ERROR ("%d: %s\n", error.line, error.text);
                        return -1;
                }
                g_array_append_val (config->channel_config_array, channel_config);
        }
        globfree (&channel_config_paths);

        return 0;
}

static gint
config_save_config_file_func (Config *config)
{
        json_dump_file(config->itvencoder_config, config->config_file_path, JSON_INDENT(4)); //TODO: error check
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
        type = g_type_register_static (G_TYPE_OBJECT, "Config", &info, 0);

        return type;
}

gint
config_load_config_file (Config *config)
{
        if (CONFIG_GET_CLASS(config)->config_load_config_file_func(config) == -1) {
                GST_ERROR ("load config file error\n");
                return -1;
        }

        return 0;
}

gint
config_save_config_file (Config *config)
{
        return CONFIG_GET_CLASS (config)->config_save_config_file_func (config);
}
