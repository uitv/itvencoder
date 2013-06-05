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
        ITVENCODER_PROP_DAEMON,
        ITVENCODER_PROP_CONFIGURE_FILE,
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

        param = g_param_spec_boolean (
                "daemon",
                "daemon",
                "run in background",
                TRUE,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ITVENCODER_PROP_DAEMON, param);

        param = g_param_spec_string (
                "configure",
                "configuref",
                "configure file path",
                "itvencoder.conf",
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, ITVENCODER_PROP_CONFIGURE_FILE, param);
}

static void
itvencoder_init (ITVEncoder *itvencoder)
{
        gchar *stat, **stats, **cpustats;
        gsize *length;
        gint i;

        itvencoder->configure = NULL;
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
        case ITVENCODER_PROP_DAEMON:
                ITVENCODER(obj)->daemon = g_value_get_boolean (value);
                break;
        case ITVENCODER_PROP_CONFIGURE_FILE:
                ITVENCODER(obj)->configure_file = (gchar *)g_value_dup_string (value);
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
        case ITVENCODER_PROP_DAEMON:
                g_value_set_boolean (value, itvencoder->daemon);
                break;
        case ITVENCODER_PROP_CONFIGURE_FILE:
                g_value_set_string(value, itvencoder->configure_file);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
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

static void
load_configure (ITVEncoder *itvencoder)
{
        if (itvencoder->configure != NULL) {
                gst_object_unref (G_OBJECT (itvencoder->configure));
        }

        itvencoder->configure = configure_new ("configure_path", itvencoder->configure_file, NULL);
        configure_load_from_file (itvencoder->configure);
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
        GstStructure *structure, *configure;
        gint i, n;
        gchar *name;
        Channel *channel;

        load_configure (itvencoder);
        value = (GValue *)gst_structure_get_value (itvencoder->configure->data, "channels");
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                value = (GValue *)gst_structure_get_value (structure, name);
                configure = (GstStructure *)gst_value_get_structure (value);
                channel = channel_new ("name", name, "configure", configure, NULL);
                channel->id = i;
                g_array_append_val (itvencoder->channel_array, channel);
                GST_INFO ("Channel %s added.", name);
        }

        return TRUE;
}

gboolean
itvencoder_channel_start (ITVEncoder *itvencoder, gint index)
{
        Channel *channel;

        channel = g_array_index (itvencoder->channel_array, gpointer, index);
        return channel_start (channel, itvencoder->daemon);
}

gboolean
itvencoder_channel_stop (ITVEncoder *itvencoder, gint index)
{
        Channel *channel;

        channel = g_array_index (itvencoder->channel_array, gpointer, index);
        channel_stop (channel);

        return TRUE;
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
        ChannelOutput *output;
        gint i, j, k;

        for (i = 0; i < itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                output = channel->output;
                if (output->state != GST_STATE_PLAYING) {
                        continue;
                }

                /* source heartbeat check */
                for (j = 0; j < output->source.stream_count; j++) {
                        if (output->source.streams[j].type == ST_UNKNOWN) {
                                continue;
                        }
                        now = gst_clock_get_time (itvencoder->system_clock);
                        time_diff = GST_CLOCK_DIFF (output->source.streams[j].last_heartbeat, now);
                        if (time_diff > HEARTBEAT_THRESHHOLD) {
                                GST_ERROR ("%s.source.%s heart beat error %lld, restart channel.",
                                        channel->name,
                                        output->source.streams[j].name,
                                        time_diff);
                                channel_stop (channel);
                        } else {
                                GST_INFO ("%s.source.%s heart beat %" GST_TIME_FORMAT,
                                        channel->name,
                                        output->source.streams[j].name,
                                        GST_TIME_ARGS (output->source.streams[j].last_heartbeat));
                        }
                }

                /* log source timestamp. */
                for (j = 0; j < output->source.stream_count; j++) {
                        GST_INFO ("%s.source.%s timestamp %" GST_TIME_FORMAT,
                                channel->name,
                                output->source.streams[j].name,
                                GST_TIME_ARGS (output->source.streams[j].current_timestamp));
                }

                /* encoder heartbeat check */
                for (j = 0; j < output->encoder_count; j++) {
                        for (k = 0; k < output->encoders[j].stream_count; k++) {
                                now = gst_clock_get_time (itvencoder->system_clock);
                                time_diff = GST_CLOCK_DIFF (output->encoders[j].streams[k].last_heartbeat, now);
                                if (time_diff > HEARTBEAT_THRESHHOLD) {
                                        GST_ERROR ("%s.encoders.%s.%s heart beat error %lld, restart",
                                                channel->name,
                                                output->encoders[j].name,
                                                output->encoders[j].streams[k].name,
                                                time_diff);
                                        channel_stop (channel);
                                } else {
                                        GST_INFO ("%s.encoders.%s.%s heart beat %" GST_TIME_FORMAT,
                                                channel->name,
                                                output->encoders[j].name,
                                                output->encoders[j].streams[k].name,
                                                GST_TIME_ARGS (output->encoders[j].streams[k].last_heartbeat));
                                }

                        }
                }

                /* log encoder current timestamp. */
                for (j = 0; j < output->encoder_count; j++) {
                        for (k = 0; k < output->encoders[j].stream_count; k++) {
                                GST_INFO ("%s.encoders.%s.%s current timestamp %" GST_TIME_FORMAT,
                                        channel->name,
                                        output->encoders[j].name,
                                        output->encoders[j].streams[k].name,
                                        GST_TIME_ARGS (output->encoders[j].streams[k].current_timestamp));
                        }
                }

                /* sync check */
                min = GST_CLOCK_TIME_NONE;
                max = 0;
                for (j = 0; j < output->source.stream_count; j++) {
                        if (output->source.streams[j].type == ST_UNKNOWN) {
                                continue;
                        }
                        if (min > output->source.streams[j].current_timestamp) {
                                min = output->source.streams[j].current_timestamp;
                        }
                        if (max < output->source.streams[j].current_timestamp) {
                                max = output->source.streams[j].current_timestamp;
                        }
                }
                time_diff = GST_CLOCK_DIFF (min, max);
                if (time_diff > SYNC_THRESHHOLD) {
                        GST_ERROR ("%s sync error %lld", channel->name, time_diff);
                        output->source.sync_error_times += 1;
                        if (output->source.sync_error_times == 3) {
                                GST_ERROR ("sync error times %d, restart %s", output->source.sync_error_times, channel->name);
                                channel_stop (channel);
                        }
                } else {
                        output->source.sync_error_times = 0;
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

        /* start channels */
        for (i = 0; i < itvencoder->channel_array->len; i++) {
                if (!itvencoder_channel_start (itvencoder, i)) {
                        return 1;
                }
        }

        /* start http streaming */
        value = (GValue *)gst_structure_get_value (itvencoder->configure->data, "server");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "httpstreaming");
        p = (gchar *)g_value_get_string (value);
        pp = g_strsplit (p, ":", 0);
        port = atoi (pp[1]);
        g_strfreev (pp);
        itvencoder->httpstreaming = httpstreaming_new ("channels", itvencoder->channel_array, "system_clock", itvencoder->system_clock, NULL);
        httpstreaming_start (itvencoder->httpstreaming, 10, port);

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
