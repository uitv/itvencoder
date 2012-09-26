
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <glib-object.h>
#include "decoderpipeline.h"

static void decoder_pipeline_class_init(DecoderPipelineClass *decoderpipelineclass);
static void decoder_pipeline_init(DecoderPipeline *decoderpipeline);

static void
decoder_pipeline_class_init(DecoderPipelineClass *decoderpipelineclass)
{
}

static void
decoder_pipeline_init(DecoderPipeline *decoderpipeline)
{
}

GType
decoder_pipeline_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (DecoderPipelineClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)decoder_pipeline_class_init,
                NULL,
                NULL,
                sizeof(DecoderPipeline),
                0,
                (GInstanceInitFunc)decoder_pipeline_init,
                NULL
        };
        type = g_type_register_static(G_TYPE_OBJECT, "DecoderPipeline", &info, 0);

        return type;
}

