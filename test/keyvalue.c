#include <unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "keyvalue.h"

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
                CONFIGURE(obj)->file_path = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_path?
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
                g_value_set_string(value, configure->file_path);
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

        if (configure->file_path != NULL) {
                g_free (configure->file_path);
                configure->file_path = NULL;
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
configure_caps_parse (gchar *name, gchar *data)
{
        GKeyFile *gkeyfile;
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;
        GstStructure *structure;
        GValue value = { 0, { { 0 } } };

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s element parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
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
                if (!g_ascii_isalnum (*p)) {
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
        GstStructure *structure, *property, *caps;
        GValue value = { 0, { { 0 } } };
        GRegex *regex;

        if (!is_valid_name (name)) {
                g_print ("Invalid element configure: %s\n", name);
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
                        caps = configure_property_parse (p[i], v);
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, caps, NULL);
                        gst_structure_free (caps);
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
                        g_print ("invalid element configuration\n");
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
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;
        GstStructure *structure, *bin;
        GValue value = { 0, { { 0 } } };

        if (!is_valid_name (name)) {
                g_print ("Invalid bin configure: %s\n", name);
                return NULL;
        }
        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        //g_print ("\n\n\n%s parse, number is %d\n", name, number);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                if (!is_valid_name (p[i])) {
                        g_print ("Invalid bin configure: %s\n", p[i]);
                        return NULL;
                }
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
                } else if (v[0] == '<') {
                        /* variable found */
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
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;
        GstStructure *structure, *source, *encoder;
        GValue value = { 0, { { 0 } } };

        gkeyfile = ini_data_parse (name, data);
        p = g_key_file_get_keys (gkeyfile, name, &number, &e);
        structure = gst_structure_empty_new (name);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, name, p[i], &e);
                if (g_strcmp0 (p[i], "onboot") == 0) {
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
                } else if (g_strcmp0 (p[i], "encoder") == 0) {
                        encoder = configure_encoder_parse (p[i], v);
                        if (encoder == NULL) {
                                return NULL;
                        }
                        gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, encoder, NULL);
                        gst_structure_free (encoder);
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
        GstStructure *structure, *channel;
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
                        g_print ("Invalid server configure: %s\n", p[i]);
                        return 1;
                }
                g_value_unset (&value);
                g_free (v);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "server", GST_TYPE_STRUCTURE, structure, NULL);
        gst_structure_free (structure);

        /* parse channel group */
        p = g_key_file_get_keys (gkeyfile, "channel", &number, &e);
        structure = gst_structure_empty_new ("channel");
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (gkeyfile, "channel", p[i], &e);
                if (!is_valid_name (p[i])) {
                        g_print ("Invalid channel configure: %s\n", p[i]);
                        return 1;
                }
                channel = configure_channel_parse (p[i], v);
                if (channel == NULL) {
                        return 1;
                }
                gst_structure_set (structure, p[i], GST_TYPE_STRUCTURE, channel, NULL);
                g_free (v);
                gst_structure_free (channel);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "channel", GST_TYPE_STRUCTURE, structure, NULL);
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
                                        g_print ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return 1;
                                }
                                break;
                        case '[':
                                if (var_status == '\0') {
                                        if (g_ascii_strncasecmp (p3, "[server]", 8) == 0) {
                                                g_free (group);
                                                group = g_strdup ("server");
                                        } else if (g_ascii_strncasecmp (p3, "[channel]", 9) == 0) {
                                                g_free (group);
                                                group = g_strdup ("channel");
                                        }
                                }
                                break;
                        case '{':
                                bracket_depth++;
                                group_array = g_strsplit (group, ".", 5);
                                if (g_ascii_strncasecmp (group, "channel", 7) == 0) {
                                        if ((bracket_depth <= 2) || ((bracket_depth == 3) && (g_strcmp0 (group_array[2], "encoder") == 0))) {
                                                regex = g_regex_new ("( *)([^ ]*)( *= *{.*)", G_REGEX_DOTALL, 0, NULL);
                                                p5 = g_regex_replace (regex, p1, -1, 0, "\\2", 0, NULL);
                                                g_regex_unref (regex);
                                                p6 = g_strdup_printf ("%s.%s", group, p5);
                                                g_free (group);
                                                g_free (p5);
                                                group = p6;
                                        }
                                }
                                g_strfreev (group_array);
                                break;
                        case '}':
                                bracket_depth--;
                                group_array = g_strsplit (group, ".", 5);
                                if (g_ascii_strncasecmp (group, "channel", 7) == 0) {
                                        if ((bracket_depth == 1) || ((bracket_depth == 2) && (g_strcmp0 (group_array[2], "encoder") == 0))) {
                                                i = 0;
                                                while (group_array[i] != NULL) {
                                                        i++;
                                                }
                                                i -= 2;
                                                p5 = g_strdup (group_array[i]);
                                                p6 = p5;
                                                while (i != 0) {
                                                        p5 = g_strdup_printf ("%s.%s", group_array[i - 1], p6);
                                                        g_free (p6);
                                                        p6 = p5;
                                                        i--;
                                                }
                                                g_free (group);
                                                group = p5;
                                        } else if (bracket_depth == 0) {
                                                g_free (group);
                                                group = g_strdup ("channel");
                                        }
                                }
                                g_strfreev (group_array);
                                break;
                        case '<':
                                /* find a variable */
                                if ((var_status == '\0') || (var_status == 'v')) {
                                        var_status = '<';
                                        variable = g_malloc (sizeof (ConfigurableVar));
                                        regex = g_regex_new ("<var[^>]*type *= *\\x22([^\\x22]*)\\x22.*", G_REGEX_DOTALL, 0, NULL);
                                        p5 = g_regex_replace (regex, p3, -1, 0, "\\1", 0, NULL);
                                        g_regex_unref (regex);
                                        variable->type = p5;
                                        regex = g_regex_new ("<var[^>]*name *= *\\x22([^\\x22]*)\\x22.*", G_REGEX_DOTALL, 0, NULL);
                                        p5 = g_regex_replace (regex, p3, -1, 0, "\\1", 0, NULL);
                                        g_regex_unref (regex);
                                        variable->name = p5;
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
                                        g_print ("line %d, position %d: %s, parse error\n", line_number, p3 - p1, p1);
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
                                        g_print ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
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
        g_file_get_contents ("configure.conf", &configure->raw, &configure->size, &e);

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

        if (g_file_set_contents ("xxxx.xxx", contents, strlen (contents), &e) == FALSE) {
                g_error (e->message);
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
        gchar *tag_close, *tag_open, *tag, *p1, *p2, *p3, *p4;
        gint i, j, indent, depth;

        p1 = p3 = path;
        p2 = p4 = group;

        /* find out identical part of path and group */
        indent = 1;
        for (;;) {
                if (((*p3 == '.') || (*p3 == '\0')) && ((*p4 == '.') || (*p4 == '\0'))) {
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
                        if ((*p1 == '.') || (*p1 == '\0')) {
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
                        if ((*p2 == '.') || (*p2 == '\0')) {
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
        gchar *var, *p1, *p2, *path, *tag;
        ConfigurableVar *conf_var;
        
        p1 = g_strdup ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<root>\n");
        path = g_strdup ("");
        for (i = 0; i < configure->variables->len; i++) {
                conf_var = g_array_index (configure->variables, gpointer, i);
                if (g_ascii_strncasecmp (conf_var->group, group, strlen (group)) == 0) {
                        if (g_strcmp0 (conf_var->group, path) != 0) {
                                /* group alternation */
                                tag = group_alter (path, conf_var->group);
                                var = g_strdup_printf ("%s%s", p1, tag);
                                g_free (p1);
                                p1 = var;
                                g_free (tag);
                                g_free (path);
                                indent = 1;
                                path = g_strdup_printf ("%s", conf_var->group);
                                for (j = 0; j < strlen (conf_var->group); j++) {
                                        if (conf_var->group[j] == '.') {
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
        var = g_strdup_printf ("%s%s</root>\n", var, tag);
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
                g_print ("parse error\n");
                return 1;
        }

        for (i = 0; i < var_array->len; i++) {
                conf_var = g_array_index (var_array, gpointer, i);
                //g_print ("id: %d value: %s", conf_var->index, conf_var->value);
                g_strreverse (conf_var->value);
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

/*************************************/

static gboolean
is_selected_element (Configure *configure, gchar *param, gchar *element)
{
        GValue *value;
        gchar *p;

        p = g_strdup_printf ("%s/elements/%s/option", param, element);
        value = configure_get_param (configure, p);
        g_free (p);
        if (value == NULL) {
                return TRUE;
        }
        p = (gchar *)g_value_get_string (value);
        if (g_strcmp0(p, "yes") == 0) {
                return TRUE;
        } else {
                return FALSE;
        }
}

typedef struct _Link {
        GstElement *src;
        GstElement *sink;
        gchar *src_name;
        gchar *sink_name;
        gchar *src_pad_name;
        gchar *sink_pad_name;
} Link;

typedef struct _DelayedLink {
        GstElement *sink;
        gchar *src_pad_name;
        gchar *sink_pad_name;
        GstCaps *caps;
        gulong signal_id;
} DelayedLink;

typedef struct _Chain {
        GSList *elements;
        GSList *links;
} Chain;

/**
 * create_element
 * @configure: configure object.
 * @param: like this: /server/httpstreaming
 *
 * create element, based on GstStructure type element info.
 *
 * Returns: the new created element or NULL.
 */
GstElement *
create_element (Configure *configure, gchar *param)
{
        GstElement *element;
        GValue *value;
        gint n, i;
        gchar *factory, *name, *p;
        GstStructure *structure, *property;
        GParamSpec *param_spec;
        GRegex *regex;

        /* extract factory */
        regex = g_regex_new ("([^ ]*).*", 0, 0, NULL);
        p = g_regex_replace (regex, param, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);
        regex = g_regex_new (".*/(.*)", 0, 0, NULL);
        factory = g_regex_replace (regex, p, -1, 0, "\\1", 0, NULL);
        g_regex_unref (regex);

        /* extract property if have FIXME: now only name property. */
        if (g_strcmp0 (p, param) != 0) {
                /* (name=xxx) found */
                regex = g_regex_new (".* name *= *([^ ]*)", 0, 0, NULL);
                name = g_regex_replace (regex, param, -1, 0, "\\1", 0, NULL);
                g_regex_unref (regex);
        } else {
                name = NULL;
        }
        g_free (p);

        /* extract element configure. */
        regex = g_regex_new (" .*", 0, 0, NULL);
        p = g_regex_replace (regex, param, -1, 0, "", 0, NULL);
        g_regex_unref (regex);
        //g_print ("create element, param: %s, fatory: %s name: %s conf path: %s\n", param, factory, name, p);
        value = (GValue *)configure_get_param (configure, p);
        element = gst_element_factory_make (factory, name);
        g_free (p);
        g_free (factory);
        if (name != factory) {
                g_free (name);
        }
        if (value == NULL) {
                /* no property configured. */
                return element;
        }

        /* set propertys if have configured any */
        structure = (GstStructure *)gst_value_get_structure (value);
        if (gst_structure_has_field (structure, "property")) {
                value = (GValue *)gst_structure_get_value (structure, "property");
                if (!GST_VALUE_HOLDS_STRUCTURE (value)) {
                        g_print ("elements property should be structure.\n");
                        gst_object_unref (GST_OBJECT (element));
                        return NULL;
                }
                property = (GstStructure *)gst_value_get_structure (value);
                n = gst_structure_n_fields (property);
                for (i = 0; i < n; i++) {
                        name = (gchar *)gst_structure_nth_field_name (property, i);
                        param_spec = g_object_class_find_property (G_OBJECT_GET_CLASS (element), name);
                        if (param_spec == NULL) {
                                g_print ("Can't find property name: %s\n", name);
                                return NULL;
                        }
                        p = (gchar *)gst_structure_get_string (property, name);
                        switch (param_spec->value_type) {
                        case G_TYPE_STRING:
                                //g_print ("set property, name: %s, p: %s\n", name, p);
                                g_object_set (element, name, p, NULL);
                                break;
                        case G_TYPE_INT:
                                g_object_set (element, name, atoi (p), NULL);
                                break;
                        case G_TYPE_BOOLEAN:
                                if (g_strcmp0 (p, "FALSE") == 0) {
                                        g_object_set (element, name, FALSE, NULL);
                                } else if (g_strcmp0 (p, "TRUE") == 0) {
                                        g_object_set (element, name, TRUE, NULL);
                                } else {
                                        g_print ("wrong configure %s\n", p); //FIXME
                                }
                                break;
                        }
                }
        }

        return element;
}

static void
pad_added_cb (GstElement *element, GstPad *pad, gpointer data)
{
        gchar *src_pad_name;
        DelayedLink *delayedlink = (DelayedLink *)data;

        src_pad_name = gst_pad_get_name (pad);
        if (g_strcmp0 (src_pad_name, delayedlink->src_pad_name) != 0) {
                g_print ("new added pad name: %s, delayed src pad name %s.\n", src_pad_name, delayedlink->src_pad_name);
                return;
        }
        delayedlink->caps = gst_pad_get_caps (pad);
        if (gst_element_link_pads_filtered (element, delayedlink->src_pad_name, delayedlink->sink, NULL, delayedlink->caps)) {
                g_print ("new added pad name: %s, delayed src pad name %s. ok!\n", src_pad_name, delayedlink->src_pad_name);
                g_signal_handler_disconnect (element, delayedlink->signal_id);
        }
        g_free (src_pad_name);
}

static void
free_delayed_link (DelayedLink *link)
{
        if (link->caps) {
                gst_caps_unref (link->caps);
        }
        g_free (link->src_pad_name);
        if (link->sink_pad_name) {
                g_free (link->sink_pad_name);
        }
        g_slice_free (DelayedLink, link);
}

/**
 * create_pipeline
 * @configure: Configure object.
 * @param: like this: /server/httpstreaming
 *
 * Returns: the cteated pipeline or NULL.
 */
GstElement *
create_pipeline (Configure *configure, gchar *param)
{
        GValue *value;
        GstStructure *structure;
        GstElement *pipeline, *element, *src;
        gchar *name, *p, *p1, **pp, **pp1, *src_name, *src_pad_name;
        gint i, n;
        Chain chain;
        Link *link;
        DelayedLink *delayedlink;
        GSList *links, *elements;

        /* pipeline */
        value = configure_get_param (configure, param);
        structure = (GstStructure *)gst_value_get_structure (value);
        name = (gchar *)gst_structure_get_name (structure);
        //g_print ("name: %s\n", name);

        /* bin */
        chain.links = NULL;
        chain.elements = NULL;
        p = g_strdup_printf ("%s/bins", param);
        value = configure_get_param (configure, p);
        g_free (p);
        structure = (GstStructure *)gst_value_get_structure (value);
        n = gst_structure_n_fields (structure);
        for (i = 0; i < n; i++) {
                name = (gchar *)gst_structure_nth_field_name (structure, i);
                //bin = gst_bin_new (name);
                p = g_strdup_printf ("%s/bins/%s/definition", param, name);
                value = configure_get_param (configure, p);
                g_free (p);
                //g_print ("%s: %s\n", name, g_value_get_string (value));
                p = (gchar *)g_value_get_string (value);
                //g_print ("p: %s\n", p);
                pp = pp1 = g_strsplit (p, "!", 0);
                src = NULL;
                src_name = NULL;
                src_pad_name = NULL;
                while (*pp != NULL) {
                        p1 = g_strdup (*pp);
                        p1 = g_strstrip (p1);
                        if (g_strrstr (p1, ".") != NULL) {
                                if (src == NULL) {
                                        /* should be a sometimes pad */
                                        src_name = g_strndup (p1, g_strrstr (p1, ".") - p1);
                                        src_pad_name = g_strndup (g_strrstr (p1, ".") + 1, strlen (p1) - strlen (src_name) -1);
                                        g_print ("src_name: %s, src_pad_name: %s\n", src_name, src_pad_name);
                                } else {
                                        /* should be a request pad */
                                        link = g_slice_new (Link);
                                        link->src = src;
                                        link->src_name = src_name;
                                        link->src_pad_name = src_pad_name;
                                        link->sink = NULL;
                                        link->sink_name = g_strndup (p1, g_strrstr (p1, ".") - p1);
                                        link->sink_pad_name = g_strndup (g_strrstr (p1, ".") + 1, strlen (p1) - strlen (link->sink_name) -1);
                                        chain.links = g_slist_append (chain.links, link);
                                }
                        } else if (is_selected_element (configure, param, p1)) {
                                /* plugin name, create a element. */
                                p = g_strdup_printf ("%s/elements/%s", param, p1);
                                element = create_element (configure, p);
                                if (element != NULL) {
                                        if (src_name != NULL) {
                                                link = g_slice_new (Link);
                                                link->src = src;
                                                link->src_name = src_name;
                                                link->src_pad_name = src_pad_name;
                                                link->sink = element;
                                                link->sink_name = p1;
                                                link->sink_pad_name = NULL;
                                                chain.links = g_slist_append (chain.links, link);
                                        }
                                        chain.elements = g_slist_append (chain.elements, element);
                                        src = element;
                                        src_name = p1;
                                        g_print ("element_name: %s\n", src_name);
                                        src_pad_name = NULL;
                                } else {
                                        g_print ("error create element %s\n", *pp);
                                        g_free (p);
                                        g_strfreev (pp1);
                                        return NULL; //FIXME release pipeline
                                }
                                g_free (p);
                        } else {
                                g_free (p1);
                        }
                        pp++;
                }
                g_strfreev (pp1);
        }

        pipeline = gst_pipeline_new (name);

        /* add element to pipeline */
        elements = chain.elements;
        while (elements != NULL) {
                element = elements->data;
                gst_bin_add (GST_BIN (pipeline), element);
                elements = g_slist_next (elements);
        }
        
        /* links element */
        links = chain.links;
        while (links != NULL) {
                link = links->data;
                if (link->src_pad_name != NULL) {
                        /* src pad is sometimes pad, delay link */
                        g_print ("sometimes pad, delayedlink: %s\n", link->src_pad_name);
                        delayedlink = g_slice_new (DelayedLink);
                        /* find src */
                        elements = chain.elements;
                        while (elements != NULL) {
                                element = elements->data;
                                if (g_strcmp0(gst_element_get_name (element), link->src_name) == 0) {
                                        break;
                                }
                                elements = g_slist_next (elements);
                        }
                        delayedlink->src_pad_name = g_strdup (link->src_pad_name);
                        delayedlink->sink = link->sink;
                        delayedlink->sink_pad_name = g_strdup (link->sink_pad_name);
                        delayedlink->signal_id = g_signal_connect_data (element, "pad-added", G_CALLBACK (pad_added_cb), delayedlink, (GClosureNotify)free_delayed_link, (GConnectFlags) 0);
                } else {
                        /* link directly */
                        g_print ("link %s and %s directly\n", link->src_name, link->sink_name);
                        gst_element_link (link->src, link->sink);
                }
                links = g_slist_next (links);
        }

        return pipeline;
}

/***************************************/

static GstFlowReturn
source_appsink_callback (GstAppSink *elt, gpointer user_data)
{
        GstBuffer *buffer;

        buffer = gst_app_sink_pull_buffer (GST_APP_SINK (elt));

        /* output running status */
        g_print ("buffer size %d, buffer duration %lld\n", GST_BUFFER_SIZE (buffer), GST_BUFFER_DURATION(buffer));

        gst_buffer_unref (buffer);
}

gint
main (gint argc, gchar *argv[])
{
        Configure *configure;
        GValue *value;
        GstStructure *structure;
        gchar *var, *str;
        GstElement *element, *pipeline, *appsink;
        GMainLoop *loop;
        GstAppSinkCallbacks appsink_callbacks = {
                NULL,
                NULL,
                source_appsink_callback,
                NULL
        };

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

                element = create_element (configure, "/channel/test/source/elements/textoverlay");
                gst_object_unref (GST_OBJECT (element));

                //pipeline = create_pipeline (configure, "/channel/test/source");
                pipeline = create_pipeline (configure, "/channel/mpegtsoverip/source");
                if (pipeline == NULL) {
                        return 1;
                }
                appsink = gst_bin_get_by_name (GST_BIN (pipeline), "video");
                if (appsink == NULL) {
                        g_print ("Get encoder sink error\n");
                }
                gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &appsink_callbacks, NULL, NULL);
                gst_object_unref (appsink);
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
