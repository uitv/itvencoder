#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "itvencoder.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

int
main(int argc, char *argv[])
{
        guint major, minor, micro, nano;
        const gchar *nano_str;
        ITVEncoder *itvencoder;
        ChannelConfig *channel_config;
        Channel *channel;
        gchar *name;
        GMainLoop *loop;

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        gst_version (&major, &minor, &micro, &nano);
        if (nano == 1)
                nano_str = "(git)";
        else if (nano == 2)
                nano_str = "(Prerelease)";
        else
                nano_str = "";
        GST_INFO ("gstreamer version : %d.%d.%d %s", major, minor, micro, nano_str);

        loop = g_main_loop_new (NULL, FALSE);
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
        
        channel_set_decoder_pipeline_state (channel, GST_STATE_PLAYING);
        channel_set_encoder_pipeline_state (channel, 0, GST_STATE_PLAYING);

        GST_INFO ("\nChannel %s Running................................................", channel->name);
        g_main_loop_run (loop);
        return 0;
}
