
/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#ifndef __DECODER_PIPELINE_H__
#define __DECODER_PIPELINE_H__

#include <gst/gst.h>

typedef struct _DecoderPipeline      DecoderPipeline;
typedef struct _DecoderPipelineClass DecoderPipelineClass;

struct _DecoderPipeline {
        GObject         parent;

        GstElement      *pipeline;
        gchar           *uri;
        guint           program_number;
};

struct _DecoderPipelineClass {
        GObjectClass    parent;
};

#define TYPE_DECODER_PIPELINE           (decoder_pipeline_get_type())
#define DECODER_PIPELINE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DECODER_PIPELINE, DecoderPipeline))
#define DECODER_PIPELINE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST    ((cls), TYPE_DECODER_PIPELINE, DecoderPipelineClass))
#define IS_DECODER_PIPELINE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DECODER_PIPELINE))
#define IS_DECODER_PIPELINE_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE    ((cls), TYPE_DECODER_PIPELINE))
#define DECODER_PIPELINE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS  ((obj), TYPE_DECODER_PIPELINE, DecoderPipelineClass))
#define decoder_pipeline_new(...)       (g_object_new(TYPE_DECODER_PIPELINE, ## __VA_ARGS__, NULL))

GType decoder_pipeline_get_type (void);

#endif /* __DECODER_PIPELINE_H__ */
