/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include "itvencoder.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

static void itvencoder_class_init (ITVEncoderClass *itvencoderclass);
static void itvencoder_init (ITVEncoder *itvencoder);
static GTimeVal itvencoder_get_start_time_func (ITVEncoder *itvencoder);

static void
itvencoder_class_init (ITVEncoderClass *itvencoderclass)
{
}

static void
itvencoder_init (ITVEncoder *itvencoder)
{
        GST_LOG ("itvencoder_init");

        g_get_current_time (&itvencoder->start_time);

        // load config
        itvencoder->config = config_new ("config_file_path", "itvencoder.conf", NULL);
        config_load_config_file(itvencoder->config);

        // initialize channels
        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        for (guint i=0; i<itvencoder->config->channel_config_array->len; i++) {
                ChannelConfig *channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                gchar *channel_name = (gchar *)json_string_value (json_object_get(channel_config->config, "name"));
                Channel *channel = channel_new ("name", channel_name, "config", channel_config->config, NULL);
                channel_parse_config (channel);
                GST_DEBUG ("parse channel %s, name is %s", channel_config->config_path, channel_name);
                g_array_append_val (itvencoder->channel_array, channel);
        }
}

GType
itvencoder_get_type (void)
{
        static GType type = 0;

        GST_LOG ("itvencoder get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ITVEncoderClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc) itvencoder_class_init,
                NULL,
                NULL,
                sizeof (ITVEncoder),
                0,
                (GInstanceInitFunc) itvencoder_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "ITVEncoder", &info, 0);

        return type;
}

GTimeVal
itvencoder_get_start_time (ITVEncoder *itvencoder)
{
        GST_LOG ("itvencoder get start time");

        GTimeVal invalid_time = {0,0};
        g_return_val_if_fail (IS_ITVENCODER (itvencoder), invalid_time);

        return itvencoder->start_time;
}
