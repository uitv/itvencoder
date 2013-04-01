#include <unistd.h>
#include <string.h>
#include <gst/gst.h>

#include "keyvalue.h"

GST_DEBUG_CATEGORY_EXTERN (ITVENCODER);
#define GST_CAT_DEFAULT ITVENCODER

enum {
        CONFIGURE_PROP_0,
        CONFIGURE_PROP_FILE_PATH,
};

static void configure_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec);
static void configure_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec);
static gint configure_extract_lines (Configure *configure);
static gint configure_replace_newline (Configure *configure);
static gint configure_file_parse (Configure *configure);
static gint configure_channel_parse (gchar *name, gchar *element);

static void
configure_class_init (ConfigureClass *configureclass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (configureclass);
        GParamSpec *param;

        g_object_class->set_property = configure_set_property;
        g_object_class->get_property = configure_get_property;

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
        configure->data = gst_structure_empty_new ("configure");
        configure->key_value_data = g_key_file_new ();
}

static void
configure_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        g_return_if_fail (IS_CONFIGURE (obj));

        switch (prop_id) {
        case CONFIGURE_PROP_FILE_PATH:
                CONFIGURE(obj)->configure_file_path = (gchar *)g_value_dup_string (value); //TODO: should release dup string config_path?
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
                g_value_set_string(value, configure->configure_file_path);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
                break;
        }
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

gint
configure_load_from_file (Configure *configure)
{
        GError *e = NULL;

        g_file_get_contents ("configure.conf", &configure->raw, &configure->size, &e);
        configure_extract_lines (configure);

        configure_replace_newline (configure);
        configure_file_parse (configure);
}

static gint
configure_channel_parse (gchar *name, gchar *element)
{
        gchar *p1, *p2, *p3;
        gint right_square_bracket;
        GKeyFile *channel;
        GKeyFileFlags flags;
        GError *e = NULL;
        gchar **p, *v;
        gint i;
        gsize number;

        p1 = g_strdup_printf ("[%s]\n", name);
        p2 = element;
        right_square_bracket = 0;
        for (;;) {
                switch (*p2) {
                case '{':
                        right_square_bracket++;
                        if (right_square_bracket > 1) {
                                p3 = g_strdup_printf ("%s%c", p1, *p2);
                                g_free (p1);
                                p1 = p3;
                        }
                        break;
                case '}':
                        right_square_bracket--;
                        if (right_square_bracket > 0) {
                                p3 = g_strdup_printf ("%s%c", p1, *p2);
                                g_free (p1);
                                p1 = p3;
                        }
                        break;
                case '\\':
                        if ((right_square_bracket <= 1) && (*(p2 + 1) == 'n')) {
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
                if (p2 - element >= strlen (element)) {
                        break;
                }
        }
        *p2 = '\0';

        channel = g_key_file_new ();
        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (channel, p1, strlen(p1), flags, &e)) {
                g_error (e->message);
                return 1;
        }
        p = g_key_file_get_keys (channel, name, &number, &e);
        g_print ("number is %d\n", number);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (channel, name, p[i], &e);
                g_print ("%s - %s\n", p[i], v);
        }
        g_strfreev (p);
}

static gint
configure_file_parse (Configure *configure)
{
        GKeyFileFlags flags;
        GError *e = NULL;
        gsize number;
        gchar **p, *v;
        gint i;
        GstStructure *structure;
        GValue value = { 0, { { 0 } } };

        g_value_init (&value, G_TYPE_STRING);

        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (configure->key_value_data, configure->ini_raw, strlen (configure->ini_raw), flags, &e)) {
                g_error (e->message);
                return 1;
        }

        p = g_key_file_get_keys (configure->key_value_data, "server", &number, &e);
        if (e != NULL) {
                g_error (e->message);
                return 1;
        }
        structure = gst_structure_empty_new ("server");
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (configure->key_value_data, "server", p[i], &e);
                g_value_set_static_string (&value, v);
                gst_structure_set_value (structure, p[i], &value);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "server", GST_TYPE_STRUCTURE, structure, NULL);

        p = g_key_file_get_keys (configure->key_value_data, "channel", &number, &e);
        structure = gst_structure_empty_new ("channel");
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (configure->key_value_data, "channel", p[i], &e);
                configure_channel_parse (p[i], v);
                g_value_set_static_string (&value, v);
                gst_structure_set_value (structure, p[i], &value);
        }
        g_strfreev (p);
        gst_structure_set (configure->data, "channel", GST_TYPE_STRUCTURE, structure, NULL);

        return 0;
}

/*
 * replace newline with \n, so it can be parsed by glib ini style parser.
 */
static gint
configure_replace_newline (Configure *configure)
{
        gchar *p1, *p2, *p3;
        gint right_square_bracket;

        p3 = p1 = g_malloc (configure->size * 2);
        p2 = configure->raw;
        right_square_bracket = 0;
        for (;;) {
                switch (*p2) {
                case '{':
                        right_square_bracket++;
                        *p3 = *p2;
                        break;
                case '}':
                        right_square_bracket--;
                        *p3 = *p2;
                        break;
                case '\n':
                        if (right_square_bracket > 0) {
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
        configure->ini_raw = p3;
        g_print ("%s\n", configure->ini_raw);
        return 0; 
}

static gint
configure_extract_lines (Configure *configure)
{
        gchar *p, *p1, *p2, *p3, *p4, *p5; 
        GRegex *reg;
        GError *e = NULL;
        ConfigurableVar *variable;
        gint i, line_number, index;
        gchar var_status;
        GstStructure *configure_mgmt;

        p = g_strdup_printf ("%s", configure->raw);
        p1 = p2 = p3 = p;
        line_number = 0;
        index = 0;
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
                        case '<':
                                if ((var_status == '\0') || (var_status == 'v')) {
                                        /* find a variable */
                                        var_status = '<';
                                } else if ((var_status == '>') && (*(p3 + 1) == '/')) {
                                        /* variable value */
                                        var_status = '/';
                                        p5 = g_strndup (p4, p3 - p4);
                                        g_array_append_val (configure->lines, p5);
                                        variable = g_malloc (sizeof (ConfigurableVar));
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

        g_free (p);
        for (i = 0; i < configure->lines->len; i++) {
                g_print ("%s\n", g_array_index (configure->lines, gchar *, i));
        }
        for (i = 0; i < configure->variables->len; i++) {
                variable = g_array_index (configure->variables, gpointer, i);
                g_print ("index %d value %s\n", variable->index, variable->value);
        }
        return 0;
}

gint
main (gint argc, gchar *argv[])
{
        Configure *configure;

        gst_init (&argc, &argv);

        configure = configure_new ("configure_path", "configure.conf", NULL);
        configure_load_from_file (configure);

        g_print ("\n\n\nHello, gstructure %s!\n\n\n", gst_structure_to_string (configure->data));
}
