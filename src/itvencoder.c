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
        ITVENCODER_PROP_CONFIGURE,
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
                "configure",
                "Configure",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ITVENCODER_PROP_CONFIGURE, param);
}

static void
itvencoder_init (ITVEncoder *itvencoder)
{
        gchar *stat, **stats, **cpustats;
        gsize *length;
        gint i;

        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
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
        case ITVENCODER_PROP_CONFIGURE:
                ITVENCODER(obj)->configure = (GstStructure *)g_value_get_pointer (value);
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
        case ITVENCODER_PROP_CONFIGURE:
                g_value_set_pointer (value, itvencoder->configure);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

/*
 * itvencoder_channel_initialize
 *
 * @itvencoder: itvencoder object.
 * @name: the name of the channel to be initialize.
 *
 * Returns: TRUE on success, FALSE on failure.
 *
 */
gboolean
itvencoder_channel_initialize (ITVEncoder *itvencoder)
{
        GValue *value;
        GstStructure *structure1, *structure2;
        gint i, n;
        gchar *name;
        Channel *channel;

        value = (GValue *)gst_structure_get_value (itvencoder->configure, "channel");
        structure1 = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure1);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure1, i);
                GST_INFO ("Channel found: %s.", name);
                channel = channel_new ("name", name, NULL);
                channel->id = i;
                value = (GValue *)gst_structure_get_value (structure1, name);
                structure2 = (GstStructure *)gst_value_get_structure (value);
                if (!channel_initialize (channel, structure2)) {
                        GST_ERROR ("Initialize channel error.");
                        return FALSE;
                }
                g_array_append_val (itvencoder->channel_array, channel);
                GST_INFO ("Channel %s added.", name);
        }

        return TRUE;
}

gboolean
itvencoder_channel_start (ITVEncoder *itvencoder)
{
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
                        if (g_strcmp0 (gst_caps_to_string (GST_BUFFER_CAPS (source_stream->ring[0])), "private/x-dvbsub") == 0) {
                                /* don't check subtitle */
                                continue;
                        }
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
                        if (g_strcmp0 (gst_caps_to_string (GST_BUFFER_CAPS (source_stream->ring[0])), "private/x-dvbsub") == 0) {
                                /* don't check subtitle */
                                continue;
                        }
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
        gint i;
        Channel *channel;
        GstClockID id;
        GstClockTime t;
        GstClockReturn ret;
        GValue *value;
        GstStructure *structure;
        gchar *p, **pp;
        gint port;

        for (i = 0; i < itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                channel_start (channel);
        }

        /* start http streaming */
        value = (GValue *)gst_structure_get_value (itvencoder->configure, "server");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "httpstreaming");
        p = (gchar *)g_value_get_string (value);
        pp = g_strsplit (p, ":", 0);
        port = atoi (pp[1]);
        g_strfreev (pp);
        itvencoder->httpstreaming = httpstreaming_new ("channels", itvencoder->channel_array, "system_clock", itvencoder->system_clock, NULL);
        httpstreaming_start (itvencoder->httpstreaming, 10, port);

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
