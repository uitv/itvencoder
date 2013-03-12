/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <gst/gst.h>
#include <string.h>
#include "itvencoder.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        ITVENCODER_PROP_0,
        ITVENCODER_PROP_CONFIG,
};

static void itvencoder_class_init (ITVEncoderClass *itvencoderclass);
static void itvencoder_init (ITVEncoder *itvencoder);
static GObject *itvencoder_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void itvencoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void itvencoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static GTimeVal itvencoder_get_start_time_func (ITVEncoder *itvencoder);
static void itvencoder_initialize_channels (ITVEncoder *itvencoder);
static gboolean itvencoder_channel_monitor (GstClock *clock, GstClockTime time, GstClockID id, gpointer user_data);
static void stat_report (ITVEncoder *itvencoder);

static void
itvencoder_class_init (ITVEncoderClass *itvencoderclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (itvencoderclass);
        GParamSpec *param;

        g_object_class->constructor = itvencoder_constructor;
        g_object_class->set_property = itvencoder_set_property;
        g_object_class->get_property = itvencoder_get_property;

        param = g_param_spec_pointer (
                "config",
                "Config",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ITVENCODER_PROP_CONFIG, param);
}

static void
itvencoder_init (ITVEncoder *itvencoder)
{
        gchar *stat, **stats, **cpustats;
        gsize *length;
        gint i;

        itvencoder->system_clock = gst_system_clock_obtain ();
        g_object_set (itvencoder->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        itvencoder->start_time = gst_clock_get_time (itvencoder->system_clock);
        g_file_get_contents ("/proc/stat", &stat, length, NULL);
        stats = g_strsplit (stat, "\n", 10);
        cpustats = g_strsplit (stats[0], " ", 10);
        itvencoder->start_ctime = 0;
        for (i = 1; i < 8; i++) {
                itvencoder->start_ctime += g_ascii_strtoull (cpustats[i], NULL, 10);
        }
}

static GObject *
itvencoder_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
itvencoder_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_ITVENCODER(obj));

        switch(prop_id) {
        case ITVENCODER_PROP_CONFIG:
                ITVENCODER(obj)->config = (Config *)g_value_get_pointer (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
itvencoder_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        ITVEncoder  *itvencoder = ITVENCODER(obj);

        switch(prop_id) {
        case ITVENCODER_PROP_CONFIG:
                g_value_set_pointer (value, itvencoder->config);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
itvencoder_initialize_channels (ITVEncoder *itvencoder)
{
        ChannelConfig *channel_config;
        Channel *channel;
        gchar *pipeline_string;
        guint i;
        gchar *name;

        // initialize channels
        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                name = g_strdup_printf ("channel-%d", i);
                channel = channel_new ("name", name, NULL);
                channel->id = i;
                g_free (name);
                pipeline_string = config_get_pipeline_string (channel_config, "decoder-pipeline");
                if (pipeline_string == NULL) {
                        GST_ERROR ("no source pipeline string error");
                        exit (0); //TODO : exit or return?
                }
                if (channel_set_source (channel, pipeline_string) != 0) {
                        GST_ERROR ("Set source pipeline error.");
                        exit (0);
                }
                for (;;) {
                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-1");
                        if (pipeline_string == NULL) {
                                GST_ERROR ("One encoder pipeline is must");
                                exit (0); //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 1 error");
                                exit (0);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-2");
                        if (pipeline_string == NULL) {
                                GST_INFO ("One encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 2 error");
                                exit (0);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-3");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Two encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 3 error");
                                exit (0);
                        }

                        pipeline_string = config_get_pipeline_string (channel_config, "encoder-pipeline-4");
                        if (pipeline_string == NULL) {
                                GST_INFO ("Three encoder pipelines found.");
                                break; //TODO : exit or return?
                        }
                        if (channel_add_encoder (channel, pipeline_string) != 0) {
                                GST_ERROR ("Add encoder pipeline 4 error");
                                exit (0);
                        }

                        GST_INFO ("Four encoder pipelines found.");
                        break;
                }
                GST_INFO ("parse channel %s, encoder channel number %d.",
                           channel_config->config_path,
                           channel->encoder_array->len);
                g_array_append_val (itvencoder->channel_array, channel);
        }
}

GType
itvencoder_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ITVEncoderClass), // class size.
                NULL, // base initializer
                NULL, // base finalizer
                (GClassInitFunc) itvencoder_class_init, // class init.
                NULL, // class finalize.
                NULL, // class data.
                sizeof (ITVEncoder),
                0, // instance size.
                (GInstanceInitFunc) itvencoder_init, // instance init.
                NULL // value table.
        };
        type = g_type_register_static (G_TYPE_OBJECT, "ITVEncoder", &info, 0);

        return type;
}

