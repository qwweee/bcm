#include <stdio.h>
#include "bstd.h"
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"
#include "b_playback.h"
#include "bcmplayer.h"
#include "bcmindexerpriv.h"

#include "nexus_platform.h"

typedef unsigned long long AC_UINT64;
typedef unsigned long AC_UINT32;

typedef struct {
	AC_UINT64 tsLength;
	AC_UINT64 duration;
} Aci_IndexInfo;


void convertPath(char* filename) {
	char* chp = strstr(filename, "data");
	char* temp[255];
	sprintf(temp, "/mnt/usb1%s", chp + 4);
	memcpy(filename, temp, 255);

}

int changePath(FILE* infile, FILE* outfile) {
	B_Segmented_File_Entry* entry;
	int entrySize = sizeof(B_Segmented_File_Entry);
	int count = 0;
	int size = 0;
	entry = (B_Segmented_File_Entry*) malloc(entrySize);

	printf("enter changePath\n");
	while (1) {
		size = fread(entry, 1, entrySize, infile);
		if (size != entrySize) break;
		convertPath(entry->filename);
		fwrite(entry, entrySize, 1, outfile);
		count++;
	}
	printf("entry size is %d\n", count);
	return count;
}

int main(int argc, char* argv[]) {
	FILE* infile = fopen(argv[1], "rb");
	FILE* outfile = fopen(argv[2], "wb");

	if (infile == NULL) {
		printf("failed to open infile\n");
		return -1;
	}
	if (outfile == NULL) {
		printf("failed to open outfile\n");
		return -1;
	}

	printf("infile:%s\n", argv[1]);
	printf("outfile:%s\n", argv[2]);
	changePath(infile, outfile);
}
