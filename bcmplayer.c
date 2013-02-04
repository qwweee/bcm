#include "bcmplayer.h"
#include "bcmindexerpriv.h"
#include "bkni.h"
#include "bstd.h"
#include "bdbg.h"
#include <stdio.h>
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"
#include "b_playback.h"
#include "nexus_platform.h"


int playStream(char* indexname, long startTime, long endTime, off_t *prePosition, FILE* outfp); 
void getTSname(char* indexname, char* tsname);
int read_data(BNAV_Player_Handle player, off_t *prePosition, FILE* tsfp, FILE* outfp); 
#define BUFFER_SIZE 524288
int main(int argc, char* argv[]) {
#if 1
	FILE *indexfp;
	FILE *outfp;
	B_Segmented_File_Entry* entry;
	int entrySize = sizeof(B_Segmented_File_Entry);
	int size = 0;
	int startTime = 0;
	int endTime = 0;
	off_t prePosition = 0;

	startTime = (long) atoi(argv[2]);
	endTime = (long) atoi(argv[3]);
	printf("startTime=%d, endTime:%d\n", startTime, endTime);
	indexfp = fopen(argv[1], "rb");
	if (indexfp == NULL) {
		printf("can't open file:%s\n", argv[1]);
		exit(0);
	}
	entry = (B_Segmented_File_Entry*) malloc(entrySize);
	outfp = fopen("/output.ts", "wb");

	while (1) {
		printf("segmented file size:%ld\n", entry->size);
		printf("segmented file voff:%ld\n", entry->voff);
		printf("segmented file doff:%ld\n", entry->doff); size = fread(entry, 1, entrySize, indexfp);
		if (size != entrySize) {
			printf("size is invalid\n");
			break;
		}
		if (playStream(entry->filename, startTime, endTime,
					&prePosition, outfp) == 1) {
			printf("reach endTime position\n");
			break;
		}
	}
	fclose(outfp);
#else
	test();
#endif
	return 0;
}

int playStream(char* indexname, long startTime, long endTime, 
		off_t *prePosition, FILE* outfp) {
	FILE *indexfp;
	FILE *tsfp;
	char *tsname;
	BNAV_Player_Handle player;
	BNAV_Player_Settings settings;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	int startIndex = 0;
	int endIndex = 0;

	tsname = (char*) malloc(strlen(indexname));
	memset(tsname, 0x0, strlen(indexname));
	getTSname(indexname, tsname);

	printf("playStream:%s\n", indexname);
	indexfp = fopen(indexname, "rb");
	if (!indexfp) {
		printf("failed to open indexfile:%s\n", indexname);
	}
	printf("playStream:tsname:%s\n", tsname);
	tsfp = fopen(tsname, "rb");
	if (!tsfp) {
		printf("failed to open tsfile:%s\n", tsfp);
	}

	BNAV_Player_GetDefaultSettings(&settings);
	settings.readCb = (BP_READ_CB)fread;
	settings.tellCb = (BP_TELL_CB)ftell;
	settings.seekCb = (BP_SEEK_CB)fseek;
	settings.filePointer = indexfp;
	settings.videoPid = 0x1e1;
	settings.normalPlayBufferSize = 188*10*1024;

	if (BNAV_Player_Open(&player, &settings)) {
		printf("failed to open BNAV_Player\n");
	}

	BNAV_Player_GetSettings(player, &settings);
	firstEntry = BNAV_Player_ReadEntry(player, settings.firstIndex);
	firstTimestamp = BNAV_get_timestamp(firstEntry);
	lastEntry = BNAV_Player_ReadEntry(player, settings.lastIndex);
	lastTimestamp = BNAV_get_timestamp(lastEntry);
	printf("firstTimestamp:%ld, lastTimestamp:%ld\n", firstTimestamp, lastTimestamp);

	printf("prePosition:%lld\n", *prePosition);
	if (startTime >= firstTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(player, startTime);
		if (endTime <= lastTimestamp) {
			endIndex = BNAV_Player_FindIndexFromTimestamp(player, endTime);
			BNAV_Player_SetBounds(player, startIndex, endIndex);
		}
		printf("startIndex:%d, endIndex:%d\n", startIndex, endIndex);
		BNAV_Player_SetCurrentIndex(player, startIndex);
		read_data(player, prePosition, tsfp, outfp);
	} else if (startTime < firstTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(player, firstTimestamp);
		if (endTime <= lastTimestamp) {
			endIndex = BNAV_Player_FindIndexFromTimestamp(player, endTime);
			BNAV_Player_SetBounds(player, startIndex, endIndex);
		}
		printf("startIndex:%d, endIndex:%d\n", startIndex, endIndex);
		BNAV_Player_SetCurrentIndex(player, startIndex);
		read_data(player, prePosition, tsfp, outfp);
	} 

	fseek(tsfp, 0, SEEK_END);
	*prePosition += ftell(tsfp);
	printf("set prePosition: %lld\n", *prePosition);
	fseek(tsfp, 0 ,SEEK_SET);

	free(tsname);
	fclose(indexfp);
	fclose(tsfp);
	BNAV_Player_Close(player);
	if (endTime <= lastTimestamp) {
		return 1;
	}
	return 0;
}

