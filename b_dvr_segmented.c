
#include <stdio.h>
#include <stdlib.h>
#include "nexus_platform.h"
#include "nexus_dma.h"
#include "nexus_file.h"
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"

#define BUFFER_SIZE (1024*1024)
void segment_test();
void file_test();
void file_length();
char*	readBuffer = NULL;
int size;

int main() {
	NEXUS_Platform_Init(NULL);
	//file_test();
	//segment_test();
	file_length();
}

void file_test() {
	FILE* inputFile;
	inputFile = fopen("/data/hnext_data/root/avata_con.ts", "rb");
	if (inputFile == NULL) {
		printf("fail to open %s\n", inputFile);
		return -1;
	}
	readBuffer = malloc(sizeof(char) * BUFFER_SIZE);
	size = fread(readBuffer, 1, BUFFER_SIZE, inputFile);
	printf("size: %d\n" , size);
	free(readBuffer);
	fclose(inputFile);
}

void file_length() {
	NEXUS_FilePlayHandle filePlayHandle;
	B_DVR_SegmentedPlayHandle segmentedPlay;
	off_t startFp;
	off_t endFp;
	printf("bedore file size:\n");
	segmentedPlay = B_DVR_SegmentedPlay_Open(
			"/data/media0_0/8306843/8306843.ts", 
			"/data/media0_0/8306843/8306843.nav", NULL);
	//filePlayHandle = NEXUS_FilePlay_OpenPosix("/data/media0_0/8306843/8306843.ts", NULL);
	filePlayHandle = B_DVR_SegmentedPlay_Get_NexusPlayFile(segmentedPlay);

	NEXUS_FilePlay_GetBounds(filePlayHandle, &startFp, &endFp);
	//filePlayHandle->file.data->bounds(filePlayHandle->file.data, &startFp, &endFp);
	printf("file start fp:%lld\n", startFp);
	printf("file end fp:%lld\n", endFp);
	NEXUS_FilePlay_Close(filePlayHandle);

}

void segment_test() {
	NEXUS_FilePlayHandle filePlayHandle;
	B_DVR_SegmentedPlayHandle segmentedPlay;
	B_DVR_SegmentedPlaySettings settings;
	FILE *outputfile;

	segmentedPlay = B_DVR_SegmentedPlay_Open(
			"/data/media0_0/8306843/8306843.ts", 
			"/data/media0_0/8306843/8306843.nav", NULL);
	//segmentedPlay = B_DVR_SegmentedPlay_Open(".", ".", NULL);
	if (segmentedPlay == NULL) {
		printf("fail to open SegmentedPlay\n");
		return -1;
	}
	outputfile = (FILE*) fopen("output.ts", "w");
	if (outputfile == NULL) {
		printf("file to open file\n");
		return -1;
	}

#if 0
	B_DVR_SegmentedPlay_GetSettings(segmentedPlay,&settings);
	settings.index_end =  1000;
	settings.index_start = 0;
	settings.media_end =  1000;
	settings.media_start = 0;
	B_DVR_SegmentedPlay_SetSettings(segmentedPlay,&settings);
	BDBG_MSG(("\n opened segmented playback"));
#endif

	readBuffer = (char*)malloc(BUFFER_SIZE);
	if (readBuffer == NULL) {
		printf("fail to malloc readBuffer\n");
		return -1;
	}
	memset(readBuffer, 0, BUFFER_SIZE);

	filePlayHandle = B_DVR_SegmentedPlay_Get_NexusPlayFile(segmentedPlay);
	if (filePlayHandle == NULL) {
		printf("can't find fileHandle\n");
		return -1;
	}

	while (1) {
		size = (int)filePlayHandle->file.data->read(filePlayHandle->file.data, 
				readBuffer, BUFFER_SIZE);
		if (size <= 0) break;
		fwrite(readBuffer, size, 1, outputfile);
	}
	printf("exit fileHandle\n");
	free(readBuffer);
	fclose(outputfile);

	return 0;
}
