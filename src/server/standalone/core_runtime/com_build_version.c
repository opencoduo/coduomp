#include <stdio.h>

const char *Com_BuildVersionString(void)
{
    static char version[64];

    sprintf(version, "%d %s %s", 1466, "Feb 10 2005", "15:43:53");
    return version;
}
