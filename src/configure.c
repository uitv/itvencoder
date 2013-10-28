/*
 *  Copyright (c) 2012 iTV.cn
 *  Author Zhang Ping <zhangping@itv.cn>
 */

#include <unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "configure.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CONFIGURE_PROP_0,
        CONFIGURE_PROP_FILE_PATH,
};

static void configure_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void configure_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static void configure_dispose (GObject *obj);
static void configure_finalize (GObject *obj);
static gint configure_release_lines (GArray *lines);
static gint configure_release_variables (GArray *variables);

static void
configure_class_init (ConfigureClass *configureclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (configureclass);
        GParamSpec *param;

        g_object_class->set_property = configure_set_property;
        g_object_class->get_property = configure_get_property;
        g_object_class->dispose = configure_dispose;
        g_object_class->finalize = configure_finalize;

        param = g_param_spec_string (
                "configure_path",
                "configuref",
                "configure file path",
                "itvencoder.conf",
                G_PARAM_WRITABLE | G_PARAM_READABLE
        );
        g_object_class_install_property (g_object_class, CONFIGURE_PROP_FILE_PATH, param);
}

static void
configure_init (Configure *configure)
{
        configure->lines = g_array_new (FALSE, FALSE, sizeof (gchar *));
        configure->variables = g_array_new (FALSE, FALSE, sizeof (gpointer));
        configure->data = NULL;
        configure->raw = NULL;
}

static void
configure_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail (IS_CONFIGURE (obj));

        switch (prop_id) {
        case CONFIGURE_PROP_FILE_PATH:
                CONFIGURE(obj)->configure_file = (gchar *)g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
                break;
        }
}

static void
configure_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
        Configure *configure = CONFIGURE (obj);

        switch (prop_id) {
        case CONFIGURE_PROP_FILE_PATH:
                g_value_set_string(value, configure->configure_file);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
}

static void
configure_dispose (GObject *obj)
{
        Configure *configure = CONFIGURE (obj);
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        if (configure->configure_file != NULL) {
                g_free (configure->configure_file);
                configure->configure_file = NULL;
        }

        G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
configure_finalize (GObject *obj)
{
        Configure *configure = CONFIGURE (obj);
        GObjectClass *parent_class = g_type_class_peek(G_TYPE_OBJECT);

        if (configure->raw != NULL) {
                g_free (configure->raw);
        }

        if (configure->data != NULL) {
                gst_structure_free (configure->data);
        }

        configure_release_lines (configure->lines);
        g_array_free (configure->lines, TRUE);

        configure_release_variables (configure->variables);
        g_array_free (configure->variables, TRUE);

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

GType
configure_get_type (void)
{
        static GType type = 0;

        if (type) return type;
        static const GTypeInfo info = {
                sizeof (ConfigureClass),
                NULL, // base class initializer
                NULL, // base class finalizer
                (GClassInitFunc)configure_class_init,
                NULL,
                NULL,
                sizeof(Configure),
                0,
                (GInstanceInitFunc)configure_init,
                NULL
        };
        type = g_type_register_static (G_TYPE_OBJECT, "Configure", &info, 0);

        return type;
}

static gchar *
ini_format (gchar *name, gchar *data)
{
        gchar *p1, *p2, *p3;
        gint bracket_depth;
 
        p1 = g_strdup_printf ("[%s]\n", name);
        p2 = data;
        bracket_depth = 0;
        for (;;) {
                switch (*p2) {
                case '{':
                        bracket_depth++;
                        if (bracket_depth > 1) {
                                p3 = g_strdup_printf ("%s%c", p1, *p2);
                                g_free (p1);
                                p1 = p3;
                        }
                        break;
                case '}':
                        bracket_depth--;
                        if (bracket_depth > 0) {
                                p3 = g_strdup_printf ("%s%c", p1, *p2);
                                g_free (p1);
                                p1 = p3;
                        }
                        break;
                case '\\':
                        if ((bracket_depth <= 1) && (*(p2 + 1) == 'n')) {
                                /* \n found, and after first { */
                                if (*(p2 - 1) != '{') {
                                        /* {\n found */
                                        p3 = g_strdup_printf ("%s%c", p1, '\n');
                                        g_free (p1);
                                        p1 = p3;
                                } 
                                p2++;
                        } else {
                                p3 = g_strdup_printf ("%s%c", p1, '\\');
                                g_free (p1);
                                p1 = p3;
                        }
                        break;
                default:
                        p3 = g_strdup_printf ("%s%c", p1, *p2);
                        g_free (p1);
                        p1 = p3;
                        break;
                }
                p2++;
                if (p2 - data >= strlen (data)) {
                        break;
                }
        }
        *p2 = '\0';
       
        return p1;
}


static GKeyFile *
ini_data_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GKeyFileFlags flags;
        GError *e = NULL;
        gchar *ini_data;

        ini_data = ini_format (name, data);
        gkeyfile = g_key_file_new ();
        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (gkeyfile, ini_data, strlen(ini_data), flags, &e)) {
                g_free (ini_data);
                g_error (e->message);
                g_error_free (e);
                return NULL;
        }
        g_free (ini_data);

        return gkeyfile;
}

