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

int main(int argc, char* argv[]) {
	NEXUS_FilePlayHandle playHandle;
	B_DVR_SegmentedPlayHandle segmentedPlay;
	Aci_IndexInfo indexInfo;
	bfile_io_read_t file = NULL;
	off_t first, last;

	NEXUS_Platform_Init(NULL);

	segmentedPlay = B_DVR_SegmentedPlay_Open(
			"/data/temp/tsb.ts",
			"/data/temp/tsb.nav", NULL);

	file = &segmentedPlay->segmented_index_reader->self;

	file->bounds(file, &first, &last);

	printf("start:%lld\n", first);
	printf("end:%lld\n", last);
	playHandle = B_DVR_SegmentedPlay_Get_NexusPlayFile(segmentedPlay);
	getIndexInfo(playHandle, &indexInfo);

	B_DVR_SegmentedPlay_Close(segmentedPlay);
	NEXUS_FilePlay_Close(playHandle);

}

void getIndexInfo(NEXUS_FilePlayHandle playHandle, Aci_IndexInfo* info) {
	BNAV_Entry* entry;
	int entrySize = sizeof(BNAV_Entry);
	off_t size = 0;
	off_t start, end;

	entry = (BNAV_Entry*) malloc(entrySize);
	int startTimeStamp = -1;
	int endTimeStamp = -1;
	while (1) {
		char buf[entrySize];
		memset(entry, 0, entrySize);
		size = playHandle->file.index->read(playHandle->file.index, buf, entrySize);
		if (size != entrySize) {
			printf("fail to read data\n");
			break;
		}
		memcpy(entry, buf, entrySize);
		if (BNAV_get_frameType(entry) == 1) {
			/*
			printf("frameType:%d\n", BNAV_get_frameType(entry));
			printf("version:%d\n", BNAV_get_version(entry));
			printf("timestamp:%u \n", BNAV_get_timestamp(entry));
			*/
			if (startTimeStamp < 0) {
				startTimeStamp = BNAV_get_timestamp(entry);
			}
			endTimeStamp = BNAV_get_timestamp(entry);
		}
	}
	printf("startTimeStamp:%u\n", startTimeStamp);
	printf("endTimeStamp:%u\n", endTimeStamp);
	printf("duration:%u\n", (endTimeStamp - startTimeStamp));
	info->duration = endTimeStamp - startTimeStamp;

	/* TS file length */
	//playHandle->file.data->seek(playHandle->file.data, 0, SEEK_END);
	//end = ftell(playHandle->file.data);
	//segmentedFile = (B_DVR_SegmentedFileHandle) playHandle->file.data;
	//start = segmentedFile->linearStartOffset;
	//end = segmentedFile->linearEndOffset;
	printf("start:%lld, end:%lld\n", start, end);
	free(entry);
}

int getEntrySize(FILE* file) {
	B_Segmented_File_Entry* entry;
	int entrySize = sizeof(B_Segmented_File_Entry);
	int count = 0;
	int size = 0;
	char buf[entrySize];

	printf("enter getEntrySize\n");
	while (1) {
		size = fread(buf, 1, entrySize, file);
		if (size != entrySize) break;
		count++;
	}
	printf("entry size is %d\n", count);
	return count;
}
#if 0
void print_index() {
	FILE *indexfile;
	B_Segmented_File_Entry* entry;
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
		totalSize += entry->size;
		printf("frameType:%d\n", BNAV_get_frameType(entry));
		printf("version:%llu\n", BNAV_get_version(entry));
		printf("timestamp:%llu \n", BNAV_get_timestamp(entry));
	}
	free(entry);
	fclose(indexfile);

	return 0;
}
#endif

