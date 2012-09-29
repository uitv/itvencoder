
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "config.h"
#include "jansson.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

int
main(int argc, char *argv[])
{
        Config *config;
        gchar *config_path;
        json_t *channel;

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        config = g_object_new(TYPE_CONFIG, "config_file_path", "itvencoder.conf", NULL);
        g_object_set(config, "config_file_path", "itvencoder.conf", NULL);
        g_object_get(config, "config_file_path", &config_path, NULL);
        GST_DEBUG("config file path is %s\n", config_path);

        config_load_config_file(config);
        channel = json_object_get(config->itvencoder_config, "channel_configs"); 
        GST_LOG("channel config files: %s\n", json_string_value(channel));
        config_save_config_file(config);

        return 0;
}
