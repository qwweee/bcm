#include <stdio.h>
#include "bstd.h"
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"
#include "b_playback.h"
#include "bcmplayer.h"
#include "bcmindexerpriv.h"

int main(int argc, char* argv[]) {
	FILE *indexfile;
	BNAV_Entry* entry;
	int entrySize = sizeof(BNAV_Entry);
	off_t size = 0;
	off_t totalSize = 0;
	char buf[512];

	indexfile = fopen(argv[1], "rb");
	if (indexfile == NULL) {
		printf("can't open file\n");
		exit(0);
	}
	entry = (BNAV_Entry*) malloc(entrySize);
	printf("entrySize:%d\n", entrySize);
	while (1) {
		memset(entry, 0, entrySize);
		size = fread(buf, 1, entrySize, indexfile);
		if (size != entrySize) {
			printf("fail to read data\n");
			break;
		}
		memcpy(entry, buf, entrySize);
		//totalSize += entry->size;
		printf("frameType:%d\n", BNAV_get_frameType(entry));
		printf("version:%llu\n", BNAV_get_version(entry));
		printf("timestamp:%llu \n", BNAV_get_timestamp(entry));
	}
	free(entry);
	fclose(indexfile);

	return 0;
}

