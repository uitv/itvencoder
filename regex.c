#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

int
main()
{
        guint i;
        gchar *string = "udpsrc uri=<%uri%> ! mpegdemux program-number=<%program-number%>";
        gchar *fmt;
        GRegex *pipe;
        GMatchInfo *match_info;
        GArray* parameters = g_array_new(FALSE, FALSE, sizeof(char*));

        /* obtain parameters list */
        pipe = g_regex_new("<%(?<para>[^<%]*)%>", G_REGEX_OPTIMIZE, 0, NULL);
        if (pipe == NULL) {
                g_print("bad regular expression\n");
                exit(0);
        }

        g_regex_match (pipe, string, 0, &match_info);
        while(g_match_info_matches(match_info)) {
                gchar *word = g_match_info_fetch_named(match_info, "para");
                //g_print("Found: %s\n", word);
                g_array_append_val(parameters, word);
                //g_free(word);
                g_match_info_next(match_info, NULL);
        }
        g_match_info_free(match_info);
        g_regex_unref(pipe);

        for(i=0; i<parameters->len; i++) {
                g_print("%s\n", g_array_index(parameters, gchar*, i));
                //g_free(g_array_index(parameters, gchar*, i)); //TODO
        }

        /* obtain pipeline format string */
        pipe = g_regex_new("<%[^<%]*%>", G_REGEX_OPTIMIZE, 0, NULL);
        if (pipe == NULL) {
                g_print("bad regular expression\n");
                exit(0);
        }

        fmt = g_regex_replace(pipe, string, -1, 0, "%s", 0, NULL);
        g_print("%s\n", fmt);

        g_regex_unref(pipe);

        return 0;
}
