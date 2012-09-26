
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "config.h"

int
main()
{
        Config *config;

        g_type_init();
        config = g_object_new(TYPE_CONFIG, 0, NULL);

        return 0;
}
