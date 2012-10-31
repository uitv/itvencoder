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
static gchar* config_get_selected_pipeline_key (ChannelConfig *channel_config, gchar *pipeline);

static void
config_class_init (ConfigClass *configclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(configclass);
        GParamSpec *param;

        GST_LOG ("config class init");

        g_object_class->constructor = config_constructor;
        g_object_class->set_property = config_set_property;
        g_object_class->get_property = config_get_property;

        configclass->config_load_config_file_func = config_load_config_file_func;
        configclass->config_save_config_file_func = config_save_config_file_func;

        param = g_param_spec_string (
                "config_file_path",
                "configf",
                "config file path",
                "itvencoder.conf",
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, CONFIG_PROP_CONFIG_FILE_PATH, param);
}

static void
config_init (Config *config)
{
        GST_LOG ("config init");

        config->config_file_path = NULL;
        config->config = NULL;
        config->channel_config_array = g_array_new (FALSE, FALSE, sizeof(gpointer)); //TODO: free!
}

static GObject *
config_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        GST_LOG ("cofnig constructor");

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
config_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CONFIG(obj));

        GST_LOG ("config set property");

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

        GST_LOG ("config get property");

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
        json_t *j;
        json_error_t error;
        gchar *channel_configs_pattern;
        glob_t channel_config_paths;
        ChannelConfig *channel_config;
        guint i;

        GST_LOG ("config load config file func");

        json_decref(config->config); //TODO: should check
        config->config = json_load_file(config->config_file_path, 0, &error);
        if(!config->config) {
                GST_ERROR("%d: %s\n", error.line, error.text);
                return -1;
        }

        j = json_object_get(config->config, "channel_configs");
        if (j == NULL) {
                GST_ERROR ("parse itvencoder config file error : channel_configs");
                return -1;
        }
        channel_configs_pattern = (gchar *)json_string_value(j);
        if (glob (channel_configs_pattern, GLOB_TILDE, NULL, &channel_config_paths) != 0) {
                GST_ERROR ("Open channel config files failure.");
                g_free (channel_configs_pattern);
                return -2;
        }
        for (i=0; i<channel_config_paths.gl_pathc; i++) { // TODO: name unique identifier check.
                GST_DEBUG ("Find channel config file: %s.", channel_config_paths.gl_pathv[i]);
                channel_config = (ChannelConfig *)g_malloc (sizeof(ChannelConfig));
                channel_config->config_path = g_strdup (channel_config_paths.gl_pathv[i]);
                channel_config->config = json_load_file(channel_config->config_path, 0, &error);
                if (!channel_config->config) { //TODO: error check, free allocated.
                        GST_ERROR ("%d: %s\n", error.line, error.text);
                        return -1;
                }
                j = json_object_get (channel_config->config, "name");
                if (j == NULL) {
                        GST_ERROR ("parse channel config file error : name");
                        return -1;
                }
                channel_config->name = (gchar *)json_string_value(j);
                if (!channel_config->name) {
                        GST_ERROR ("channel name error");
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
        GST_LOG ("config save cofnig file func");

        json_dump_file(config->config, config->config_file_path, JSON_INDENT(4)); //TODO: error check
}

GType
config_get_type (void)
{
        static GType type = 0;

        GST_LOG ("cofnig get type");

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
        GST_LOG ("config load config file");

        if (CONFIG_GET_CLASS(config)->config_load_config_file_func(config) == -1) {
                GST_ERROR ("load config file error\n");
                return -1;
        }

        return 0;
}

gint
config_save_config_file (Config *config)
{
        GST_LOG ("config save config file");

        return CONFIG_GET_CLASS (config)->config_save_config_file_func (config);
}

static gchar*
config_get_selected_pipeline_key (ChannelConfig *channel_config, gchar *pipeline)
{
        json_t *j1, *j2, *j3;
        gchar *key, *selected_pipeline_key;
        guint i;

        GST_LOG ("config get selected pipeline");

        j1 = json_object_get (channel_config->config, pipeline);
        if (j1 == NULL) {
                GST_ERROR ("parse channel config file error: %s", channel_config->name);
                return NULL;
        }
        j2 = json_object_get (j1, "selectable-keys");
        if (j2 == NULL) {
                GST_ERROR ("parse channel config file error: %s", channel_config->name);
                return NULL;
        }
        key = (gchar *)json_string_value (json_array_get (j2, 0));
        j3 = json_object_get (j1, key);
        if (j3 == NULL) {
                GST_ERROR ("parse channel config file error : %s", key);
                return NULL;
        }
        key = g_strdup ((gchar *)json_string_value (j3));
        for (i = 1; i < json_array_size (j2); i++) {
                selected_pipeline_key = (gchar *)json_string_value (json_array_get (j2, i));
                j3 = json_object_get (j1, selected_pipeline_key);
                if (j3 == NULL) {
                        GST_ERROR ("parse channel config file error : %s", selected_pipeline_key);
                        return NULL;
                }
                selected_pipeline_key = (gchar *)json_string_value (j3);
                selected_pipeline_key = g_strconcat (key, "-", selected_pipeline_key);
                g_free (key);
                key = selected_pipeline_key;
        }

        return selected_pipeline_key;
}

gchar*
config_get_pipeline_string (ChannelConfig *channel_config, gchar *pipeline)
{
        json_t *j, *selected_pipeline;
        gchar *selected_pipeline_key;
        gchar *t1, *t2;
        GRegex *regex;
        GMatchInfo *match_info;

        GST_LOG ("config get decoder pipeline string");

        j = json_object_get (channel_config->config, pipeline);
        if (j == NULL) {
                GST_INFO ("No %s in %s", pipeline, channel_config->config_path);
                return NULL;
        }
        selected_pipeline_key = config_get_selected_pipeline_key (channel_config, pipeline);
        /* Got the selected pipeline, e.g mpeg2-mp3 */
        selected_pipeline = json_object_get (j, selected_pipeline_key);
        if (selected_pipeline == NULL) {
                GST_ERROR ("parse selected pipeline object error: %s", channel_config->name);
                return NULL;
        }
        j = json_object_get (selected_pipeline, "pipeline-template");
        if (j == NULL) {
                GST_ERROR ("parse selected pipeline template error: %s", channel_config->name);
                return NULL;
        }
        t1 = (gchar *)json_string_value (j);
        t2 = g_strdup (t1);
        regex = g_regex_new ("<%(?<para>[^<%]*)%>", G_REGEX_OPTIMIZE, 0, NULL);
        if (regex == NULL) {
                GST_ERROR ("bad regular expression");
                return NULL;
        }
        g_regex_match (regex, t1, 0, &match_info);
        g_regex_unref (regex);

        while (g_match_info_matches (match_info)) {
                gchar *key = g_match_info_fetch_named (match_info, "para");
                gchar *regex_str = g_strdup_printf ("<%c%s%c>", '%', key, '%');
                regex = g_regex_new (regex_str, G_REGEX_OPTIMIZE, 0, NULL);
                json_t *value = json_object_get(selected_pipeline, key);
                if (value == NULL) {
                        GST_ERROR ("parse channel config error : %s?", key);
                        g_regex_unref (regex);
                        g_match_info_free (match_info);
                        g_free (key);
                        g_free (selected_pipeline_key);
                        return NULL;
                }
                if (json_is_string (value)) {
                        gchar *v = (gchar *)json_string_value (value);
                        gchar *t = t2;
                        t2 = g_regex_replace (regex, t, -1, 0, v, 0, NULL);
                        g_free (t);
                } else if (json_is_integer (value)) {
                        gchar *v = g_strdup_printf ("%i", json_integer_value (value));
                        gchar *t = t2;
                        t2 = g_regex_replace (regex, t, -1, 0, v, 0, NULL);
                        g_free (t);
                } else {
                        GST_ERROR ("unsupported type of channel configuration - %s", key);
                }
                g_free (key);
                g_regex_unref (regex);
                g_match_info_next (match_info, NULL);
        }

        g_match_info_free (match_info);
        g_free (selected_pipeline_key);

        return t2;
}

