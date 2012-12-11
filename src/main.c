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

static gint remove_pid_file ()
{
        unlink ("/var/run/itvencoder.pid");
}

int
main(int argc, char *argv[])
{
        guint major, minor, micro, nano;
        const gchar *nano_str;
        ITVEncoder *itvencoder;
        ChannelConfig *channel_config;
        Channel *channel;
        gchar *name;
        HTTPServer *httpserver;
        GMainLoop *loop;
        gint i, j;

        signal (SIGPIPE, SIG_IGN);
        signal (SIGUSR1, sighandler);
        if (create_pid_file () != 0) { //FIXME remove when process exit
                exit (1);
        }

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        gst_version (&major, &minor, &micro, &nano);
        if (nano == 1)
                nano_str = "(git)";
        else if (nano == 2)
                nano_str = "(Prerelease)";
        else
                nano_str = "";
        GST_INFO ("%s version : %s", ENCODER_NAME, ENCODER_VERSION);
        GST_INFO ("gstreamer version : %d.%d.%d %s", major, minor, micro, nano_str);

        _log = log_new ("log_path", "/var/log/itvencoder/itvencoder.log", NULL);
        if (log_set_log_handler (_log) != 0) {
                exit (1);
        }

        loop = g_main_loop_new (NULL, FALSE);
        itvencoder = itvencoder_new (0, NULL);
        GST_INFO ("start time : %lld", itvencoder_get_start_time(itvencoder));
        GST_INFO ("listening ports : %d", json_integer_value(json_object_get(itvencoder->config->config, "listening_ports")));
        GST_INFO ("channel configs : %s", json_string_value(json_object_get(itvencoder->config->config, "channel_configs")));
        GST_INFO ("log directory : %s", json_string_value(json_object_get(itvencoder->config->config, "log_directory")));
        for (i=0; i<itvencoder->config->channel_config_array->len; i++) {
                channel_config = g_array_index (itvencoder->config->channel_config_array, gpointer, i);
                GST_INFO ("config file %d - %s", i, channel_config->config_path);
        }

        itvencoder_start (itvencoder);
        g_main_loop_run (loop);
        return 0;
}

