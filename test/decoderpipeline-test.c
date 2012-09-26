#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "decoderpipeline.h"

int
main()
{
        DecoderPipeline *decoderpipeline;

        g_type_init();
        decoderpipeline = g_object_new(TYPE_DECODER_PIPELINE, NULL);
        return 0;
}
