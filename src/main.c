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

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

Log *_log;

static void sighandler (gint number)
{
        log_reopen (_log);
}

static gint create_pid_file ()
{
        pid_t pid;
        FILE *fd;

        pid = getpid ();
        fd = fopen ("/var/run/itvencoder.pid", "w");
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
        GST_INFO ("%s version : %s", ENCODER_NAME, ENCODER_VERSION);
        GST_INFO ("gstreamer version : %d.%d.%d %s", major, minor, micro, nano_str);
}

static void
print_itvencoder_info (ITVEncoder *itvencoder)
{
        ChannelConfig *channel_config;
        gint i;

        GST_INFO ("start time : %lld", itvencoder_get_start_time(itvencoder));
        GST_INFO ("listening ports : %d", json_integer_value (json_object_get (itvencoder->config->config, "listening_ports")));
        GST_INFO ("channel configs : %s", json_string_value (json_object_get (itvencoder->config->config, "channel_configs")));
        GST_INFO ("log directory : %s", json_string_value (json_object_get (itvencoder->config->config, "log_directory")));
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                GST_INFO ("config file %d - %s", i, channel_config->config_path);
        }
}

int
main(int argc, char *argv[])
{
        ITVEncoder *itvencoder;
        GMainLoop *loop;
        gboolean foreground;
        gchar *config = NULL;
        pid_t process_id = 0;
        gint status;
        gint8 exit_status;
        gint ret;

        GOptionEntry options[] = {
                {"foreground", 'd', 0, G_OPTION_ARG_NONE, &foreground, ("Run in the foreground"), NULL},
                {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config, ("-c itvencoder.conf: Specify a config file"), NULL},
                {NULL}
        };
        GOptionContext *ctx;
        GError *err = NULL;
        if (!g_thread_supported ()) {
                g_thread_init (NULL);
        }
        ctx = g_option_context_new ("[ADDITIONAL ARGUMENTS]");
        g_option_context_add_main_entries (ctx, options, NULL);
        g_option_context_add_group (ctx, gst_init_get_option_group ());
        if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
                g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
                exit (1);
        }
        g_option_context_free (ctx);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        _log = log_new ("log_path", "/var/log/itvencoder/itvencoder.log", NULL);
        ret = log_set_log_handler (_log);
        if (ret != 0) {
                exit (ret);
        }

        if (!foreground) { /* run in background */

                /* daemon */
                if (daemon (0,0) != 0) {
                        GST_ERROR ("Failed to daemonize");
                        exit (-1);
                }
        
                if (create_pid_file () != 0) { //FIXME remove when process exit
                        exit (1);
                }

                /* remove gstInfo default handler. */
                gst_debug_remove_log_function (gst_debug_log_default);
        }

        signal (SIGPIPE, SIG_IGN);
        signal (SIGUSR1, sighandler);

        print_version_info ();
        loop = g_main_loop_new (NULL, FALSE);
        itvencoder = itvencoder_new (0, NULL);
        print_itvencoder_info (itvencoder);
        itvencoder_start (itvencoder);
        g_main_loop_run (loop);

        return 0;
}

