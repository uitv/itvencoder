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

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        itvencoder = itvencoder_new (0, NULL);
        GST_INFO ("start time : %lld", itvencoder_get_start_time(itvencoder));
        GST_INFO ("listening ports : %d", json_integer_value(json_object_get(itvencoder->config->config, "listening_ports")));
        GST_INFO ("channel configs : %s", json_string_value(json_object_get(itvencoder->config->config, "channel_configs")));
        GST_INFO ("log directory : %s", json_string_value(json_object_get(itvencoder->config->config, "log_directory")));
        for (guint i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                GST_INFO ("config file %d - %s", i, channel_config->config_path);
                GST_INFO ("channel name - %s", channel_config->name);
                GST_INFO ("name - %s", json_string_value (json_object_get(channel_config->config, "name")));
        }

        GST_INFO ("\nChannel description ----------------------------");
        for (guint i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                GST_INFO ("\nchannel %s has %d encoder pipeline.>>>>>>>>>>>>>>>>>>>>>>>>>\nchannel decoder pipeline string is %s",
                           channel->name,
                           channel->encoder_pipeline_array->len,
                           channel->decoder_pipeline->pipeline_string);
                for (guint j=0; j<channel->encoder_pipeline_array->len; j++) {
                        EncoderPipeline *encoder_pipeline = g_array_index (channel->encoder_pipeline_array, gpointer, j);
                        GST_INFO ("\nchannel encoder pipeline string is %s", encoder_pipeline->pipeline_string); 
                }
        }

        return 0;
}