static GstStructure *
configure_property_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v, *p1;
        gint i;
        gsize number;
        GstStructure *structure;
        GValue value = { 0, { { 0 } } };
        GRegex *regex;

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s element parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                regex = g_regex_new ("<[^>]*>([^<]*)", 0, 0, NULL);
                p1 = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                g_regex_unref (regex);
                g_free (v);
                v = p1;
                //g_print ("%s : %s\n", p[i], v);
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_static_string (&value, v);
                gst_structure_set_value (structure, p[i], &value);
                g_value_unset (&value);
                g_free (v);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

/*
 * valid name should be alphanumeric.
 */
static gboolean
is_valid_name (gchar *name)
{
        gchar *p;

        p = name;
        while (*p != '\0') {
                if (!g_ascii_isalnum (*p) && ! (*p == '_')) {
                        return FALSE;
                }
                p++;
        }

        return TRUE;
}

static GstStructure *
configure_element_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v, *var;
        gint i;
        gsize number;
        GstStructure *structure, *property;
        GValue value = { 0, { { 0 } } };
        GRegex *regex;

        if (!is_valid_name (name)) {
                GST_ERROR ("Invalid element configure: %s\n", name);
                return NULL;
        }
        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s element parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                //g_print ("%s : %s\n", p[i], v);
                if (g_strcmp0 (p[i], "property") == 0) {
                        property = configure_property_parse (p[i], v);
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, property, NULL);
                        gst_structure_free (property);
                } else if (g_strcmp0 (p[i], "caps") == 0) {
                        regex = g_regex_new ("<[^>]*>([^<]*)", 0, 0, NULL);
                        var = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                        g_regex_unref (regex);
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, var);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                        g_free (var);
                } else if (g_strcmp0 (p[i], "option") == 0) {
                        regex = g_regex_new ("<[^>]*>([^<]*)", 0, 0, NULL);
                        var = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                        g_regex_unref (regex);
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, var);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                        g_free (var);
                } else {
                        GST_ERROR ("invalid element configuration\n");
                }
                g_free (v);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

static GstStructure *
configure_bin_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GRegex *regex;
        GError *e = NULL;
        gchar **p, *v1, *v2;
        gint i;
        gsize number;
        GstStructure *structure, *bin;
        GValue value = { 0, { { 0 } } };

        if (!is_valid_name (name)) {
                GST_ERROR ("Invalid bin configure: %s\n", name);
                return NULL;
        }
        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                if (!is_valid_name (p[i])) {
                        GST_ERROR ("Invalid bin configure: %s\n", p[i]);
                        return NULL;
                }
                v1 = g_key_file_get_value (gkeyfile, name, p[i], &e);
                //g_print ("%s : %s\n", p[i], v1);
                /* remove var tag */
                regex = g_regex_new ("<[^>]*>([^<]*)</[^>]*>", 0, 0, NULL);
                v2 = g_regex_replace (regex, v1, -1, 0, "\\1", 0, NULL);
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_static_string (&value, v2);
                gst_structure_set_value (structure, p[i], &value);
                g_value_unset (&value);
                g_regex_unref (regex);
                g_free (v1);
                g_free (v2);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

static GstStructure *
configure_bins_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;
        GstStructure *structure, *bin;

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s element parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                //g_print ("%s : %s\n", p[i], v);
                bin = configure_bin_parse (p[i], v);
                if (bin == NULL) {
                        return NULL;
                }
                gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, bin, NULL);
                gst_structure_free (bin);
                g_free (v);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

static GstStructure *
configure_pipeline_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v, *var;
        gint i;
        gsize number;
        GstStructure *structure, *elements, *element, *bins;
        GValue value = { 0, { { 0 } } };
        GRegex *regex;

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("number is %d\n", number);
        structure = gst_structure_empty_new (name);
        elements = gst_structure_empty_new ("elements");
        regex = g_regex_new ("<[^>]*>([^<]*)", 0, 0, NULL);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                //g_print ("%s : %s\n", p[i], v);
                if (g_strcmp0 (p[i], "bins") == 0) {
                        /* bin found */
                        bins = configure_bins_parse (p[i], v);
                        if (bins == NULL) {
                                return NULL;
                        }
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, bins, NULL);
                        gst_structure_free (bins);
                } else if (g_strcmp0 (p[i], "httpstreaming") == 0) {
                        /* whether support http output or not */
                        var = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, var);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                        g_free (var);
                } else if (g_strcmp0 (p[i], "udpstreaming") == 0) {
                        /* whether support udp output or not */
                        var = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, var);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                        g_free (var);
                } else {
                        /* should be a element */
                        element = configure_element_parse (p[i], v);
                        if (element == NULL) {
                                return NULL;
                        }
                        gst_structure_set (elements, p[i], GST_TYPE_STRUCTURE, element, NULL);
                        gst_structure_free (element);
                }
                g_free (v);
        }
        g_regex_unref (regex);
        gst_structure_set (structure, "elements", GST_TYPE_STRUCTURE, elements, NULL);
        gst_structure_free (elements);
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