static gboolean
itvencoder_channel_monitor (GstClock *clock, GstClockTime time, GstClockID id, gpointer user_data)
{
        GstClockID nextid;
        GstClockTime now, min, max;
        GstClockTimeDiff time_diff;
        GstClockReturn ret;
        ITVEncoder *itvencoder = (ITVEncoder *)user_data;
        Channel *channel;
        SourceStream *source_stream;
        EncoderStream *encoder_stream;
        gint i, j;

        for (i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                if (channel->source->state != GST_STATE_PLAYING) {
                        continue;
                }

                /* source heartbeat check */
                for (i = 0; i < channel->source->streams->len; i++) {
                        source_stream = g_array_index (channel->source->streams, gpointer, i);
                        now = gst_clock_get_time (itvencoder->system_clock);
                        time_diff = GST_CLOCK_DIFF (source_stream->last_heartbeat, now);
                        if (time_diff > HEARTBEAT_THRESHHOLD) {
                                GST_ERROR ("channel %s stream %s heart beat error %lld, restart channel %s",
                                        channel->name,
                                        source_stream->name,
                                        time_diff,
                                        channel->name);
                                if (g_mutex_trylock (channel->operate_mutex)) {
                                        channel_restart (channel);
                                        g_mutex_unlock (channel->operate_mutex);
                                } else {
                                        GST_WARNING ("Try lock channel %s to restart failure!", channel->name);
                                }
                        } else {
                                GST_INFO ("channel %s stream %s heart beat %" GST_TIME_FORMAT,
                                        channel->name,
                                        source_stream->name,
                                        GST_TIME_ARGS (source_stream->last_heartbeat));
                        }
                }

                /* encoder heartbeat check */
                for (i = 0; i < channel->encoder_array->len; i++) {
                        Encoder *encoder = g_array_index (channel->encoder_array, gpointer, i);
                        if (encoder->state != GST_STATE_PLAYING) {
                                continue;
                        }

                        for (j = 0; j < encoder->streams->len; j++) {
                                encoder_stream = g_array_index (encoder->streams, gpointer, j);
                                now = gst_clock_get_time (itvencoder->system_clock);
                                time_diff = GST_CLOCK_DIFF (encoder_stream->last_heartbeat, now);
                                if (time_diff > HEARTBEAT_THRESHHOLD) {
                                        GST_ERROR ("endcoder %s stream %s heart beat error %lld, restart",
                                                encoder->name,
                                                encoder_stream->name,
                                                time_diff);
                                        channel_encoder_restart (encoder);
                                } else {
                                        GST_INFO ("channel %s encoder stream %s heart beat %" GST_TIME_FORMAT,
                                                channel->name,
                                                encoder_stream->name,
                                                GST_TIME_ARGS (encoder_stream->last_heartbeat));
                                }

                                time_diff = GST_CLOCK_DIFF (GST_BUFFER_TIMESTAMP (encoder_stream->source->ring[encoder_stream->current_position]),
                                        GST_BUFFER_TIMESTAMP (encoder_stream->source->ring[encoder_stream->source->current_position]));
                                if (time_diff > GST_SECOND) {
                                        GST_WARNING ("channel %s encoder stream %s delay %" GST_TIME_FORMAT,
                                                channel->name,
                                                encoder_stream->name,
                                                GST_TIME_ARGS (time_diff));
                                }
                        }
                }

                /* sync check */
                min = GST_CLOCK_TIME_NONE;
                max = 0;
                for (i = 0; i < channel->source->streams->len; i++) {
                        source_stream = g_array_index (channel->source->streams, gpointer, i);
                        if (min > source_stream->current_timestamp) {
                                min = source_stream->current_timestamp;
                        }
                        if (max < source_stream->current_timestamp) {
                                max = source_stream->current_timestamp;
                        }
                }
                time_diff = GST_CLOCK_DIFF (min, max);
                if (time_diff > SYNC_THRESHHOLD) {
                        GST_ERROR ("channel %s sync error %lld", channel->name, time_diff);
                        channel->source->sync_error_times += 1;
                        if (channel->source->sync_error_times == 3) {
                                GST_ERROR ("sync error times %d, restart channel %s", channel->source->sync_error_times, channel->name);
                                if (g_mutex_trylock (channel->operate_mutex)) {
                                        channel_restart (channel);
                                        channel->source->sync_error_times = 0;
                                        g_mutex_unlock (channel->operate_mutex);
                                } else {
                                        GST_WARNING ("Try lock channel %s restart lock failure!", channel->name);
                                }
                        }
                } else {
                        channel->source->sync_error_times = 0;
                        for (i = 0; i < channel->source->streams->len; i++) {
                                source_stream = g_array_index (channel->source->streams, gpointer, i);
                                GST_INFO ("channel %s stream %s timestamp %" GST_TIME_FORMAT,
                                        channel->name,
                                        source_stream->name,
                                        GST_TIME_ARGS (source_stream->current_timestamp));
                        }
                }
        }

        httpserver_report_request_data (itvencoder->httpstreaming->httpserver);

        stat_report (itvencoder);

        now = gst_clock_get_time (itvencoder->system_clock);
        nextid = gst_clock_new_single_shot_id (itvencoder->system_clock, now + 2000 * GST_MSECOND); // FIXME: id should be released
        ret = gst_clock_id_wait_async (nextid, itvencoder_channel_monitor, itvencoder);
        gst_clock_id_unref (nextid);
        if (ret != GST_CLOCK_OK) {
                GST_WARNING ("Register itvencoder monitor failure");
                return FALSE;
        }

        return TRUE;
}

