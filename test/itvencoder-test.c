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
        GST_DEBUG("%lld\n", itvencoder_get_start_time(itvencoder));
        GST_LOG("%lld\n", itvencoder_get_start_time(itvencoder));
        return 0;
}
