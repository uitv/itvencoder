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
};

static void httpmgmt_class_init (HTTPMgmtClass *httpmgmtclass);
static void httpmgmt_init (HTTPMgmt *httpmgmt);
static GObject *httpmgmt_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties);
static void httpmgmt_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void httpmgmt_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);

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
}

static void
httpmgmt_init (HTTPMgmt *httpmgmt)
{
        httpmgmt->system_clock = gst_system_clock_obtain ();
        g_object_set (httpmgmt->system_clock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
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
configure_request (HTTPMgmt *httpmgmt, RequestData *request_data)
{
        gchar *buf, *p, *var;

        if (request_data->method == HTTP_GET) {
                /* get variables, /configure/path/to/var. */
                p = request_data->uri + 10;
                if ((*p == '\0') || (*(p + 1) == '\0')) {
                        p = "";
                } else {
                        p += 1;
                }
                var = configure_get_var (httpmgmt->itvencoder->configure, p);
                if (var == NULL) {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                } else {
                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "application/xml", strlen (var), var);
                        g_free (var);
                }
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
        } else if (request_data->method == HTTP_POST) {
                /* save configure. */
                var = request_data->raw_request + request_data->header_size;
                configure_set_var (httpmgmt->itvencoder->configure, var);
                if (configure_save_to_file (httpmgmt->itvencoder->configure) != 0) {
                        buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                } else {
                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 7, "Success");
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                }
                GST_INFO ("Reload configure.");
                itvencoder_reload_configure (httpmgmt->itvencoder); 
        }

}

static gchar *
stop_channel (HTTPMgmt *httpmgmt, gint index)
{
        gint ret;
        gchar *buf;

        ret = itvencoder_channel_stop (httpmgmt->itvencoder, index, SIGUSR2);
        if (ret == 0) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 7, "Success");
        } else if (ret == 1) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 21, "Stop a stoped channel");
        } else {
                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
        }

        return buf;
}

static gchar *
start_channel (HTTPMgmt *httpmgmt, gint index)
{
        gint ret;
        gchar *buf;

        ret = itvencoder_channel_start (httpmgmt->itvencoder, index);
        if (ret == 0) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 7, "Success");
        } else if (ret == 1) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 24, "Start a disabled channel");
        } else if (ret == 2) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 31, "Start a already started channel");
        } else {
                buf = g_strdup_printf (http_500, PACKAGE_NAME, PACKAGE_VERSION);
        }

        return buf;
}

static gchar *
restart_channel (HTTPMgmt *httpmgmt, gint index)
{
        gint ret;
        gchar *buf;

        ret = itvencoder_channel_stop (httpmgmt->itvencoder, index, SIGKILL);
        if (ret == 1) {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 24, "Restart a stoped channel");
        } else {
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 7, "Success");
        }

        return buf;
}

static void
channel_request (HTTPMgmt *httpmgmt, RequestData *request_data)
{
        gint i, index;
        gchar *buf;

        if (!httpmgmt->itvencoder->daemon) {
                /* run in foreground, channel operation is forbidden. */
                buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/plain", 9, "Forbidden");
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
                return;
        }

        index = itvencoder_url_encoder_index (request_data->uri);
        if (index == -1) {
                index = itvencoder_url_channel_index (request_data->uri);
                if (index == -1) {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                } else if (g_str_has_suffix (request_data->uri, "/stop")) {
                        GST_WARNING ("Stop channel %d", index);
                        buf = stop_channel (httpmgmt, index);
                } else if (g_str_has_suffix (request_data->uri, "/start")) {
                        GST_WARNING ("Start channel %d", index);
                        buf = start_channel (httpmgmt, index);
                } else if (g_str_has_suffix (request_data->uri, "/restart")) {
                        GST_WARNING ("Restart channel %d", index);
                        buf = restart_channel (httpmgmt, index);
                } else {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                }
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
        }
}

static gchar *
get_mediainfo (gchar *uri)
{
        GstElement *pipeline, *source, *demux;
        gchar *argv[4];
        gchar path[512];
        gchar *output, *p;
        gchar *error;
        gint status;
        gboolean ret;

        if (!g_str_has_prefix (uri, "udp://")) {
                /* udp src. */
                return NULL;
        }

        memset (path, '\0', sizeof (path));
        readlink ("/proc/self/exe", path, sizeof (path));
        argv[0] = path;
        argv[1] = "-m";
        argv[2] = uri;
        argv[3] = NULL;
        ret = g_spawn_sync (NULL, // working directory
                            argv,
                            NULL, // envp
                            0, // flags
                            NULL, // GSpawnChildSetupFunc
                            NULL, // user data
                            &output, // standard output
                            &error, // standard error
                            &status,
                            NULL); // error
        if (error != NULL) {
                g_free (error);
        }
        return output;
}