static GstStructure *
configure_encoder_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;
        GstStructure *structure, *encoder;
        GValue value = { 0, { { 0 } } };

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("number is %d\n", number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                //g_print ("%s : %s\n", p[i], v);
                encoder = configure_pipeline_parse (p[i], v);
                if (encoder == NULL) {
                        return NULL;
                }
                if (gst_structure_has_field (structure, p[i])) {
                        GST_ERROR ("Encoder name conflict: %s", p[i]);
                        return NULL;
                }
                gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, encoder, NULL);
                gst_structure_free (encoder);
                g_free (v);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

static GstStructure *
configure_channel_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GRegex *regex;
        GError *e = NULL;
        gchar **p, *v, *v1;
        gint i;
        gsize number;
        GstStructure *structure, *source, *encoder;
        GValue value = { 0, { { 0 } } };

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                if (g_strcmp0 (p[i], "enable") == 0) {
                        /* remove var tag */
                        regex = g_regex_new ("<[^>]*>([^<]*)</[^>]*>", 0, 0, NULL);
                        v1 = g_regex_replace (regex, v, -1, 0, "\\1", 0, NULL);
                        if ((g_strcmp0 (v1, "no") != 0) && (g_strcmp0 (v1, "yes") != 0)) {
                                GST_ERROR ("Configure error: %s.\n", v);
                                return NULL;
                        }
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, v1);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                        g_regex_unref (regex);
                        g_free (v1);
                } else if (g_strcmp0 (p[i], "debug") == 0) {
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, v);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                } else if (g_strcmp0 (p[i], "source") == 0) {
                        source = configure_pipeline_parse (p[i], v);
                        if (source == NULL) {
                                return NULL; //FIXME
                        }
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, source, NULL);
                        gst_structure_free (source);
                } else if (g_strcmp0 (p[i], "encoders") == 0) {
                        encoder = configure_encoder_parse (p[i], v);
                        if (encoder == NULL) {
                                return NULL;
                        }
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, encoder, NULL);
                        gst_structure_free (encoder);
                } else if (g_strcmp0 (p[i], "title") == 0) {
                        g_value_init (&value, G_TYPE_STRING);
                        g_value_set_static_string (&value, v);
                        gst_structure_set_value (structure, p[i], &value);
                        g_value_unset (&value);
                } else {
                        GST_ERROR ("Unknown channel configure entrance: %s.", p[i]);
                        return NULL;
                }
                g_free (v);
        }
        g_strfreev (p);
        g_key_file_free (gkeyfile);

        return structure;
}

/*
 * replace newline with \n, so it can be parsed by glib ini style parser.
 */
static gchar *
prepare_for_file_parse (Configure *configure)
{
        gchar *p1, *p2, *p3;
        gint bracket_depth;

        p3 = p1 = g_malloc (configure->size * 2);
        p2 = configure->raw;
        bracket_depth = 0;
        for (;;) {
                switch (*p2) {
                case '{':
                        bracket_depth++;
                        *p3 = *p2;
                        break;
                case '}':
                        bracket_depth--;
                        *p3 = *p2;
                        break;
                case '\n':
                        if (bracket_depth > 0) {
                                *p3 = '\\';
                                p3++;
                                *p3 = 'n'; 
                        } else {
                                *p3 = *p2;
                        }
                        break;
                default:
                        *p3 = *p2;
                        break;
                }
                p2++;
                p3++;
                if ((p2 - configure->raw) == configure->size) {
                        break;
                }
        }
        *p3 = '\0';
        p3 = g_strdup_printf ("%s", p1);
        g_free (p1);

        return p3; 
}

/*
 * parse configure as GstStructure type from which to construct gstreamer pipeline.
 */
static gint
configure_file_parse (Configure *configure)
{
        GKeyFile *gkeyfile;
        GKeyFileFlags flags;
        GError *e = NULL;
        gsize number;
        gchar *ini, **p, *v;
        gint i;
        GstStructure *structure, *channels;
        GValue value = { 0, { { 0 } } };

        ini = prepare_for_file_parse (configure);
        gkeyfile = g_key_file_new ();
        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (gkeyfile, ini, strlen (ini), flags, &e)) {
                g_free (ini);
                g_error (e->message);
                g_error_free (e);
                return 1;
        }
        g_free (ini);

        /* parse server group */
        p = g_key_file_get_keys (gkeyfile, "server", &number, &e);
        if (e != NULL) {
                g_error (e->message);
                g_error_free (e);
                return 1;
        }
        structure = gst_structure_empty_new ("server");
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, "server", p[i], &e);
                g_value_init (&value, G_TYPE_STRING);
                g_value_set_static_string (&value, v);
                gst_structure_set_value (structure, p[i], &value);
                if (!is_valid_name (p[i])) {
                        GST_ERROR ("Invalid server configure: %s\n", p[i]);
                        return 1;
                }
                g_value_unset (&value);
                g_free (v);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "server", GST_TYPE_STRUCTURE, structure, NULL);
        gst_structure_free (structure);

        /* parse channels group */
        p = g_key_file_get_keys (gkeyfile, "channels", &number, &e);
        structure = gst_structure_empty_new ("channels");
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, "channels", p[i], &e);
                if (!is_valid_name (p[i])) {
                        GST_ERROR ("Invalid channel configure: %s\n", p[i]);
                        return 1;
                }
                if (gst_structure_has_field (structure, p[i])) {
                        GST_ERROR ("Channel name conflict: %s", p[i]);
                        return 1;
                }
                channels = configure_channel_parse (p[i], v);
                if (channels == NULL) {
                        return 1;
                }
                gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, channels, NULL);
                g_free (v);
                gst_structure_free (channels);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "channels", GST_TYPE_STRUCTURE, structure, NULL);
        gst_structure_free (structure);

        g_key_file_free (gkeyfile);

        return 0;
}

