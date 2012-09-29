#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "decoderpipeline.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

int
main(int argc, char *argv[])
{
        DecoderPipeline *decoderpipeline;

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        decoderpipeline = g_object_new(TYPE_DECODER_PIPELINE, 0, NULL);

        return 0;
}
