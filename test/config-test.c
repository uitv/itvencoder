
#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "config.h"
#include "jansson.h"

GST_DEBUG_CATEGORY(ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

static void
printf_extension_log_func (GstDebugCategory * category,
                           GstDebugLevel level, const gchar * file, const gchar * function,
                           gint line, GObject * object, GstDebugMessage * message, gpointer unused)
{
        const gchar *dbg_msg;

        dbg_msg = gst_debug_message_get (message);
        //fail_unless (dbg_msg != NULL);

        /* g_print ("%s\n", dbg_msg); */

        /* quick hack to still get stuff to show if GST_DEBUG is set */
        //if (g_getenv ("GST_DEBUG")) {
                gst_debug_log_default (category, level, file, function, line, object,
                                       message, unused);
        //}
}

int
main(int argc, char *argv[])
{
        Config *config;
        gchar *config_path;
        json_t *channel;

        gst_init(&argc, &argv);
        GST_DEBUG_CATEGORY_INIT(ITVENCODER, "ITVENCODER", 0, "itvencoder log");

        config = g_object_new(TYPE_CONFIG, "config_file_path", "itvencoder.conf", NULL);
        g_object_set(config, "config_file_path", "itvencoder.conf", NULL);
        g_object_get(config, "config_file_path", &config_path, NULL);
        GST_DEBUG("config file path is %s\n", config_path);

        gst_debug_remove_log_function(gst_debug_log_default);
        gst_debug_add_log_function (printf_extension_log_func, NULL);
        config_load_config_file(config);
        channel = json_object_get(config->config, "channel_configs"); 
        GST_LOG("channel config files: %s\n", json_string_value(channel));
        config_save_config_file(config);

        return 0;
}
