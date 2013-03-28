#include <glib.h>

gint
configure_file_parse (gchar *conf, gsize s)
{
        GKeyFile *configure;
        GKeyFileFlags flags;
        GError *e;
        gsize size;
        gchar **p, *v;
        gint i;

        configure = g_key_file_new ();

        flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
        if (!g_key_file_load_from_data (configure, conf, s, flags, &e)) {
                g_error (e->message);
                return -1;
        }

        p = g_key_file_get_keys (configure, "globle", &size, &e);

        g_print ("size is %d\n", size);
        for (i = 0; i < size; i++) {
                v = g_key_file_get_value (configure, "globle", p[i], &e);
                g_print ("%s - %s\n", p[i], v);
        }
}

gchar *
configure_extract_template (gchar *conf, gsize size)
{
        gchar *template, *p, *p1, *p2, *p3, *p4, *p5; 
        GRegex *reg;
        GError *e = NULL;
        GArray *conf_array;
        GArray *variable_array;
        gint i, line_number;
        gchar var_status;

        conf_array = g_array_new (FALSE, FALSE, sizeof (gchar *));
        template = g_strdup_printf ("");
        p = g_strdup_printf ("%s", conf);
        p1 = p2 = p3 = p;
        line_number = 0;
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
                                        var_status = '/';
                                        p5 = g_strndup (p4, p3 - p4);
                                        g_array_append_val (conf_array, p5);
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
                g_print ("%s\n", g_array_index (conf_array, gchar *, i));
        }
        return template;
}

gint
main (gint argc, gchar *argv[])
{
        GError *e;
        gsize size;
        gchar *p, *pp;
        gchar *conf, *conf_tmp, *conf_template;
        gint right_square_bracket;

        g_file_get_contents ("configure.conf", &conf, &size, &e);

        configure_extract_template (conf, size);

        conf_tmp = g_malloc (size * 2);
        p = conf;
        pp = conf_tmp;
        right_square_bracket = 0;
        for (;;) {
                switch (*p) {
                case '{':
                        right_square_bracket++;
                        *pp = *p;
                        pp++; p++;
                        break;
                case '}':
                        right_square_bracket--;
                        *pp = *p;
                        pp++; p++;
                        break;
                case '\n':
                        if (right_square_bracket > 0) {
                                *pp = '\\'; pp++;
                                *pp = 'n'; pp++;
                                p++;
                        } else {
                                *pp = *p;
                                pp++; p++;
                        }
                        break;
                default:
                        *pp = *p;
                        pp++; p++;
                        break;
                }
                if ((p - conf) == size) break;
        }
        *pp = '\0';
        g_free (conf);
        conf = conf_tmp;
        configure_file_parse (conf, pp - conf);
}
