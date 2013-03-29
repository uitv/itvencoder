#include <string.h>
#include <glib.h>

gint
configure_file_parse (gchar *conf, gsize size)
{
        GKeyFile *configure;
        GKeyFileFlags flags;
        GError *e;
        gsize number;
        gchar **p, *v;
        gint i;

        configure = g_key_file_new ();

        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (configure, conf, size, flags, &e)) {
                g_error (e->message);
                return -1;
        }

        p = g_key_file_get_keys (configure, "globle", &number, &e);
        g_print ("number is %d\n", number);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (configure, "globle", p[i], &e);
                g_print ("%s - %s\n", p[i], v);
        }
        g_strfreev (p);

        p = g_key_file_get_keys (configure, "elements", &number, &e);
        g_print ("number is %d\n", number);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (configure, "elements", p[i], &e);
                g_print ("%s - %s\n", p[i], v);
        }
        g_strfreev (p);

        p = g_key_file_get_keys (configure, "channels", &number, &e);
        g_print ("number is %d\n", number);
        for (i = 0; i < number; i++) {
                v = g_key_file_get_value (configure, "channels", p[i], &e);
                g_print ("%s - %s\n", p[i], v);
        }
        g_strfreev (p);
}

typedef struct _ConfigurableVar {
        gint index;
        gchar *name;
        gchar *type;
        gchar *description;
        gchar *value;
} ConfigurableVar;

gchar *
configure_extract_variable (gchar *conf, gsize size)
{
        gchar *p, *p1, *p2, *p3, *p4, *p5; 
        GRegex *reg;
        GError *e = NULL;
        GArray *conf_array;
        GArray *variable_array;
        ConfigurableVar *configurable_var;
        gint i, line_number, index;
        gchar var_status;

        conf_array = g_array_new (FALSE, FALSE, sizeof (gchar *));
        variable_array = g_array_new (FALSE, FALSE, sizeof (gpointer));
        p = g_strdup_printf ("%s", conf);
        p1 = p2 = p3 = p;
        line_number = 0;
        index = 0;
        for (;;) {
                if (p1 - p >= size) break;

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
                                        g_array_append_val (conf_array, p5);
                                        index++;
                                        p3 = p2;
                                } else {
                                        g_print ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return NULL;
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
                                        g_array_append_val (conf_array, p5);
                                        configurable_var = g_malloc (sizeof (ConfigurableVar));
                                        configurable_var->value = g_strdup (p5);
                                        configurable_var->index = index;
                                        g_array_append_val (variable_array, configurable_var);
                                        index++;
                                        p4 = p3;
                                } else {
                                        g_print ("line %d, position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return NULL;
                                }
                                break;
                        case '>':
                                if (var_status == '<') {
                                        /* open variable define */
                                        var_status = '>';
                                        p5 = g_strndup (p4, p3 - p4 + 1);
                                        g_array_append_val (conf_array, p5);
                                        index++;
                                        p4 = p3 + 1;
                                } else if (var_status == '/') {
                                        /* close variable define */
                                        var_status = 'v';
                                } else {
                                        g_print ("line %d position %d: %s, parse error\n", line_number, p3 - p1, p1);
                                        return NULL;
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
        for (i = 0; i < conf_array->len; i++) {
                //g_print ("%s\n", g_array_index (conf_array, gchar *, i));
        }
        for (i = 0; i < variable_array->len; i++) {
                configurable_var = g_array_index (variable_array, gpointer, i);
                //g_print ("index %d value %s\n", configurable_var->index, configurable_var->value);
        }
        return NULL;
}

/*
 * replace newline with \n, so it can be parsed by glib ini style parser.
 */
gchar *
configure_replace_newline (gchar *conf, gsize size)
{
        gchar *p1, *p2, *p3;
        gint right_square_bracket;

        p3 = p1 = g_malloc (size * 2);
        p2 = conf;
        right_square_bracket = 0;
        for (;;) {
                switch (*p2) {
                case '{':
                        right_square_bracket++;
                        *p3 = *p2;
                        p3++; p2++;
                        break;
                case '}':
                        right_square_bracket--;
                        *p3 = *p2;
                        p3++; p2++;
                        break;
                case '\n':
                        if (right_square_bracket > 0) {
                                *p3 = '\\'; p3++;
                                *p3 = 'n'; p3++;
                                p2++;
                        } else {
                                *p3 = *p2;
                                p3++; p2++;
                        }
                        break;
                default:
                        *p3 = *p2;
                        p3++; p2++;
                        break;
                }
                if ((p2 - conf) == size) break;
        }
        *p3 = '\0';
        p3 = g_strdup_printf ("%s", p1);
        g_free (p1);
        return p3;
}

gint
main (gint argc, gchar *argv[])
{
        GError *e;
        gsize size;
        gchar *p, *conf;
        gint right_square_bracket;

        g_file_get_contents ("configure.conf", &conf, &size, &e);

        configure_extract_variable (conf, size);
        
        p = configure_replace_newline (conf, size);
        configure_file_parse (p, strlen (p));
}
