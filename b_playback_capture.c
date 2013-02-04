#include "b_playback.h"
#include "bstd.h"
#include "bkni.h"
#include "bdbg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
Sample app that uses bcmplayer to read and play an MPEG2 TS file w/ trick codes.
Could be the basis for a server-based stream player.
**/

int main(int argc, char **argv)
{
    b_playback_t p;
    struct b_playback_start_settings start_settings;
    struct b_playback_trick_mode trick_mode;
    FILE *outputfile;
    int rc;
    int i;

    if (argc < 5) {
        printf("Usage: b_playback_capture inputfile.ts inputfile.nav 0x11 outputfile.ts\n");
        return -1;
    }

    BKNI_Init();
    BDBG_Init();

    p = b_playback_create(NULL);
    BDBG_ASSERT(p);

    b_playback_get_default_start_settings(&start_settings);
    start_settings.datafile = argv[1];
    start_settings.indexfile = argv[2];
    start_settings.video_pid = strtoul(argv[3], NULL, 0);
    rc = b_playback_start(p, &start_settings);
    BDBG_ASSERT(!rc);

    outputfile = fopen(argv[4], "w");
    BDBG_ASSERT(outputfile);

    /* first pass w/ normal play */
    for (i=0;i<10;i++) {
        void *buffer;
        unsigned size;
        rc = b_playback_get_buffer(p, &buffer, &size);
        BDBG_ASSERT(!rc);
        if (size == 0) break;
        fwrite(buffer, size, 1, outputfile);
        rc = b_playback_read_complete(p, size);
        BDBG_ASSERT(!rc);
    }

    rc = b_playback_seek(p, 0, SEEK_SET);
    BDBG_ASSERT(!rc);
	printf("goto trick mode\n");

    b_playback_get_default_trick_mode(&trick_mode);
    trick_mode.rate = B_PLAYBACK_NORMAL_RATE * 10;
    rc = b_playback_trick_mode(p, &trick_mode);
    BDBG_ASSERT(!rc);

    /* second pass as trick mode */
    for (i=0;i<1000;i++) {
        void *buffer;
        unsigned size;
        rc = b_playback_get_buffer(p, &buffer, &size);
        BDBG_ASSERT(!rc);
        if (size == 0) break;
        fwrite(buffer, size, 1, outputfile);
        rc = b_playback_read_complete(p, size);
        BDBG_ASSERT(!rc);
    }

    fclose(outputfile);
    b_playback_stop(p);
    b_playback_destroy(p);

    /* you can now decode the file. be sure to decode in vsync mode because of the embedded trick mode. */

    return 0;
}
