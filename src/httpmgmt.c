/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <gst/gst.h>
#include <string.h>

#include "httpmgmt.h"
#include "itvencoder.h"
#include "configure.h"
#include "mgmt_html.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        HTTPMGMT_PROP_0,
        HTTPMGMT_PROP_ITVENCODER,
        HTTPMGMT_PROP_CONFIGURE_FILE,
};

static void httpmgmt_class_init (HTTPMgmtClass *httpmgmtclass);
static void httpmgmt_init (HTTPMgmt *httpmgmt);
static GObject *httpmgmt_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpmgmt_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpmgmt_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static GstClockTime mgmtserver_dispatcher (gpointer data, gpointer user_data);

static void
httpmgmt_class_init (HTTPMgmtClass *httpmgmtclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (httpmgmtclass);
        GParamSpec *param;

        g_object_class->constructor = httpmgmt_constructor;
        g_object_class->set_property = httpmgmt_set_property;
        g_object_class->get_property = httpmgmt_get_property;

        param = g_param_spec_pointer (
                "itvencoder",
                "itvencoder",
                NULL,
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPMGMT_PROP_ITVENCODER, param);

        param = g_param_spec_string (
                "configure",
                "configuref",
                "configure file path",
                "itvencoder.conf",
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, HTTPMGMT_PROP_CONFIGURE_FILE, param);
}

static void
httpmgmt_init (HTTPMgmt *httpmgmt)
{
        httpmgmt->system_clock = gst_system_clock_obtain ();
        g_object_set (httpmgmt->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
        httpmgmt->configure = NULL;
}

static GObject *
httpmgmt_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties)
{
        GObject *obj;
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        obj = parent_class->constructor(type, n_construct_properties, construct_properties);

        return obj;
}

static void
httpmgmt_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail(IS_HTTPMGMT(obj));

        switch(prop_id) {
        case HTTPMGMT_PROP_ITVENCODER:
                HTTPMGMT(obj)->itvencoder = (ITVEncoder *)g_value_get_pointer (value);
                break;
        case HTTPMGMT_PROP_CONFIGURE_FILE:
                HTTPMGMT(obj)->configure_file = (gchar *)g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
httpmgmt_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        HTTPMgmt  *httpmgmt = HTTPMGMT(obj);

        switch(prop_id) {
        case HTTPMGMT_PROP_ITVENCODER:
                g_value_set_pointer (value, httpmgmt->itvencoder);
                break;
        case HTTPMGMT_PROP_CONFIGURE_FILE:
                g_value_set_string(value, httpmgmt->configure_file);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

GType
httpmgmt_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (HTTPMgmtClass), // class size.
                NULL, // base initializer
                NULL, // base finalizer
                (GClassInitFunc) httpmgmt_class_init, // class init.
                NULL, // class finalize.
                NULL, // class data.
                sizeof (HTTPMgmt),
                0, // instance size.
                (GInstanceInitFunc) httpmgmt_init, // instance init.
                NULL // value table.
        };
        type = g_type_register_static (G_TYPE_OBJECT, "HTTPMgmt", &info, 0);

        return type;
}

static void
load_configure (HTTPMgmt *httpmgmt)
{
        if (httpmgmt->configure != NULL) {
                gst_object_unref (G_OBJECT (httpmgmt->configure));
        }

        httpmgmt->configure = configure_new ("configure_path", httpmgmt->configure_file, NULL);
        configure_load_from_file (httpmgmt->configure);
}

gint
httpmgmt_start (HTTPMgmt *httpmgmt)
{
        GValue *value;
        GstStructure *structure;
        gchar *p, **pp;
        gint port;

        load_configure (httpmgmt);
        value = (GValue *)gst_structure_get_value (httpmgmt->configure->data, "server");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "httpmgmt");
        p = (gchar *)g_value_get_string (value);
        pp = g_strsplit (p, ":", 0);
        port = atoi (pp[1]);
        g_strfreev (pp);
        httpmgmt->httpserver = httpserver_new ("maxthreads", 1, "port", port, NULL);
        if (httpserver_start (httpmgmt->httpserver, mgmtserver_dispatcher, httpmgmt) != 0) {
                GST_ERROR ("Start mgmt httpserver error!");
                exit (0);
        }
}

static gint
get_channel_index (gchar *uri)
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

/**
 * mgmt_dispatcher:
 * @data: RequestData type pointer
 * @user_data: httpmgmt type pointer
 *
 * Process http request.
 *
 * Returns: positive value if have not completed the processing, for example live streaming.
 *      0 if have completed the processing.
 */