int test() {
	NEXUS_FilePlayHandle filePlayHandle;
	BNAV_Player_Handle player;
	BNAV_Player_PlayMode playMode;
	BNAV_Player_Settings settings;
	BNAV_Indexer_Settings cfg;
	NEXUS_FilePlayHandle file;
	FILE *fp;
	FILE *outfp;
	FILE *tsfp;

	//NEXUS_Platform_Init(NULL);
	//BKNI_Init();
	//BDBG_Init();
	//B_Os_Init();

#if 0
	file = NEXUS_FilePlay_OpenPosix("/data/media0_0/2139238/2139238.nav", NULL);
	if (!file) {
		fprintf(stderr, "can't open file\n");
		return -1;
	}
#endif
	/* get segmented file list */
	fp = fopen("/data/media0_0/2139238/2139238.nav_1", "rb");
	tsfp = fopen("/data/media0_0/2139238/2139238.ts_1", "rb");
	BNAV_Player_GetDefaultSettings(&settings);
	printf("navVersion:%d\n", settings.navVersion);
	//settings.navVersion = BNAV_VersionOriginal;
	//settings.navVersion = 3;
	printf("navVersion:%d\n", settings.navVersion);
	settings.readCb = (BP_READ_CB)fread;
	settings.tellCb = (BP_TELL_CB)ftell;
	settings.seekCb = (BP_SEEK_CB)fseek;
	settings.filePointer = fp;
	settings.videoPid = 0x1e1;
	settings.normalPlayBufferSize = 188*4*1024;

	if (BNAV_Player_Open(&player, &settings)) {
		printf("failed to open BNAV_Player\n");
	}
	BNAV_Player_PlayEntry entry;
	char packet[188+4];
	int startIndex = 0;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	BNAV_Entry *curEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	BNAV_Player_Position position;
	
	outfp = fopen("/output.ts", "wb");
#if 0
	BNAV_Player_GetPlayMode(player, &playMode);
	playMode.playMode = eBpPlayI;
	playMode.playModeModifier = 3;  // 0=x1, 1=2x, 2=x4, 3=x8, 4=x16
	BNAV_Player_SetPlayMode(player, &playMode);
#endif
	BNAV_Player_GetSettings(player, &settings);
	firstEntry = BNAV_Player_ReadEntry(player, settings.firstIndex);
	firstTimestamp = BNAV_get_timestamp(firstEntry);
	//firstTimestamp = BNAV_get_timestamp(firstEntry);
	lastEntry = BNAV_Player_ReadEntry(player, settings.lastIndex);
	lastTimestamp = BNAV_get_timestamp(lastEntry);
	printf("firstTimestamp:%ld, lastTimestamp:%ld\n", firstTimestamp, lastTimestamp);
#if 0
	startIndex = BNAV_Player_FindIndexFromTimestamp(player, firstTimestamp + (10*1000));
	printf("startIndex:%d\n" , startIndex);
	BNAV_Player_SetCurrentIndex(player, startIndex);
#endif
	//curEntry = BNAV_Player_ReadEntry(player, startIndex);
#if 1
	while (1) {

		printf("firstIndex:%d, lastIndex:%d\n", settings.firstIndex, settings.lastIndex);

		if (BNAV_Player_GetNextPlayEntry(player, &entry, packet)) {
			printf("end of file, break\n");
			break;
		}
		BNAV_Player_GetPosition(player, &position);
		printf("pts:%ld\n", position.pts);
#if 1
		if (entry.isInsertedPacket) {
			printf("isInsertedPacket\n");
			fwrite(packet, 188, 1,outfp);
		} else {
			char buf[entry.byteCount];
			off_t offset = ((off_t)entry.startOffsetHi)<<32 | entry.startOffset;
			printf("offset:%llu, count:%d\n", offset, entry.byteCount);
			fseek(tsfp, offset, SEEK_SET);
			fread(buf, entry.byteCount, 1,tsfp);
			fwrite(buf, entry.byteCount, 1, outfp);
		}
#endif
	}
#endif

	//read_data(player, tsfp, outfp);
	fclose(outfp);
	fclose(fp);
	BNAV_Player_Close(player);
}

int read_data(BNAV_Player_Handle player, off_t *prePosition, FILE* tsfp, FILE* outfp)  {
	BNAV_Player_PlayEntry entry;
	char packet[188+4];
	printf("read_data\n");
	while (1) {
		if (BNAV_Player_GetNextPlayEntry(player, &entry, packet)) {
			printf("end of file, break\n");
			break;
		}
		//BNAV_Player_GetPosition(player, &position);
		//printf("pts:%ld\n", position.pts);
		if (entry.isInsertedPacket) {
			printf("isInsertedPacket\n");
			fwrite(packet, 188, 1,outfp);
		} else {
			char buf[entry.byteCount];
			off_t offset = ((off_t)entry.startOffsetHi)<<32 | entry.startOffset;
			printf("offset:%llu, count:%d\n", offset, entry.byteCount);
			fseek(tsfp, offset-(*prePosition), SEEK_SET);
			fread(buf, entry.byteCount, 1,tsfp);
			fwrite(buf, entry.byteCount, 1, outfp);
		}
	}
}

void getTSname(char* indexname, char* tsname) {
	char* firstChar;
	memset(tsname, 0x0, strlen(indexname));
	firstChar = strstr(indexname, "nav_");
	memcpy(tsname, indexname, firstChar - indexname);
	//printf("1111:%s\n", tsname);
	sprintf(tsname, "%sts%s", tsname, firstChar + 3);
	//printf("222:%s\n", tsname);
}

