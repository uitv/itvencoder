#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "itvencoder.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

int
main(int argc, char *argv[])
{
        ITVEncoder *itvencoder;
        ChannelConfig *channel_config;
        Channel *channel;
        gchar *name;
        json_t *config;

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        itvencoder = g_object_new(TYPE_ITVENCODER, 0, NULL);
        GST_DEBUG("%lld", itvencoder_get_start_time(itvencoder));
        GST_LOG("%lld", itvencoder_get_start_time(itvencoder));
        GST_INFO ("%d", json_integer_value(json_object_get(itvencoder->config->config, "listening_ports")));
        GST_INFO ("%s", json_string_value(json_object_get(itvencoder->config->config, "channel_configs")));
        GST_INFO ("%s", json_string_value(json_object_get(itvencoder->config->config, "log_directory")));
        for (guint i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                GST_DEBUG ("config file %d - %s", i, channel_config->config_path);
                GST_DEBUG ("name - %s", json_string_value (json_object_get(channel_config->config, "name")));
        }

        for (guint i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                GST_DEBUG (">>>find channel, name is %s, decoder pipeline: %s", channel->name, channel->decoder_pipeline_fmt);
                GST_DEBUG (">>>decoderpipeline template - %s", json_string_value (json_object_get(channel->config, "name")));
        }

        GST_DEBUG (">>>name - %s", json_string_value (json_object_get(channel_config->config, "name")));
        channel = g_object_new (TYPE_CHANNEL, "name", "cctv-99", "config", channel_config->config, NULL);
        g_object_get (channel, "name", &name, NULL);
        GST_DEBUG ("channel name is %s", name);
        g_object_get (channel, "config", &config, NULL);
        GST_DEBUG ("channel name - %s", json_string_value (json_object_get(config, "name")));

        return 0;
}