static GstClockTime
mgmtserver_dispatcher (gpointer data, gpointer user_data)
{
        RequestData *request_data = data;
        HTTPMgmt *httpmgmt = user_data;
        gchar *buf, *path;
        gint i, index;
        Encoder *encoder;
        Channel *channel;
        gboolean *ret;

        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_INFO ("new request arrived, socket is %d, uri is %s", request_data->sock, request_data->uri);
                switch (request_data->uri[1]) {
                case 'c':
                        /* get or post configure data. */
                        if (g_str_has_prefix (request_data->uri, "/configure")) {
                                if (request_data->method == HTTP_GET) {
                                        path = request_data->uri + 10;
                                        if ((*path == '\0') || (*(path + 1) == '\0')) {
                                                path = "";
                                        } else {
                                                path += 1;
                                        }
                                        buf = configure_get_var (httpmgmt->configure, path);
                                        if (buf == NULL) {
                                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                        }
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                } else if (request_data->method == HTTP_POST) {
                                        /* save configure. */
                                        gchar *var;
                                        var = request_data->raw_request + request_data->header_size;
                                        configure_set_var (httpmgmt->configure, var);
                                        if (configure_save_to_file (httpmgmt->configure) != 0) {
                                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                }
                        } else if (g_str_has_prefix (request_data->uri, "/channel")) {
                                encoder = channel_get_encoder (request_data->uri, httpmgmt->itvencoder->channel_array);
                                if (encoder == NULL) {
                                        index = get_channel_index (request_data->uri);
                                        if (index == -1) {
                                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                                return 0;
                                        } else if (request_data->parameters[0] == 's') {
                                                GST_WARNING ("Stop channel %s", channel->name);
                                                //ret = channel_source_stop (channel->source);
                                                if (ret == 0) {
                                                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                } else {
                                                        buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                }
                                                return 0;
                                        } else if (request_data->parameters[0] == 'p') {
                                                GST_WARNING ("Start channel %s", channel->name);
                                                //ret = channel_source_start (channel->source);
                                                if (ret == 0) {
                                                        for (i=0; i<channel->encoder_array->len; i++) {
                                                                encoder = g_array_index (channel->encoder_array, gpointer, i);
                                                                GST_INFO ("channel encoder pipeline string is %s", encoder->pipeline_string);
                                                                //if (channel_encoder_start (encoder) != 0) {//FIXME; return error
                                                                //        GST_ERROR ("Start encoder %s falure", encoder->name);
                                                                //}
                                                        }
                                                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                } else {
                                                        buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                }
                                                return 0;
                                        } else if (g_str_has_suffix (request_data->uri, "/restart")) {
                                                GST_WARNING ("Restart channel");
                                                index = get_channel_index (request_data->uri);
                                                if (itvencoder_channel_start (httpmgmt->itvencoder, index)) {
                                                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                } else {
                                                        buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                        write (request_data->sock, buf, strlen (buf));
                                                        g_free (buf);
                                                }
                                                return 0;
                                        } else {
                                                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                                return 0;
                                        }
                                } else if (request_data->parameters[0] == 's') {
                                        GST_WARNING ("Stop endcoder");
                                        //ret = channel_encoder_stop (encoder);
                                        if (ret == 0) {
                                                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else if (request_data->parameters[0] == 'p') {
                                        GST_WARNING ("Start endcoder");
                                        //ret = channel_encoder_start (encoder);
                                        if (ret == 0) {
                                                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else if (request_data->parameters[0] == 'r') {
                                        GST_WARNING ("Restart endcoder");
                                        //ret = channel_encoder_restart (encoder);
                                        if (ret == 0) {
                                                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        } else {
                                                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                                                write (request_data->sock, buf, strlen (buf));
                                                g_free (buf);
                                        }
                                        return 0;
                                } else {
                                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                                        write (request_data->sock, buf, strlen (buf));
                                        g_free (buf);
                                        return 0;
                                }
                        }
                        break;
                case 'm':
                        /* get mgmt, index.html */
                        if (g_str_has_prefix (request_data->uri, "/mgmt")) {
                                write (request_data->sock, index_html, strlen (index_html));
                                return 0;
                        }
                case 'g':
                        /* get gui.css */
                        if (g_str_has_prefix (request_data->uri, "/gui.css")) {
                                write (request_data->sock, gui_css, strlen (gui_css));
                                return 0;
                        }
                case 'i': /* uri is /httpmgmt/..... */
                        buf = g_strdup_printf (itvencoder_ver, PACKAGE_NAME, PACKAGE_VERSION, strlen (PACKAGE_NAME) + strlen (PACKAGE_VERSION) + 1, PACKAGE_NAME, PACKAGE_VERSION); 
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                case 'k': /* kill self */
                        if (g_str_has_prefix (request_data->uri, "/kill")) {
                                exit (1);
                        }
                default:
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                        return 0;
                }
        case HTTP_FINISH:
                g_free (request_data->user_data);
                request_data->user_data = NULL;
                return 0;
        default:
                GST_ERROR ("Unknown status %d", request_data->status);
                buf = g_strdup_printf (http_400, PACKAGE_NAME, PACKAGE_VERSION);
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
                return 0;
        }
}