static void
tools_request (HTTPMgmt *httpmgmt, RequestData *request_data)
{
        gchar *buf, *uri, *mediainfo;

        if (g_str_has_prefix (request_data->uri, "/tools/mediainfo")) {
                uri = request_data->uri + 17; // strlen ("/tools/mediainfo/")
                mediainfo = get_mediainfo (uri);
                if (mediainfo != NULL) {
                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "application/xml", strlen (mediainfo), mediainfo);
                        g_free (mediainfo);
                } else {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                }
        } else {
                buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
        }
        write (request_data->sock, buf, strlen (buf));
        g_free (buf);
}

/**
 * httpmgmt_dispatcher:
 * @data: RequestData type pointer
 * @user_data: httpmgmt type pointer
 *
 * Process http request.
 *
 * Returns: positive value if have not completed the processing, for example live streaming.
 *      0 if have completed the processing.
 */
static GstClockTime
httpmgmt_dispatcher (gpointer data, gpointer user_data)
{
        RequestData *request_data = data;
        HTTPMgmt *httpmgmt = user_data;
        gchar *buf;

        switch (request_data->status) {
        case HTTP_REQUEST:
                GST_INFO ("new request arrived, socket is %d, uri is %s", request_data->sock, request_data->uri);
                if (g_str_has_prefix (request_data->uri, "/configure")) {
                        /* get or post configure data. */
                        configure_request (httpmgmt, request_data);
                } else if (g_str_has_prefix (request_data->uri, "/channel")) {
                        /* channel operate. */
                        channel_request (httpmgmt, request_data);
                } else if (g_str_has_prefix (request_data->uri, "/mgmt")) {
                        /* get mgmt, index.html */
                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/html", strlen (index_html), index_html);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                } else if (g_str_has_prefix (request_data->uri, "/gui.css")) {
                        /* get gui.css */
                        buf = g_strdup_printf (http_200, PACKAGE_NAME, PACKAGE_VERSION, "text/css", strlen (gui_css), gui_css);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                } else if (g_str_has_prefix (request_data->uri, "/version")) {
                        /* get version */
                        gchar *ver;
                        ver = g_strdup_printf ("iTVEncoder version: %s\niTVEncoder build: %s %s", VERSION, __DATE__, __TIME__);
                        buf = g_strdup_printf (itvencoder_ver, PACKAGE_NAME, PACKAGE_VERSION, strlen (ver), ver);
                        g_free (ver);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                } else if (g_str_has_prefix (request_data->uri, "/tools")) {
                        /* tools. */
                        tools_request (httpmgmt, request_data);
                } else {
                        buf = g_strdup_printf (http_404, PACKAGE_NAME, PACKAGE_VERSION);
                        write (request_data->sock, buf, strlen (buf));
                        g_free (buf);
                }
                break;
        case HTTP_FINISH:
                g_free (request_data->user_data);
                request_data->user_data = NULL;
                break;
        default:
                GST_ERROR ("Unknown status %d", request_data->status);
                buf = g_strdup_printf (http_400, PACKAGE_NAME, PACKAGE_VERSION);
                write (request_data->sock, buf, strlen (buf));
                g_free (buf);
        }

        return 0;
}

gint
httpmgmt_start (HTTPMgmt *httpmgmt)
{
        GValue *value;
        GstStructure *structure;
        gchar *p, **pp;
        gint port;

        /* httpmgmt port */
        value = (GValue *)gst_structure_get_value (httpmgmt->itvencoder->configure->data, "server");
        structure = (GstStructure *)gst_value_get_structure (value);
        value = (GValue *)gst_structure_get_value (structure, "httpmgmt");
        p = (gchar *)g_value_get_string (value);
        pp = g_strsplit (p, ":", 0);
        port = atoi (pp[1]);
        g_strfreev (pp);

        /* start httpmgmt */
        httpmgmt->httpserver = httpserver_new ("maxthreads", 1, "port", port, NULL);
        if (httpserver_start (httpmgmt->httpserver, httpmgmt_dispatcher, httpmgmt) != 0) {
                GST_ERROR ("Start mgmt httpserver error!");
                exit (0);
        }
}

