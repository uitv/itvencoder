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

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        itvencoder = g_object_new(TYPE_ITVENCODER, 0, NULL);
        GST_DEBUG("%lld", itvencoder_get_start_time(itvencoder));
        GST_LOG("%lld", itvencoder_get_start_time(itvencoder));
        GST_INFO ("%d", json_integer_value(json_object_get(itvencoder->config->itvencoder_config, "listening_ports")));
        GST_INFO ("%s", json_string_value(json_object_get(itvencoder->config->itvencoder_config, "channel_configs")));
        GST_INFO ("%s", json_string_value(json_object_get(itvencoder->config->itvencoder_config, "log_directory")));
        return 0;
}
