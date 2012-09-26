#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "itvencoder.h"

int
main()
{
        ITVEncoder *itvencoder;

        g_type_init();
        itvencoder = g_object_new(TYPE_ITVENCODER, 0, NULL);
        printf("%lld\n", itvencoder_get_start_time(itvencoder));
        return 0;
}
