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

gint
main (gint argc, gchar *argv[])
{
        GError *e;
        gsize size;
        gchar *p, *pp;
        gchar *conf, *conf_tmp, *conf_template;
        GRegex *reg;
        gint right_square_bracket;

        g_file_get_contents ("configure.conf", &conf, &size, &e);

        conf_template = g_malloc (size * 2);
        reg = g_regex_new ("(^[^#]*>)[^<]*</", G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
        if (reg == NULL) {
                g_error (e->message);
                return -1;
        }
        conf_template = g_regex_replace (reg, conf, -1, 0, "\\1%s</", G_REGEX_MATCH_NOTBOL | G_REGEX_MATCH_NOTEOL, NULL);
        g_print ("%s", conf_template);

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
        g_print ("%s",conf_tmp);
        g_free (conf);
        conf = conf_tmp;
        configure_file_parse (conf, pp - conf);
}
