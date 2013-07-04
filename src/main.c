/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <string.h>

#include "log.h"
#include "itvencoder.h"
#include "configure.h"
#include "httpmgmt.h"
#include "httpstreaming.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

Log *_log;

static void sighandler (gint number)
{
        log_reopen (_log);
}

static void
print_version_info ()
{
        guint major, minor, micro, nano;
        const gchar *nano_str;

        gst_version (&major, &minor, &micro, &nano);
        if (nano == 1)
                nano_str = "(git)";
        else if (nano == 2)
                nano_str = "(Prerelease)";
        else
                nano_str = "";
        g_print ("iTVEncoder version: %s\n", VERSION);
        g_print ("iTVEncoder build: %s %s\n", __DATE__, __TIME__);
        g_print ("gstreamer version : %d.%d.%d %s\n", major, minor, micro, nano_str);
}

static gint
init_log (gchar *log_path)
{
        gint ret;

        _log = log_new ("log_path", log_path, NULL);
        ret = log_set_log_handler (_log);
        if (ret != 0) {
                return ret;
        }

        /* remove gstInfo default handler. */
        gst_debug_remove_log_function (gst_debug_log_default);

        return 0;
}

static gboolean foreground = FALSE;
static gboolean version = FALSE;
static gchar *config_path = NULL;
static gint channel_id = -1;
static GOptionEntry options[] = {
        {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path, ("-c /full/path/to/itvencoder.conf: Specify a config file, full path is must."), NULL},
        {"channel", 'n', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &channel_id, NULL, NULL},
        {"foreground", 'd', 0, G_OPTION_ARG_NONE, &foreground, ("Run in the foreground"), NULL},
        {"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("display version information and exit."), NULL},
        {NULL}
};

int
main (int argc, char *argv[])
{
        Configure *configure;
        GValue *value;
        GstStructure *channels;
        ITVEncoder *itvencoder;
        HTTPMgmt *httpmgmt;
        HTTPStreaming *httpstreaming;
        GMainLoop *loop;
        gint ret;
        GOptionContext *ctx;
        GError *err = NULL;
        gchar *log_dir, *log_path;

        if (!g_thread_supported ()) {
                g_thread_init (NULL);
        }
        ctx = g_option_context_new (NULL);
        g_option_context_add_main_entries (ctx, options, NULL);
        g_option_context_add_group (ctx, gst_init_get_option_group ());
        if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
                g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
                exit (0);
        }
        g_option_context_free (ctx);
        GST_DEBUG_CATEGORY_INIT (ITVENCODER, "itvencoder", 0, "itvencoder log");

        if (version) {
                print_version_info ();
                exit (0);
        }

        if (gst_debug_get_default_threshold () < GST_LEVEL_WARNING) {
                gst_debug_set_default_threshold (GST_LEVEL_WARNING);
        }

        /* configure. */
        if (config_path) {
                /* config command line option */
                configure = configure_new ("configure_path", config_path, NULL);
                if (configure_load_from_file (configure) != 0) {
                        g_print ("Load configure file %s error, exit ...\n", config_path);
                        return 1;
                }
        } else {
                /* default config path */
                configure = configure_new ("configure_path", "/etc/itvencoder/itvencoder.conf", NULL);
                if (configure_load_from_file (configure) != 0) {
                        g_print ("Load configure file /etc/itvencoder/itvencoder.conf error, exit ...\n");
                        return 1;
                }
        }

        if (channel_id != -1) {
                GValue *value;
                GstStructure *structure;
                gchar *name;
                Channel *channel;

                /* launch a channel. */
                value = configure_get_param (configure, "/server/logdir");
                log_dir = (gchar *)g_value_get_string (value);
                if (log_dir[strlen(log_dir) - 1] == '/') {
                        log_path = g_strdup_printf ("%schannel%d/itvencoder.log", log_dir, channel_id);
                } else {
                        log_path = g_strdup_printf ("%s/channel%d/itvencoder.log", log_dir, channel_id);
                }
                ret = init_log (log_path);
                if (ret != 0) {
                        exit (1);
                }

                value = (GValue *)gst_structure_get_value (configure->data, "channels");
                structure = (GstStructure *)gst_value_get_structure (value);
                name = (gchar *)gst_structure_nth_field_name (structure, channel_id);
                value = (GValue *)gst_structure_get_value (structure, name);
                structure = (GstStructure *)gst_value_get_structure (value);
                channel = channel_new ("name", name, "configure", structure, NULL);
                channel->id = channel_id;
                loop = g_main_loop_new (NULL, FALSE);
                channel_setup (channel, TRUE);
                channel_start (channel, FALSE);
                g_main_loop_run (loop);
                return 0;
        }

        if (!foreground) {
                /* run in background. */
                if (daemon (0, 0) != 0) {
                        g_print ("Failed to daemonize");
                        exit (0);
                }

                value = configure_get_param (configure, "/server/logdir");
                log_dir = (gchar *)g_value_get_string (value);
                if (log_dir[strlen(log_dir) - 1] == '/') {
                        log_path = g_strdup_printf ("%sitvencoder.log", log_dir);
                } else {
                        log_path = g_strdup_printf ("%s/itvencoder.log", log_dir);
                }
                ret = init_log (log_path);
                if (ret != 0) {
                        g_print ("Init log error, ret %d.\n", ret);
                        exit (1);
                }
        }

        signal (SIGPIPE, SIG_IGN);
        signal (SIGUSR1, sighandler);
        GST_WARNING ("iTVEncoder started ...");

        loop = g_main_loop_new (NULL, FALSE);

        /* itvencoder */
        if (config_path) {
                itvencoder = itvencoder_new ("daemon", !foreground, "configure", config_path, NULL);
        } else {
                itvencoder = itvencoder_new ("daemon", !foreground, "configure", "/etc/itvencoder/itvencoder.conf", NULL);
        }
        itvencoder->log_path = log_path;
        if (!itvencoder_channel_initialize (itvencoder)) {
                GST_ERROR ("exit ...");
                return 1;
        }
        if (itvencoder_start (itvencoder) != 0) {
                GST_ERROR ("exit ...");
                exit (1);
        }

        /* management */
        httpmgmt = httpmgmt_new ("itvencoder", itvencoder, NULL);
        httpmgmt_start (httpmgmt);

        /* httpstreaming */
        httpstreaming = httpstreaming_new ("itvencoder", itvencoder, NULL);
        httpstreaming_start (httpstreaming, 10);

        g_main_loop_run (loop);

        return 0;
}