static gint
configure_extract_lines (Configure *configure)
{
        gchar *p, *p1, *p2, *p3, *p4, *p5, *p6, *group, **group_array; 
        GError *e = NULL;
        ConfigurableVar *variable;
        gint i, line_number, index, bracket_depth;
        gchar var_status;
        GstStructure *configure_mgmt;
        GRegex *regex;
        GMatchInfo *match_info;

        p = g_strdup (configure->raw);
        p1 = p2 = p3 = p;
        line_number = 0;
        index = 0;
        group = NULL;
        bracket_depth = 0;
        for (;;) {
                if (p1 - p >= configure->size) break;

                /* process one line every time */
                while ((*p2 != '\n') && (*p2 != '\0')) {
                        p2++;
                }
                line_number++;
                
                p4 = p1;
                var_status = '\0';
                for (;;) {
                        switch (*p3) {
                        case '#': /* line end by comment. */
                        case '\n': /* end of line. */
                                if ((var_status == '\0') || (var_status == 'v')) {
                                        p5 = g_strndup (p4, p2 - p4 + 1);
                                        g_array_append_val (configure->lines, p5);
                                        index++;
                                        p3 = p2;
                                } else {
                                        GST_ERROR ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return 1;
                                }
                                break;
                        case '[':
                                if (var_status == '\0') {
                                        if (g_ascii_strncasecmp (p3, "[server]", 8) == 0) {
                                                g_free (group);
                                                group = g_strdup ("server");
                                        } else if (g_ascii_strncasecmp (p3, "[channels]", 10) == 0) {
                                                g_free (group);
                                                group = g_strdup ("channels");
                                        }
                                }
                                break;
                        case '{':
                                bracket_depth++;
                                group_array = g_strsplit (group, "/", 5);
                                if (g_ascii_strncasecmp (group, "channels", 8) == 0) {
                                        if ((bracket_depth <= 2) || ((bracket_depth == 3) && (g_strcmp0 (group_array[2], "encoders") == 0))) {
                                                regex = g_regex_new ("( *)([^ ]*)( *= *{.*)", G_REGEX_DOTALL, 0, NULL);
                                                p5 = g_regex_replace (regex, p1, -1, 0, "\\2", 0, NULL);
                                                g_regex_unref (regex);
                                                p6 = g_strdup_printf ("%s/%s", group, p5);
                                                g_free (group);
                                                g_free (p5);
                                                group = p6;
                                        }
                                }
                                g_strfreev (group_array);
                                break;
                        case '}':
                                bracket_depth--;
                                group_array = g_strsplit (group, "/", 5);
                                if (g_ascii_strncasecmp (group, "channels", 8) == 0) {
                                        if ((bracket_depth == 1) || ((bracket_depth == 2) && (g_strcmp0 (group_array[2], "encoders") == 0))) {
                                                i = 0;
                                                while (group_array[i] != NULL) {
                                                        i++;
                                                }
                                                i -= 2;
                                                p5 = g_strdup (group_array[i]);
                                                p6 = p5;
                                                while (i != 0) {
                                                        p5 = g_strdup_printf ("%s/%s", group_array[i - 1], p6);
                                                        g_free (p6);
                                                        p6 = p5;
                                                        i--;
                                                }
                                                g_free (group);
                                                group = p5;
                                        } else if (bracket_depth == 0) {
                                                g_free (group);
                                                group = g_strdup ("channels");
                                        }
                                }
                                g_strfreev (group_array);
                                break;
                        case '<':
                                /* find a variable */
                                if ((var_status == '\0') || (var_status == 'v')) {
                                        var_status = '<';
                                        variable = g_malloc (sizeof (ConfigurableVar));
                                        regex = g_regex_new ("^<var[^>]*type *= *\\x22(?<type>[^\\x22]*)\\x22.*", G_REGEX_DOTALL, 0, NULL);
                                        g_regex_match (regex, p3, 0, &match_info);
                                        g_regex_unref (regex);
                                        if (!g_match_info_matches (match_info)) {
                                                GST_ERROR ("parse var type error, line: %d\n", line_number);
                                                return 1;
                                        }
                                        variable->type = g_match_info_fetch_named (match_info, "type");
                                        g_match_info_free (match_info);
                                        regex = g_regex_new ("^<var[^>]*name *= *\\x22(?<name>[^\\x22]*)\\x22.*", G_REGEX_DOTALL, 0, NULL);
                                        g_regex_match (regex, p3, 0, &match_info);
                                        g_regex_unref (regex);
                                        if (!g_match_info_matches (match_info)) {
                                                GST_ERROR ("parse var name error, line: %d\n", line_number);
                                                return 1;
                                        }
                                        variable->name = g_match_info_fetch_named (match_info, "name");
                                        g_match_info_free (match_info);
                                        variable->group = g_strdup (group);
                                } else if ((var_status == '>') && (*(p3 + 1) == '/')) {
                                        /* variable value */
                                        var_status = '/';
                                        p5 = g_strndup (p4, p3 - p4);
                                        g_array_append_val (configure->lines, p5);
                                        variable->value = g_strdup (p5);
                                        variable->index = index;
                                        g_array_append_val (configure->variables, variable);
                                        index++;
                                        p4 = p3;
                                } else {
                                        GST_ERROR ("line %d, position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return 1;
                                }
                                break;
                        case '>':
                                if (var_status == '<') {
                                        /* open variable define */
                                        var_status = '>';
                                        p5 = g_strndup (p4, p3 - p4 + 1);
                                        g_array_append_val (configure->lines, p5);
                                        index++;
                                        p4 = p3 + 1;
                                } else if (var_status == '/') {
                                        /* close variable define */
                                        var_status = 'v';
                                } else {
                                        GST_ERROR ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return 1;
                                }
                                break;
                        }

                        if (p3 == p2) {
                                /* complete one line processing. */
                                break;
                        }
                        p3++;
                }
                p2++;
                p1 = p3 = p2;
        }

        g_free (group);
        g_free (p);
        #if 0
        for (i = 0; i < configure->lines->len; i++) {
                g_print ("%s\n", g_array_index (configure->lines, gchar *, i));
        }
        for (i = 0; i < configure->variables->len; i++) {
                variable = g_array_index (configure->variables, gpointer, i);
                g_print ("name: %s; group: %s; index: %d; value: %s\n", variable->name, variable->group, variable->index, variable->value);
        }
        #endif
        return 0;
}

static gint
configure_release_lines (GArray *lines)
{
        gint i;
        gchar *line;

        for (i = lines->len - 1; i >= 0; i--) {
                line = g_array_index (lines, gpointer, i);
                g_free (line);
                g_array_remove_index (lines, i);
        }

        return 0;
}

static gint
configure_release_variables (GArray *variables)
{
        ConfigurableVar *conf_var;
        gint i;

        for (i = variables->len - 1; i >= 0; i--) {
                conf_var = g_array_index (variables, gpointer, i);
                g_free (conf_var->group);
                g_free (conf_var->name);
                g_free (conf_var->type);
                g_free (conf_var->value);
                g_array_remove_index (variables, i);
                g_free (conf_var);
        }

        return 0;
}

/*
 * if configure_load_from_file is reload, release resources first.
 */
static gint
configure_reset (Configure *configure)
{
        if (configure->raw != NULL) {
                g_free (configure->raw);
        }

        if (configure->data != NULL) {
                gst_structure_free (configure->data);
        }
        configure->data = gst_structure_empty_new ("configure");

        configure_release_lines (configure->lines);
        configure_release_variables (configure->variables);
}

/*
 * load configure file.
 */
gint
configure_load_from_file (Configure *configure)
{
        GError *e = NULL;
        gint ret;

        configure_reset (configure);

        /* load file */
        if (!g_file_get_contents (configure->configure_file, &configure->raw, &configure->size, &e)) {
                GST_ERROR ("Error: %s\n", e->message);
                g_error_free (e);
                return 1;
        }

        /* extract line for management */
        ret = configure_extract_lines (configure);
        if (ret != 0) {
                return ret;
        }

        /* parse file, for gstreamer pipeline creating */
        ret = configure_file_parse (configure);

        return ret;
}

/*
 * save to configure file after variable changed.
 */
gint
configure_save_to_file (Configure *configure)
{
        gint i;
        gchar *line, *contents, *p;
        GError *e = NULL;

        p = g_strdup_printf ("");
        for (i = 0; i < configure->lines->len; i++) {
                line = g_array_index (configure->lines, gchar *, i);
                contents = g_strdup_printf ("%s%s", p, line);
                g_free (p);
                p = contents;
        }

        if (g_file_set_contents (configure->configure_file, contents, strlen (contents), &e) == FALSE) {
                GST_ERROR ("error: %s\n", e->message);
                g_error_free (e);
                g_free (contents);
                return 1;
        }

        g_free (contents);
        return 0;
}

static gchar*
group_alter (gchar *path, gchar *group)
{
        gchar *tag_close, *tag_open, *tag, *p1, *p2, *p3, *p4, *p5;
        gint i, j, indent, depth;
        GRegex *regex;

        p1 = p3 = path;
        p2 = p4 = group;

        /* find out identical part of path and group */
        indent = 1;
        for (;;) {
                if (((*p3 == '/') || (*p3 == '\0')) && ((*p4 == '/') || (*p4 == '\0'))) {
                        indent++;
                        if (*p3 == '\0') {
                                p1 = p3;
                        } else {
                                p1 = p3 + 1;
                        }
                        if (*p4 == '\0') {
                                p2 = p4;
                        } else {
                                p2 = p4 + 1;
                        }
                }
                if ((*p3 == '\0') || (*p4 == '\0') || (*p3 != *p4)) {

                        break;
                }
                p3++;
                p4++;
        }

        /* close tag */
        tag_close = g_strdup ("");
        if (*p1 != '\0') {
                depth = indent;
                p3 = p4 = g_strdup ("</");
                for (;;) {
                        if ((*p1 == '/') || (*p1 == '\0')) {
                                for (i = 0; i < depth; i++) {
                                        p4 = g_strdup_printf ("    %s", p3);
                                        g_free (p3);
                                        p3 = p4;
                                }
                                p4 = g_strdup_printf ("%s>\n", p3);
                                g_free (p3);
                                p3 = tag_close;
                                tag_close = g_strdup_printf ("%s%s", p4, p3);
                                g_free (p3);
                                g_free (p4);
                                p3 = p4 = g_strdup ("</");
                                depth++;
                        } else {
                                p4 = g_strdup_printf ("%s%c", p3, *p1);
                                g_free (p3);
                                p3 = p4;
                        }
                        if (*p1 == '\0') {
                                g_free (p3);
                                break;
                        }
                        p1++;
                }
        }

        /* open tag */
        tag_open = g_strdup ("");
        if (*p2 != '\0') {
                depth = indent;
                p3 = p4 = g_strdup ("<");
                for (;;) {
                        if ((*p2 == '/') || (*p2 == '\0')) {
                                for (i = 0; i < depth; i++) {
                                        p4 = g_strdup_printf ("    %s", p3);
                                        g_free (p3);
                                        p3 = p4;
                                }
                                p4 = g_strdup_printf ("%s>\n", p3);
                                g_free (p3);
                                p3 = tag_open;
                                tag_open = g_strdup_printf ("%s%s", p3, p4);
                                g_free (p3);
                                g_free (p4);
                                p3 = p4 = g_strdup ("<");
                                depth++;
                        } else {
                                p4 = g_strdup_printf ("%s%c", p3, *p2);
                                g_free (p3);
                                p3 = p4;
                        }
                        if (*p2 == '\0') {
                                g_free (p3);
                                break;
                        }
                        p2++;
                }
        }

        /* channel tag <channel name="thename"> */
        regex = g_regex_new (".*channels/([^/]*).*", G_REGEX_DOTALL, 0, NULL);
        p1 = NULL;
        p1 = g_regex_replace (regex, group, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        if ((p1 != NULL) && (g_strcmp0 (p1, group) != 0)) {
                p2 = g_strdup_printf ("(.*<)%s(>.*)", p1);
                regex = g_regex_new (p2, G_REGEX_DOTALL, 0, NULL);
                p3 = g_strdup_printf ("\\1channel name=\"%s\"\\2", p1);
                p4 = g_regex_replace (regex, tag_open, -1, 0, p3, 0, NULL);
                g_free (p2);
                g_free (p3);
                p2 = tag_open;
                tag_open = p4;
                g_free (p2);
        }
        g_free (p1);

        /* channel tag </channel> */
        regex = g_regex_new (".*channels/([^/]*).*", G_REGEX_DOTALL, 0, NULL);
        p1 = NULL;
        p1 = g_regex_replace (regex, path, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        if ((p1 != NULL) && (g_strcmp0 (p1, path) != 0)) {
                p2 = g_strdup_printf (".*channels/(%s).*", p1);
                regex = g_regex_new (p2, G_REGEX_DOTALL, 0, NULL);
                p3 = NULL;
                p3 = g_regex_replace (regex, group, -1, 0, "\\1", 0, NULL);
                g_regex_unref (regex);
                if ((p3 != NULL) && (g_strcmp0 (group, p3) == 0)) {
                        p4 = g_strdup_printf ("(.*)(</%s>)(.*)", p1);
                        regex = g_regex_new (p4, G_REGEX_DOTALL, 0, NULL);
                        p5 = g_regex_replace (regex, tag_close, -1, 0, "\\1</channel>\\3", 0, NULL);
                        g_regex_unref (regex);
                        g_free (tag_close);
                        tag_close = p5;
                        g_free (p4);
                }
                g_free (p2);
                g_free (p3);
        }
        g_free (p1);

        /* encoder tag <encoder name="thename"> */
        regex = g_regex_new (".*encoders/([^/]*)$", G_REGEX_DOTALL, 0, NULL);
        p1 = NULL;
        p1 = g_regex_replace (regex, group, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        if ((p1 != NULL) && (g_strcmp0 (p1, group) != 0)) {
                p2 = g_strdup_printf ("(.*<)%s(>.*)", p1);
                regex = g_regex_new (p2, G_REGEX_DOTALL, 0, NULL);
                p3 = g_strdup_printf ("\\1encoder name=\"%s\"\\2", p1);
                p4 = g_regex_replace (regex, tag_open, -1, 0, p3, 0, NULL);
                g_free (p2);
                g_free (p3);
                p2 = tag_open;
                tag_open = p4;
                g_free (p2);
        }
        g_free (p1);

        /* encoder tag </encoder> */
        regex = g_regex_new (".*encoders/([^/]*).*", G_REGEX_DOTALL, 0, NULL);
        p1 = NULL;
        p1 = g_regex_replace (regex, path, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        if ((p1 != NULL) && (g_strcmp0 (p1, path) != 0)) {
                p2 = g_strdup_printf (".*encoders/(%s).*", p1);
                regex = g_regex_new (p2, G_REGEX_DOTALL, 0, NULL);
                p3 = NULL;
                p3 = g_regex_replace (regex, group, -1, 0, "\\1", 0, NULL);
                g_regex_unref (regex);
                if ((p3 != NULL) && (g_strcmp0 (group, p3) == 0)) {
                        p4 = g_strdup_printf ("(.*)(</%s>)(.*)", p1);
                        regex = g_regex_new (p4, G_REGEX_DOTALL, 0, NULL);
                        p5 = g_regex_replace (regex, tag_close, -1, 0, "\\1</encoder>\\3", 0, NULL);
                        g_regex_unref (regex);
                        g_free (tag_close);
                        tag_close = p5;
                        g_free (p4);
                }
                g_free (p2);
                g_free (p3);
        }
        g_free (p1);

        tag = g_strdup_printf ("%s%s", tag_close, tag_open);
        g_free (tag_close);
        g_free (tag_open);

        return tag;
}

static gchar *
add_indent (gchar *var, gint indent)
{
        gint i;
        gchar *p1, *p2;
        
        p1 = var;
        for (i = 0; i <= indent; i++) {
                p2 = g_strdup_printf ("%s    ", p1);
                g_free (p1);
                p1 = p2;
        }
        p2 = g_strdup_printf ("%s    ", p1);
        g_free (p1);

        return p2;
}

/*
 * get configurable item, managment interface.
 */
gchar*
configure_get_var (Configure *configure, gchar *group)
{
        gint i, j, indent;
        gchar *var, *p1, *path, *tag;
        ConfigurableVar *conf_var;
        
        p1 = g_strdup ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
        path = g_strdup ("");
        var = NULL;
        for (i = 0; i < configure->variables->len; i++) {
                conf_var = g_array_index (configure->variables, gpointer, i);
                if (g_ascii_strncasecmp (conf_var->group, group, strlen (group)) == 0) {
                        if (g_strcmp0 (conf_var->group, path) != 0) {
                                /* group alternation, close and open another tag. */
                                tag = group_alter (path, conf_var->group);
                                var = g_strdup_printf ("%s%s", p1, tag);
                                g_free (p1);
                                p1 = var;
                                g_free (tag);
                                g_free (path);
                                indent = 1;
                                path = g_strdup_printf ("%s", conf_var->group);
                                for (j = 0; j < strlen (conf_var->group); j++) {
                                        if (conf_var->group[j] == '/') {
                                                indent++;
                                        }
                                }
                        }

                        var = add_indent (p1, indent - 1);
                        p1 = var;
                        var = g_strdup_printf ("%s<var name=\"%s\" id=\"%d\" type=\"%s\">%s</var>\n", p1, conf_var->name, conf_var->index, conf_var->type, conf_var->value);
                        g_free (p1);
                        p1 = var;
                } 
        }

        tag = group_alter (path, "");
        if (var != NULL) {
                var = g_strdup_printf ("%s%s</root>\n", var, tag);
        }
        g_free (p1);
        g_free (tag);
        g_free (path);

        return var;
}

static void
start_element (GMarkupParseContext *context, const gchar *element, const gchar **attr_names, const gchar **attr_vals, gpointer data, GError **e)
{
        ConfigurableVar *conf_var;
        GArray *var_array;
        const gchar **names, **vals;

        var_array = (GArray *)data;
        if (g_strcmp0 (element, "var") == 0) {
                conf_var = (ConfigurableVar *) g_malloc (sizeof (ConfigurableVar));
                names = attr_names;
                vals = attr_vals;
                while (*names != NULL) {
                        if (g_strcmp0 (*names, "id") == 0) {
                                conf_var->index = atoi (*vals);
                                break;
                        }
                        names++;
                        vals++;
                }
                g_array_append_val (var_array, conf_var);
        }
}

static void
end_element (GMarkupParseContext *context, const gchar *element, gpointer data, GError **e)
{
        if (g_strcmp0 (element, "var") == 0) {
                //g_print ("end: %s\n\n", element);
        }
}

static void
text (GMarkupParseContext *context, const char *text, gsize length, gpointer data, GError **e)
{
        gchar *element;
        ConfigurableVar *conf_var;
        GArray *var_array;
        gint index;

        var_array = (GArray *)data;
        if (var_array->len == 0) {
                return;
        }
        index = var_array->len - 1;
        conf_var = g_array_index (var_array, gpointer, index);
        element = (gchar *)g_markup_parse_context_get_element (context);
        if (g_strcmp0 (element, "var") == 0) {
                conf_var->value = g_strdup (text);
        }
}

static gint
set_var (Configure *configure, ConfigurableVar *conf_var)
{
        gchar *line;

        line = g_array_index (configure->lines, gchar *, conf_var->index);
        g_free (line);
        g_array_remove_index (configure->lines, conf_var->index);
        g_array_insert_val (configure->lines, conf_var->index, conf_var->value);

        line = g_array_index (configure->lines, gchar *, conf_var->index);
        //g_print (", after set: %s\n", line);

        return 0;
}

gint
configure_set_var (Configure *configure, gchar *var)
{
        GMarkupParser parser = {
                start_element,
                end_element,
                text,
                NULL,
                NULL
        };
        GMarkupParseContext *context;
        GError *e = NULL;
        GArray *var_array;
        ConfigurableVar *conf_var;
        gint i;

        var_array = g_array_new (FALSE, FALSE, sizeof (gpointer));
        context = g_markup_parse_context_new (&parser, 0, var_array, NULL);

        if (!g_markup_parse_context_parse (context, var, -1, &e)) {
                g_array_free (var_array, TRUE);
                g_markup_parse_context_free (context);
                GST_ERROR ("parse error\n");
                return 1;
        }

        for (i = 0; i < var_array->len; i++) {
                conf_var = g_array_index (var_array, gpointer, i);
                //g_print ("id: %d value: %s", conf_var->index, conf_var->value);
                set_var (configure, conf_var);
        }

        for (i = var_array->len - 1; i >= 0; i--) {
                conf_var = g_array_index (var_array, gpointer, i);
                g_array_remove_index (var_array, i);
                g_free (conf_var);
        }

        g_array_free (var_array, TRUE);
        g_markup_parse_context_free (context);

        return 0;
}

/*
 * get configuration parameter which used to construct encode channels.
 */
GValue *
configure_get_param (Configure *configure, gchar *param)
{
        GValue *value;
        GstStructure *structure;
        gchar *key, *p1, *p2;

        if (param[0] != '/') {
                /* must be absolute path */
                return NULL;
        }

        /* locate param */
        p2 = p1 = param + 1;
        structure = configure->data;
        for (;;) {
                p2++;
                if ((*p2 == '/') || (*p2 == '\0')) {
                        key = g_strndup (p1, p2 - p1);
                        p2++;
                        p1 = p2;
                        value = (GValue *)gst_structure_get_value (structure, key);
                        g_free (key);
                        if (value == NULL) {
                                return NULL;
                        }
                        if ((p2 - param) >= strlen (param)) {
                                break;
                        } else {
                                structure = (GstStructure *)gst_value_get_structure (value);
                        }
                }
        }

        return value;
}

#if 0
gint
main (gint argc, gchar *argv[])
{
        Configure *configure;
        GValue *value;
        GstStructure *structure;
        gchar *var, *str;
        GstElement *element, *pipeline, *appsink;
        GMainLoop *loop;
        Graph *graph;

        gst_init (&argc, &argv);

        for (;;) {
                //break;

                configure = configure_new ("configure_path", "configure.conf", NULL);
                if (configure_load_from_file (configure) != 0) {
                        g_print ("exit ...\n");
                        return 1;
                }

                var = configure_get_var (configure, "");
                g_print ("configure:\n%s", var);
                configure_set_var (configure, var);
                g_free (var);
                configure_save_to_file (configure);

                value = configure_get_param (configure, "/server/httpstreaming");
                g_print ("pipeline: %s\n", g_value_get_string (value));

                value = configure_get_param (configure, "/server/httpmgmt");
                g_print ("pipeline: %s\n", g_value_get_string (value));

                value = configure_get_param (configure, "/channel/test/source/elements/textoverlay");
                structure = (GstStructure *)gst_value_get_structure (value);
                str = gst_structure_to_string (structure);
                g_print ("textoverlay: %s\n", str);
                g_free (str);

                value = configure_get_param (configure, "/channel/test/source/bins/videosrc/definition");
                g_print ("videosource: %s\n", g_value_get_string (value));

                value = configure_get_param (configure, "/channel/mpegtsoverip/source/elements/textoverlay/option");
                g_print ("textoverlay option: %s\n", g_value_get_string (value));

                value = configure_get_param (configure, "/channel");
                structure = (GstStructure *)gst_value_get_structure (value);
                str = gst_structure_to_string (structure);
                g_free (str);

                value = configure_get_param (configure, "/channel/mpegtsoverip/encoder/encoder1/elements/x264enc/property/name");
                g_print ("encoder1: %s\n", g_value_get_string (value));

                value = (GValue *)configure_get_param (configure, "/channel/mpegtsoverip/source");
                structure = (GstStructure *)gst_value_get_structure (value);
                graph = get_pipeline_graph (structure);
                pipeline = create_pipeline (graph);
                if (pipeline == NULL) {
                        return 1;
                }
                gst_element_set_state (pipeline, GST_STATE_PLAYING);
                sleep (5);
                gst_element_set_state (pipeline, GST_STATE_NULL);

                //loop = g_main_loop_new (NULL, FALSE);
                //g_main_loop_run (loop);

                gst_object_unref (G_OBJECT (pipeline));
                gst_object_unref (G_OBJECT (configure));

                //break;
        }
}
#endif
