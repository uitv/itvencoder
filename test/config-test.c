
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "config.h"
#include "jansson.h"

int
main()
{
        Config *config;
        gchar *config_path;
        json_t *channel;

        g_type_init();
        config = g_object_new(TYPE_CONFIG, "config_file_path", "itvencoder.conf", NULL);
        g_object_set(config, "config_file_path", "itvencoder.conf", NULL);
        g_object_get(config, "config_file_path", &config_path, NULL);
        g_print("config file path is %s\n", config_path);
        config_load_config_file(config);
        channel = json_object_get(config->itvencoder_config, "channel_configs"); 
        g_print("channel config files: %s\n", json_string_value(channel));
        config_save_config_file(config);

        return 0;
}
