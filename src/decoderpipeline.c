/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <gst/gst.h>
#include "decoderpipeline.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

static void decoder_pipeline_class_init (DecoderPipelineClass *decoderpipelineclass);
static void decoder_pipeline_init (DecoderPipeline *decoderpipeline);

static void
decoder_pipeline_class_init (DecoderPipelineClass *decoderpipelineclass)
{
}

static void
decoder_pipeline_init (DecoderPipeline *decoderpipeline)
{
        GST_LOG ("decoder pipeline init");
}

GType
decoder_pipeline_get_type (void)
{
        static GType type = 0;

        GST_LOG ("decoder pipeline get type");

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (DecoderPipelineClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)decoder_pipeline_class_init,
                NULL,
                NULL,
                sizeof (DecoderPipeline),
                0,
                (GInstanceInitFunc)decoder_pipeline_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "DecoderPipeline", &info, 0);

        return type;
}

