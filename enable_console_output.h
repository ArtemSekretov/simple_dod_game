#include <stdio.h>

static void
enable_console(void)
{
    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
}