/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <string.h>

#include "log.h"
#include "itvencoder.h"
#include "configure.h"
#include "keyvalue.h"
#include "httpmgmt.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

Log *_log;

static void sighandler (gint number)
{
        log_reopen (_log);
}

static gint create_pid_file (gchar *pid_file)
{
        pid_t pid;
        FILE *fd;

        pid = getpid ();
        fd = fopen (pid_file, "w");
        if (fd == NULL) {
                perror ("open /var/run/itvencoder.pid file error\n");
                return 1;
        }
        fprintf (fd, "%d\n", pid);
        fclose (fd);

        return 0;
}

static gint
remove_pid_file ()
{
        unlink ("/var/run/itvencoder.pid");
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
        GST_WARNING ("%s version : %s", PACKAGE_NAME, PACKAGE_VERSION);
        GST_WARNING ("gstreamer version : %d.%d.%d %s", major, minor, micro, nano_str);
}

static void
print_itvencoder_info (ITVEncoder *itvencoder)
{
        ChannelConfig *channel_config;
        gint i;

        GST_WARNING ("start time : %lld", itvencoder_get_start_time(itvencoder));
        GST_WARNING ("listening ports : %d", json_integer_value (json_object_get (itvencoder->config->config, "listening_ports")));
        GST_WARNING ("channel configs : %s", json_string_value (json_object_get (itvencoder->config->config, "channel_configs")));
        GST_WARNING ("log directory : %s", itvencoder->config->log_dir);
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                GST_WARNING ("config file %d - %s", i, channel_config->config_path);
        }
}

static gboolean foreground = FALSE;
static gboolean version = FALSE;
static gchar *config_path = NULL;
static GOptionEntry options[] = {
        {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config_path, ("-c /full/path/to/itvencoder.conf: Specify a config file, full path is must."), NULL},
        {"foreground", 'd', 0, G_OPTION_ARG_NONE, &foreground, ("Run in the foreground"), NULL},
        {"version", 'v', 0, G_OPTION_ARG_NONE, &version, ("display version information and exit."), NULL},
        {NULL}
};

int
main (int argc, char *argv[])
{
        Config *config;
        Configure *configure;
        GValue *value;
        GstStructure *channels;
        ITVEncoder *itvencoder;
        HTTPMgmt *httpmgmt;
        GMainLoop *loop;
        pid_t process_id = 0;
        gint status;
        gint8 exit_status;
        gint ret;
        GOptionContext *ctx;
        GError *err = NULL;

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
                g_print ("iTVEncoder version: %s\n", VERSION);
                g_print ("iTVEncoder build: %s %s\n", __DATE__, __TIME__);
                exit (0);
        }

        if (gst_debug_get_default_threshold () < GST_LEVEL_WARNING) {
                gst_debug_set_default_threshold (GST_LEVEL_WARNING);
        }

        signal (SIGPIPE, SIG_IGN);

        /* config command line option */
        if (config_path) {
                config = config_new ("config_path", config_path, NULL);
                if (config_load_config_file (config) != 0) {
                        GST_ERROR ("config files load error: %s", config_path);
                        exit (0);
                }
        } else {
                config = config_new ("config_path", "/etc/itvencoder/itvencoder.conf");
                if (config_load_config_file (config) != 0) {
                        GST_ERROR ("config files load error: /etc/itvencoder/itvencoder.conf");
                        exit (0);
                }
        }

        if (config_path) {
                /* config command line option */
                configure = configure_new ("configure_path", config_path, NULL);
                if (configure_load_from_file (configure) != 0) {
                        g_print ("Load configure file %s error, exit ...\n", config_path);
                        return 1;
                }
        } else {
                /* default config path */
                configure = configure_new ("configure_path", "/etc/itvencoder.conf", NULL);
                if (configure_load_from_file (configure) != 0) {
                        g_print ("Load configure file /etc/itvencoder.conf error, exit ...\n");
                        return 1;
                }
        }

        /* run in background? */
        if (!foreground) {
                /* daemon */
                if (daemon (0, 0) != 0) {
                        GST_ERROR ("Failed to daemonize");
                        exit (0);
                }

                for (;;) {
                        process_id = fork ();
                        if (process_id > 0) {
                                /* parent process */
                                status = 0;
                                for (;;) {
                                        wait (&status);
                                        exit_status = (gint8) WEXITSTATUS (status);
                                        if (WIFEXITED (status) && (exit_status != 0)) {
                                                /* abnormal exit, restart */
                                                break;
                                        }
                                        if (WIFSIGNALED (status)) {
                                                /* child exit on an unhandled signal, restart */
                                                break;
                                        }
                                        if (WIFEXITED (status) && (exit_status == 0)) {
                                                /* exit code is 0, must exit. */
                                                exit (0);
                                        }
                                }
                        } else if (process_id == 0) {
                                /* children process, itvencoder server */
                                gchar *log_dir;
                                gchar *log_path;
                                gchar *pid_file;

                                value = configure_get_param (configure, "/server/logdir");
                                log_dir = (gchar *)g_value_get_string (value);
                                #if 0
                                if (config->log_dir[strlen(config->log_dir) - 1] == '/') {
                                        log_path = g_strdup_printf ("%sitvencoder.log", config->log_dir);
                                } else {
                                        log_path = g_strdup_printf ("%s/itvencoder.log", config->log_dir);
                                }
                                #endif
                                if (log_dir[strlen(log_dir) - 1] == '/') {
                                        log_path = g_strdup_printf ("%sitvencoder.log", log_dir);
                                } else {
                                        log_path = g_strdup_printf ("%s/itvencoder.log", log_dir);
                                }
                                _log = log_new ("log_path", log_path, NULL);
                                g_free (log_path);
                                ret = log_set_log_handler (_log);
                                if (ret != 0) {
                                        exit (ret);
                                }

                                /* remove gstInfo default handler. */
                                gst_debug_remove_log_function (gst_debug_log_default);

                                value = configure_get_param (configure, "/server/pidfile");
                                pid_file = (gchar *)g_value_get_string (value);
                                if (create_pid_file (pid_file) != 0) { //FIXME remove when process exit
                                        exit (1);
                                }
                                break;
                        }
                }
                if (process_id != 0) {
                        /* parent exit */
                        exit (0);
                }
        } else {
                log_set_stdout_handler ();
                gst_debug_remove_log_function (gst_debug_log_default);
        }

        signal (SIGPIPE, SIG_IGN);
        signal (SIGUSR1, sighandler);

        GST_WARNING ("Started ...");
        print_version_info ();

        loop = g_main_loop_new (NULL, FALSE);
        value = configure_get_param (configure, "/channel");
        channels = (GstStructure *)gst_value_get_structure (value);
        itvencoder = itvencoder_new ("config", config, NULL);
        g_object_set (itvencoder, "configure", channels, NULL);
        itvencoder_start (itvencoder);
        httpmgmt = httpmgmt_new ("itvencoder", itvencoder, NULL);
        httpmgmt_start (httpmgmt);
        print_itvencoder_info (itvencoder);
        g_main_loop_run (loop);

        return 0;
}

