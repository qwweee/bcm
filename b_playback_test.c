#include "b_playback.h"
#include "bstd.h"
#include "bkni.h"
#include "bdbg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"

/**
Sample app that uses bcmplayer to read and play an MPEG2 TS file w/ trick codes.
Could be the basis for a server-based stream player.
**/
#define COUNT_FILE 3
int bcmplayer_start(char* tsfile, char* indexfile, long vpid, FILE* output); 

#if 0
int main(int argc, char **argv)
{
	char index_files[COUNT_FILE][40];
	char ts_files[COUNT_FILE][40];
    FILE *outputfile;
    int i;

    BKNI_Init();
    BDBG_Init();
	sprintf(index_files[0], "/data/media0_0/110197/110197.nav_0");
	sprintf(index_files[1], "117324.nav_0");
	sprintf(index_files[2], "117324.nav_1");

	sprintf(ts_files[0], "/data/media0_0/110197/110197.ts_0");
	sprintf(ts_files[1], "117324.ts_0");
	sprintf(ts_files[2], "117324.ts_1");

	outputfile = (FILE*) fopen("output.ts", "w");
	BDBG_ASSERT(outputfile);
	
//		printf("%s \n ", ts_files[1]);
//		bcmplayer_start(ts_files[1], index_files[1], 112, outputfile);
	for (i = 0; i < 1 /*COUNT_FILE*/; i++) {
//		if (i == 0) continue;
		printf("%s \n ", ts_files[i]);
		bcmplayer_start(ts_files[i], index_files[i], 112, outputfile);
	}
	fclose(outputfile);
}
#endif

int main() {
	FILE *indexfile;
	FILE *tsfile;
	B_Segmented_File_Entry* entry;
	B_Segmented_File_Entry* tsEntry;
	int size;
	int entrySize = sizeof(B_Segmented_File_Entry);
	char buf[512];
    FILE *outputfile;
	outputfile = (FILE*) fopen("output.ts", "w");

	indexfile = fopen("/data/media0_0/110197/110197.nav", "rb");
	tsfile = fopen("/data/media0_0/110197/110197.ts", "rb");
	if (indexfile == NULL) {
		printf("can't open file\n");
		exit(0);
	}
	entry = (B_Segmented_File_Entry*) malloc(entrySize);
	tsEntry = (B_Segmented_File_Entry*) malloc(entrySize);
	printf("entrySize:%d\n", entrySize);
	while (1) {
		memset(entry, 0, entrySize);
		size = fread(buf, 1, entrySize, indexfile);
		memcpy(entry, buf, entrySize);
		if (size != entrySize) {
			printf("fail to read data\n");
			break;
		}
		printf("size:%d\n", entry->size);
		printf("segment_no:%d\n", entry->segment_no);
		printf("filename:%s\n", entry->filename);
		size = fread(buf, 1, entrySize, tsfile);
		memcpy(tsEntry, buf, entrySize);
		printf("filename:%s\n", tsEntry->filename);
		bcmplayer_start(tsEntry->filename, entry->filename, 0x1FFF, outputfile);
	}
	free(entry);
	fclose(indexfile);
	fclose(outputfile);

	return 0;
}
int bcmplayer_start(char* tsfile, char* indexfile, long vpid, FILE* outputfile) {
    b_playback_t p;
    struct b_playback_start_settings start_settings;
	struct b_playback_trick_mode trick_mode;
    int rc;
    p = b_playback_create(NULL);
    BDBG_ASSERT(p);

    b_playback_get_default_start_settings(&start_settings);
	printf("tsfile:%s, indexfile:%s\n", tsfile, indexfile);
    start_settings.datafile = tsfile;
    start_settings.indexfile = indexfile;
    start_settings.video_pid = vpid;
    rc = b_playback_start(p, &start_settings);
    BDBG_ASSERT(!rc);

	b_playback_get_default_trick_mode(&trick_mode);
	trick_mode.rate = B_PLAYBACK_NORMAL_RATE;// * 100;
	rc = b_playback_trick_mode(p, &trick_mode);
	BDBG_ASSERT(!rc);
    /* first pass w/ normal play */
    while (1) {
        void *buffer;
        unsigned size;
        rc = b_playback_get_buffer(p, &buffer, &size);
        BDBG_ASSERT(!rc);
        if (size == 0) { 
			printf(" end of file, exit\n");
			break;
		}
        fwrite(buffer, size, 1, outputfile);
        rc = b_playback_read_complete(p, size);
        BDBG_ASSERT(!rc);
    }

    b_playback_stop(p);
    b_playback_destroy(p);
    return 0;
}