GstClockTime
itvencoder_get_start_time (ITVEncoder *itvencoder)
{
        return itvencoder->start_time;
}

gint
itvencoder_start (ITVEncoder *itvencoder)
{
        gint i, j;
        Channel *channel;
        GstClockID id;
        GstClockTime t;
        GstClockReturn ret;

        itvencoder_initialize_channels (itvencoder);

        /* start encoder */
        for (i=0; i<itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                GST_INFO ("channel %s has %d encoder pipeline. channel source pipeline string is %s",
                        channel->name,
                        channel->encoder_array->len,
                        channel->source->pipeline_string);
                if (channel_source_start (channel->source) !=0 ) {
                        GST_ERROR ("Fatal error! exit");
                        exit (0);
                }
                for (j=0; j<channel->encoder_array->len; j++) {
                        Encoder *encoder = g_array_index (channel->encoder_array, gpointer, j);
                        GST_INFO ("channel encoder pipeline string is %s", encoder->pipeline_string);
                        if (channel_encoder_start (encoder) != 0) {
                                GST_ERROR ("Fatal error! exit");
                                exit (0);
                        }
                }
        }

        /* start http streaming */
        itvencoder->httpstreaming = httpstreaming_new ("channels", itvencoder->channel_array, "system_clock", itvencoder->system_clock, NULL);
        httpstreaming_start (itvencoder->httpstreaming, 10, itvencoder->config->http_streaming_port);

        /* regist itvencoder monitor */
        t = gst_clock_get_time (itvencoder->system_clock)  + 5000 * GST_MSECOND;
        id = gst_clock_new_single_shot_id (itvencoder->system_clock, t); 
        ret = gst_clock_id_wait_async (id, itvencoder_channel_monitor, itvencoder);
        gst_clock_id_unref (id);
        if (ret != GST_CLOCK_OK) {
                GST_WARNING ("Regist itvencoder monitor failure");
                exit (0);
        }

        return 0;
}

static void
stat_report (ITVEncoder *itvencoder)
{
        gchar *stat_file, *stat, **stats, **cpustats;
        gsize *length;
        guint64 utime, stime, ctime; // process user time, process system time, total cpu time
        guint64 rss; // Resident Set Size.
        gint i;

        stat_file = g_strdup_printf ("/proc/%d/stat", getpid ());
        g_file_get_contents (stat_file, &stat, length, NULL);
        stats = g_strsplit (stat, " ", 44);
        utime = g_ascii_strtoull (stats[13],  NULL, 10); // seconds
        stime = g_ascii_strtoull (stats[14], NULL, 10);
        rss = g_ascii_strtoull (stats[23], NULL, 10) * sysconf (_SC_PAGESIZE);
        g_free (stat_file);
        g_free (stat);
        g_strfreev (stats);
        g_file_get_contents ("/proc/stat", &stat, length, NULL);
        stats = g_strsplit (stat, "\n", 10);
        cpustats = g_strsplit (stats[0], " ", 10);
        ctime = 0;
        for (i = 1; i < 8; i++) {
                ctime += g_ascii_strtoull (cpustats[i], NULL, 10);
        }
        g_free (stat);
        g_strfreev (stats);
        g_strfreev (cpustats);
        GST_INFO ("average cpu: %d%%, cpu: %d%%, rss: %lluMB",
                ((utime + stime) * 100) / (ctime - itvencoder->start_ctime),
                ((utime - itvencoder->last_utime + stime - itvencoder->last_stime) * 100) / (ctime - itvencoder->last_ctime),
                rss/1000000);
        itvencoder->last_ctime = ctime;
        itvencoder->last_utime = utime;
        itvencoder->last_stime = stime;
}
