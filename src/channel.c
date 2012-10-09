/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>

#include "jansson.h"
#include "channel.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CHANNEL_PROP_0,
        CHANNEL_PROP_NAME,
        CHANNEL_PROP_CONFIG,
};

static void channel_class_init (ChannelClass *channelclass);
static void channel_init (Channel *channel);
static GObject *channel_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static gint channel_parse_config_func (Channel *channel);

static void
channel_class_init (ChannelClass *channelclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS(channelclass);
        GParamSpec *config_param;

        GST_LOG ("channel class init.");

        g_object_class->constructor = channel_constructor;
        g_object_class->set_property = channel_set_property;
        g_object_class->get_property = channel_get_property;

        channelclass->channel_parse_config_func = channel_parse_config_func;

        config_param = g_param_spec_string ("name",
                                            "name",
                                            "name of channel",
                                            "",
                                            G_PARAM_WRITABLE | G_PARAM_READABLE);
        g_object_class_install_property (g_object_class, CHANNEL_PROP_NAME, config_param);

        config_param = g_param_spec_pointer ("config",
                                             "config",
                                             "json_t type point of config",
                                             G_PARAM_WRITABLE | G_PARAM_READABLE);
        g_object_class_install_property (g_object_class, CHANNEL_PROP_CONFIG, config_param);
}

static void
channel_init (Channel *channel)
{
        GST_LOG ("channel object init");
}

static GObject *
channel_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        GST_LOG ("channel constructor");

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
channel_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_CHANNEL(obj));

        GST_LOG ("channel set property");

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                CHANNEL(obj)->name = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_file_path?
                break;
        case CHANNEL_PROP_CONFIG:
                CHANNEL(obj)->config = (json_t *)g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
channel_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Channel *channel = CHANNEL(obj);

        GST_LOG ("channel get property");

        switch(prop_id) {
        case CHANNEL_PROP_NAME:
                g_value_set_string (value, channel->name);
                break;
        case CHANNEL_PROP_CONFIG:
                g_value_set_pointer (value, channel->config);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static gint
channel_parse_config_func (Channel *channel)
{
        GST_LOG ("channel parse config func");

        json_t *decoder_pipeline = json_object_get (channel->config, "decoder-pipeline");
        if (!json_is_object (decoder_pipeline)) {
                GST_ERROR ("parse channel config file error: %s", channel->name);
                return -1;
        }
        GRegex *regex = g_regex_new("<%[^<%]*%>", G_REGEX_OPTIMIZE, 0, NULL);
        if (regex == NULL) {
                GST_WARNING ("bad regular expression\n");
                return 1;
        }
        gchar *template = (gchar *)json_string_value (json_object_get(decoder_pipeline, "decoder-pipeline-template"));
        channel->decoder_pipeline_fmt = g_regex_replace (regex, template, -1, 0, "%s", 0, NULL);

        return 0;
}

GType
channel_get_type (void)
{
        static GType type = 0;

        GST_LOG ("channel get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ChannelClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)channel_class_init,
                NULL,
                NULL,
                sizeof (Channel),
                0,
                (GInstanceInitFunc)channel_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Channel", &info, 0);

        return type;
}

gint
channel_parse_config (Channel *channel)
{
        GST_LOG ("channel parse config");

        if (CHANNEL_GET_CLASS(channel)->channel_parse_config_func(channel) != 0) {
                GST_ERROR ("channel parse config error\n");
                return -1;
        }

        return 0;
}
