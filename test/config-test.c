
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "config.h"

int
main()
{
        Config *config;
        gchar *config_path;

        g_type_init();
        config = g_object_new(TYPE_CONFIG, "config_file_path", "itvencoder.conf", NULL);
        //g_object_set(config, "config_file_path", "itvencoder.conf", NULL);
        g_object_get(config, "config_file_path", &config_path, NULL);
        g_print("config file path is %s\n", config_path);

        return 0;
}
