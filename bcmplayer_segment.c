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
typedef struct b_file_bp_info {
    unsigned index_entrysize;
    NEXUS_FilePlayHandle play_file;
} b_file_bp_info;


#define BUFFER_SIZE 524288
int main() {
	NEXUS_FilePlayHandle filePlayHandle;
	B_DVR_SegmentedPlayHandle segmentedPlay;
	BNAV_Player_Handle player;
	BNAV_Player_PlayMode playMode;
	BNAV_Player_Settings settings;
	BNAV_Indexer_Settings cfg;
	NEXUS_FilePlayHandle file;
	FILE *fp;
	FILE *outfp;

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
	segmentedPlay = B_DVR_SegmentedPlay_Open(
			"/data/media0_0/2139238/2139238.ts", 
			"/data/media0_0/2139238/2139238.nav", NULL);
	//filePlayHandle = NEXUS_FilePlay_OpenPosix("/data/media0_0/8306843/8306843.ts", NULL);
	filePlayHandle = B_DVR_SegmentedPlay_Get_NexusPlayFile(segmentedPlay);
	BNAV_Player_GetDefaultSettings(&settings);
	printf("navVersion:%d\n", settings.navVersion);
	//settings.navVersion = BNAV_VersionOriginal;
	//settings.navVersion = 3;
	printf("navVersion:%d\n", settings.navVersion);
	settings.readCb = (BP_READ_CB)filePlayHandle->file.index->read;
	settings.tellCb = (BP_TELL_CB)ftell;
	settings.seekCb = (BP_SEEK_CB)filePlayHandle->file.index->seek;
	settings.filePointer = filePlayHandle->file.index;
	settings.videoPid = 0x1e1;

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

	getchar();
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
			fseek(filePlayHandle->file.data, offset, SEEK_SET);
			fread(buf, entry.byteCount, 1, filePlayHandle->file.data);
			fwrite(buf, entry.byteCount, 1, outfp);
		}
#endif
	}

	fclose(outfp);
	fclose(fp);
	BNAV_Player_Close(player);
}
