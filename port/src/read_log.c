/* DW1_READLOG=file: log every CdRead the game issues (destination, bytes) for
 * the buffer-overflow audit in tools/check_reads.py. The first line gives the
 * run-time address of main, so addresses map back to `nm` symbols in a PIE.
 * Natively a read that overruns a C array lands on whatever the linker put
 * next, unlike the PS1 RAM layout the game expects (BOYS.MMD was one).
 * Hooked with the linker's --wrap=CdRead. */
#include <stdio.h>
#include <stdlib.h>

#include <libcd.h>

int __real_CdRead(int sectors, u_long *buf, int mode);
int main(int argc, char **argv);

static FILE *log_file;
static int opened;

int __wrap_CdRead(int sectors, u_long *buf, int mode)
{
    if (!opened) {
        const char *path = getenv("DW1_READLOG");

        opened = 1;
        if (path != NULL && (log_file = fopen(path, "w")) != NULL)
            fprintf(log_file, "main %p\n", (void *)main);
    }
    if (log_file != NULL) {
        fprintf(log_file, "read %p %d\n", (void *)buf, sectors * 2048);
        fflush(log_file);
    }
    return __real_CdRead(sectors, buf, mode);
}
