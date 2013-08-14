/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <gst/gst.h>
#include <string.h>
#include <glob.h>

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
        itvencoder->configure = NULL;
        itvencoder->channel_array = g_array_new (FALSE, FALSE, sizeof(gpointer));
        itvencoder->system_clock = gst_system_clock_obtain ();
        g_object_set (itvencoder->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        itvencoder->start_time = gst_clock_get_time (itvencoder->system_clock);
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

static gint
itvencoder_load_configure (ITVEncoder *itvencoder)
{
        gint ret;

        if (itvencoder->configure != NULL) {
                gst_object_unref (G_OBJECT (itvencoder->configure));
        }

        itvencoder->configure = configure_new ("configure_path", itvencoder->configure_file, NULL);
        ret = configure_load_from_file (itvencoder->configure);

        return ret;
}

gint
itvencoder_reload_configure (ITVEncoder *itvencoder)
{
        Channel *channel;
        GValue *value;
        GstStructure *structure;
        gint i;
        gchar *enable;

        if (itvencoder_load_configure (itvencoder) != 0) {
                GST_ERROR ("load configure error.");
                return 1;
        }

        for (i = 0; i < itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                value = (GValue *)gst_structure_get_value (itvencoder->configure->data, "channels");
                structure = (GstStructure *)gst_value_get_structure (value);
                value = (GValue *)gst_structure_get_value (structure, channel->name);
                channel->configure = (GstStructure *)gst_value_get_structure (value);
                enable = (gchar *)gst_structure_get_string (channel->configure, "enable");
                if (g_strcmp0 (enable, "no") == 0) {
                        channel->enable = FALSE;
                } else if (g_strcmp0 (enable, "yes") == 0) {
                        channel->enable = TRUE;
                }
        }

        return 0;
}

static void
remove_semaphore (Channel *channel)
{
        gchar *name, *sem_name;
        GValue *value;
        GstStructure *structure;
        gint i, n;

        value = (GValue *)gst_structure_get_value (channel->configure, "encoders");
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                sem_name = g_strdup_printf ("/%s%s", channel->name, name);
                sem_unlink (sem_name);
                g_free (sem_name);
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
gint
itvencoder_channel_initialize (ITVEncoder *itvencoder)
{
        GValue *value;
        GstStructure *structure, *configure;
        gint i, n;
        gchar *name, *log_dir;
        Channel *channel;
        gchar *enable;

        if (itvencoder_load_configure (itvencoder) != 0) {
                GST_ERROR ("load configure error.");
                return 1;
        }

        value = (GValue *)gst_structure_get_value (itvencoder->configure->data, "channels");
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                value = (GValue *)gst_structure_get_value (structure, name);
                configure = (GstStructure *)gst_value_get_structure (value);
                channel = channel_new ("name", name, "configure", configure, NULL);
                remove_semaphore (channel);
                channel->id = i;
                enable = (gchar *)gst_structure_get_string (channel->configure, "enable");
                if (g_strcmp0 (enable, "no") == 0) {
                        channel->enable = FALSE;
                } else if (g_strcmp0 (enable, "yes") == 0) {
                        channel->enable = TRUE;
                }
                channel_setup (channel, itvencoder->daemon);

                g_array_append_val (itvencoder->channel_array, channel);
                GST_INFO ("Channel %s added.", name);
        }

        return 0;
}

gint
itvencoder_channel_start (ITVEncoder *itvencoder, gint index)
{
        Channel *channel;

        channel = g_array_index (itvencoder->channel_array, gpointer, index);

        if (!channel->enable) {
                GST_WARNING ("Can't start a channel %s with enable set to no.", channel->name);
                return 0;
        }

        if (channel->worker_pid != 0) {
                GST_WARNING ("Start channel %s, but it's already started.", channel->name);
                return 2;
        }

        /* reset channel. */
        channel_reset (channel);

        /* start the channel. */
        return channel_start (channel, itvencoder->daemon);
}

gint
itvencoder_channel_stop (ITVEncoder *itvencoder, gint index, gint sig)
{
        Channel *channel;

        channel = g_array_index (itvencoder->channel_array, gpointer, index);
        return channel_stop (channel, sig);
}

static void
stat_report (Channel *channel)
{
        gchar *stat_file, *stat, **stats, **cpustats;
        gsize *length;
        guint64 utime, stime, ctime; // process user time, process system time, total cpu time
        guint64 rss; // Resident Set Size.
        gint i;

        stat_file = g_strdup_printf ("/proc/%d/stat", channel->worker_pid);
        if (!g_file_get_contents (stat_file, &stat, length, NULL)) {
                GST_ERROR ("Read process %d's stat failure.");
                return;
        }
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
        GST_INFO ("Channel %s's average cpu: %d%%, cpu: %d%%, rss: %lluMB",
                channel->name,
                ((utime + stime) * 100) / (ctime - channel->start_ctime),
                ((utime - channel->last_utime + stime - channel->last_stime) * 100) / (ctime - channel->last_ctime),
                rss/1000000);
        channel->last_ctime = ctime;
        channel->last_utime = utime;
        channel->last_stime = stime;
}

static void
rotate_log (ITVEncoder *itvencoder, gchar *log_path, pid_t pid)
{
        struct stat st;
        gchar *name;
        glob_t pglob;
        gint i;

        g_stat (log_path, &st);
        if (st.st_size > LOG_SIZE) {
                name = g_strdup_printf ("%s-%llu", log_path, gst_clock_get_time (itvencoder->system_clock));
                g_rename (log_path, name);
                g_free (name);
                kill (pid, SIGUSR1); /* reopen log file. */
                name = g_strdup_printf ("%s-*", log_path);
                glob (name, 0, NULL, &pglob);
                if (pglob.gl_pathc > LOG_ROTATE) {
                        for (i = 0; i < pglob.gl_pathc - LOG_ROTATE; i++) {
                                g_remove (pglob.gl_pathv[i]);
                        }
                }
                globfree (&pglob);
                g_free (name);
        }
}

static void
log_rotate (ITVEncoder *itvencoder)
{
        gchar *log_path;
        gint i;
        Channel *channel;

        /* itvencoder log rotate. */
        log_path = g_strdup_printf ("%sitvencoder.log", itvencoder->log_dir);
        rotate_log (itvencoder, log_path, getpid());
        g_free (log_path);

        /* channels log rotate. */
        for (i = 0; i < itvencoder->channel_array->len; i++) {
                channel = g_array_index (itvencoder->channel_array, gpointer, i);
                log_path = g_strdup_printf ("%s/channel%d/itvencoder.log", itvencoder->log_dir, i);
                rotate_log (itvencoder, log_path, channel->worker_pid);
                g_free (log_path);
        }
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
                if (!channel->enable) {
                        continue;
                }
                output = channel->output;
                if (*(output->state) != GST_STATE_PLAYING) {
                        continue;
                }

                /* source heartbeat check */
                for (j = 0; j < output->source.stream_count; j++) {
                        if (output->source.streams[j].type == ST_UNKNOWN) {
                                continue;
                        }
                        now = gst_clock_get_time (itvencoder->system_clock);
                        time_diff = GST_CLOCK_DIFF (output->source.streams[j].last_heartbeat, now);
                        if ((time_diff > HEARTBEAT_THRESHHOLD) && itvencoder->daemon) {
                                GST_ERROR ("%s.source.%s heart beat error %lld, restart channel.",
                                        channel->name,
                                        output->source.streams[j].name,
                                        time_diff);
                                /* restart channel. */
                                goto restart;
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
                                if ((time_diff > HEARTBEAT_THRESHHOLD) && itvencoder->daemon) {
                                        GST_ERROR ("%s.encoders.%s.%s heart beat error %lld, restart",
                                                channel->name,
                                                output->encoders[j].name,
                                                output->encoders[j].streams[k].name,
                                                time_diff);
                                        /* restart channel. */
                                        goto restart;
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
                if ((time_diff > SYNC_THRESHHOLD) && itvencoder->daemon){
                        GST_ERROR ("%s sync error %lld", channel->name, time_diff);
                        output->source.sync_error_times += 1;
                        if (output->source.sync_error_times == 3) {
                                GST_ERROR ("sync error times %d, restart %s", output->source.sync_error_times, channel->name);
                                /* restart channel. */
                                goto restart;
                        }
                } else {
                        output->source.sync_error_times = 0;
                }

                /* stat report. */
                if (itvencoder->daemon && (channel->worker_pid != 0)) {
                        stat_report (channel);
                }

                /* log rotate. */
                if (itvencoder->daemon) {
                        log_rotate (itvencoder);
                }
                continue;
restart:
                channel_stop (channel, SIGKILL);
                g_usleep (1000000); // wait 1s.
                itvencoder_channel_start (itvencoder, i);
        }

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
        GstClockID id;
        GstClockTime t;
        GstClockReturn ret;

        /* start channels */
        for (i = 0; i < itvencoder->channel_array->len; i++) {
                if (itvencoder_channel_start (itvencoder, i) != 0) {
                        return 1;
                }
        }

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

gint
itvencoder_url_channel_index (gchar *uri)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *c;
        gint index = -1;

        regex = g_regex_new ("^/channel/(?<channel>[0-9]+).*", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                c = g_match_info_fetch_named (match_info, "channel");
                index = atoi (c);
                g_free (c);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return index;
}

gint
itvencoder_url_encoder_index (gchar *uri)
{
        GRegex *regex = NULL;
        GMatchInfo *match_info = NULL;
        gchar *e;
        gint index = -1;

        regex = g_regex_new ("^/channel/.*/encoder/(?<encoder>[0-9]+).*", G_REGEX_OPTIMIZE, 0, NULL);
        g_regex_match (regex, uri, 0, &match_info);
        if (g_match_info_matches (match_info)) {
                e = g_match_info_fetch_named (match_info, "encoder");
                index = atoi (e);
                g_free (e);
        }

        if (match_info != NULL)
                g_match_info_free (match_info);
        if (regex != NULL)
                g_regex_unref (regex);

        return index;
}

