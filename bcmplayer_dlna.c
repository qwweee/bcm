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

typedef enum {
	IDX_SUCCESS = 0,
	IDX_MOVE_NEXT_FILE, /** move to next segmented index file */
	IDX_END_OF_FILE,	/** reached end of segmented index file */
	IDX_BEGIN_OF_FILE,  /** reached begin of segmented index file */
	IDX_END_OF_TIME,	/** reached end of timestamp of index file */
	IDX_FAIL_UNKNOWN,
} INDEX_RESULT;

typedef struct _PlayInstance {
	int plId;
	B_Segmented_File_Entry* fileEntries;
	int countOfFileEntry;
	int currentEntry;
	int currentIndex;
	BNAV_Player_Handle player;
	off_t prePosition;
	FILE* tsfp;
	FILE* indexfp;
	long startTime;
	long endTime;
	float rate;

} PlayInstance;
int aci_hn_timeseek_playStream(char* indexname, long startTime, long endTime);
int aci_hn_timeseek_getNextChunk(int plId, char** buf, int* bufsize);
int aci_hn_timeseek_setRate(int plId, int rate);
int aci_hn_timeseek_stopStream(int plId);
int playStream(char* indexname, long startTime, long endTime, off_t *prePosition, FILE* outfp); 
void getTSname(char* indexname, char* tsname);
int read_data(BNAV_Player_Handle player, off_t *prePosition, FILE* tsfp, FILE* outfp); 

static INDEX_RESULT setCurrentIndex(PlayInstance *instance);
static INDEX_RESULT setNextIndex(PlayInstance *instance);
static INDEX_RESULT setIndex(PlayInstance *instance, bool isMoveNextEntry);
static INDEX_RESULT setForwardIndex(PlayInstance *instance);
static INDEX_RESULT setBackwardIndex(PlayInstance *instance);


PlayInstance instances[10];
FILE *outfp;
#define BUFFER_SIZE 524288

int aci_hn_timeseek_playStream(char* indexname, long startTime, long endTime) {
	B_Segmented_File_Entry* entry;
	int i;
	int entrySize = sizeof(B_Segmented_File_Entry);
	int size = 0;
	INDEX_RESULT ret = IDX_FAIL_UNKNOWN;

	PlayInstance *instance = &instances[0];
	instance->startTime = startTime;
	instance->endTime = endTime;
	printf("aci_hn_timeseek_playstream\n");
	FILE *indexfp = fopen(indexname, "rb");
	if (indexfp == NULL) {
		printf("can't open file:%s\n", indexname);
		exit(0);
	}
	fseek(indexfp, 0, SEEK_END);
	instance->countOfFileEntry = ftell(indexfp) / entrySize;
	fseek(indexfp, 0, SEEK_SET);
	printf("count of file entries:%d\n", instance->countOfFileEntry);
	entry = (B_Segmented_File_Entry*) malloc(entrySize);
	instance->fileEntries = (B_Segmented_File_Entry*) malloc(
			entrySize*instance->countOfFileEntry);
	for (i = 0; i < instance->countOfFileEntry; i++) {	
		size = fread(entry, 1, entrySize, indexfp);
		memcpy(instance->fileEntries + i, entry, entrySize);
	}
	for (i = 0; i < instance->countOfFileEntry;i++) {
		//memcpy(entry, instance->fileEntries + i, entrySize);
		printf("entry:%s\n", instance->fileEntries[i].filename);
	}

	ret = setCurrentIndex(instance);
	printf("111ret:%d\n", ret);
	while (ret == IDX_MOVE_NEXT_FILE) {
		printf("ret:%d\n", ret);
		ret = setNextIndex(instance);
	}
	return ret;
}
static INDEX_RESULT setCurrentIndex(PlayInstance *instance) {
	return setIndex(instance, false);
}

static INDEX_RESULT setNextIndex(PlayInstance *instance) {
	INDEX_RESULT ret = IDX_FAIL_UNKNOWN;
	printf("called %s\n", __FUNCTION__);
	if (instance->rate > -1) {
		fseek(instance->tsfp, 0, SEEK_END);
		instance->prePosition += ftell(instance->tsfp);
		printf("set prePosition: %lld\n", instance->prePosition);
		fseek(instance->tsfp, 0 ,SEEK_SET);
	}
	ret = setIndex(instance , true);
	if (ret == IDX_SUCCESS && instance->rate < 0) {
		fseek(instance->tsfp, 0, SEEK_END);
		instance->prePosition -= ftell(instance->tsfp);
		printf("ftell: %ld\n", ftell(instance->tsfp));
		printf("set prePosition: %lld\n", instance->prePosition);
		fseek(instance->tsfp, 0 ,SEEK_SET);
	}
	return ret;
}
#if 1
static INDEX_RESULT setIndex(PlayInstance *instance, bool isMoveNextEntry) {
	BNAV_Player_Handle player;
	BNAV_Player_Settings settings;
    BNAV_Player_PlayMode playMode;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	int startIndex = 0;
	int endIndex = 0;
	char *tsname;
	B_Segmented_File_Entry* entry;
	INDEX_RESULT ret = IDX_FAIL_UNKNOWN;

	printf("--- called %s\n", __FUNCTION__);
	/* close previous instance */
	printf("close previous instance\n");
	if (instance->player != NULL) {
		BNAV_Player_Close(instance->player);
		instance->player = NULL;
	}
	if (instance->indexfp != NULL) {
		fclose(instance->indexfp);
		instance->indexfp = NULL;
	}
	if (instance->tsfp != NULL) {
		fclose(instance->tsfp);
		instance->tsfp = NULL;
	}
	/* move to next index if isMoveNextEntry is true */
	if (isMoveNextEntry) {
		if (instance->rate > -1) {
			instance->currentEntry++;
		} else {
			instance->currentEntry--;
		}
	}
	/* check currentEntry valication */
	if (instance->currentEntry > instance->countOfFileEntry) {
		printf("current is greater than real entry\n");
		ret = IDX_END_OF_FILE;
		goto err;
	}
	if (instance->currentEntry < 0) {
		printf("current is less than 0\n");
		ret = IDX_BEGIN_OF_FILE;
		goto err;
	}
	entry = &instance->fileEntries[instance->currentEntry];
	tsname = (char*) malloc(strlen(entry->filename));
	memset(tsname, 0x0, strlen(entry->filename));
	getTSname(entry->filename, tsname);

	printf("playStream:%s\n", entry->filename);
	instance->indexfp = fopen(entry->filename, "rb");
	if (!instance->indexfp) {
		printf("failed to open indexfile:%s\n", entry->filename);
	}
	printf("playStream:tsname:%s\n", tsname);
	instance->tsfp = fopen(tsname, "rb");
	if (!instance->tsfp) {
		printf("failed to open tsfile:%s\n", tsname);
	}

	free(tsname);
	BNAV_Player_GetDefaultSettings(&settings);
	settings.readCb = (BP_READ_CB)fread;
	settings.tellCb = (BP_TELL_CB)ftell;
	settings.seekCb = (BP_SEEK_CB)fseek;
	settings.filePointer = instance->indexfp;
	settings.videoPid = 0xFFFF;
	settings.normalPlayBufferSize = 188*10*1024;

	if (BNAV_Player_Open(&instance->player, &settings)) {
		printf("failed to open BNAV_Player\n");
	}
	/* set play rate */
    BNAV_Player_GetPlayMode(instance->player, &playMode);
	if (instance->rate == 0 || instance->rate == 1) {
		playMode.playMode = eBpPlayNormal;
		playMode.playModeModifier = 0;
	} else {
		playMode.playMode = eBpPlayI;
		playMode.playModeModifier = instance->rate;
	}
	printf("setRate:playModeModifier:%d\n", playMode.playModeModifier);
    BNAV_Player_SetPlayMode(instance->player, &playMode);
	if (instance->rate > -1) {
		ret = setForwardIndex(instance);
	} else {
		ret = setBackwardIndex(instance);
	}
err:
	printf("%s ret:%d\n", __FUNCTION__, ret);
	return ret;
}

static INDEX_RESULT setForwardIndex(PlayInstance *instance) {
	BNAV_Player_Settings settings;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	int startIndex = 0;
	int endIndex = 0;

	printf("called %s\n", __FUNCTION__);
	BNAV_Player_GetSettings(instance->player, &settings);
	firstEntry = BNAV_Player_ReadEntry(instance->player, settings.firstIndex);
	firstTimestamp = BNAV_get_timestamp(firstEntry);
	lastEntry = BNAV_Player_ReadEntry(instance->player, settings.lastIndex);
	lastTimestamp = BNAV_get_timestamp(lastEntry);
	printf("firstTimestamp:%ld, lastTimestamp:%ld\n", firstTimestamp, lastTimestamp);

	printf("prePosition:%lld\n", instance->prePosition);
	if (instance->startTime > lastTimestamp) {
		// move to next segmented file
		return IDX_MOVE_NEXT_FILE;
	} else if (instance->startTime >= firstTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, instance->startTime);
		if (instance->endTime <= lastTimestamp) {
			endIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, instance->endTime);
			BNAV_Player_SetBounds(instance->player, startIndex, endIndex);
		}
		printf("startIndex:%d, endIndex:%d\n", startIndex, endIndex);
		BNAV_Player_SetCurrentIndex(instance->player, startIndex);
	} else if (instance->startTime < firstTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, firstTimestamp);
		if (instance->endTime <= lastTimestamp) {
			endIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, instance->endTime);
			BNAV_Player_SetBounds(instance->player, startIndex, endIndex);
		}
		printf("startIndex:%d, endIndex:%d\n", startIndex, endIndex);
		BNAV_Player_SetCurrentIndex(instance->player, startIndex);
	} 
	instance->currentIndex = startIndex;
	if (instance->endTime <= firstTimestamp) {
		return IDX_END_OF_TIME;
	}
	return IDX_SUCCESS;
}

static INDEX_RESULT setBackwardIndex(PlayInstance *instance) {
	BNAV_Player_Settings settings;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	int startIndex = 0;
	int endIndex = 0;

	printf("called %s\n", __FUNCTION__);
	BNAV_Player_GetSettings(instance->player, &settings);
	firstEntry = BNAV_Player_ReadEntry(instance->player, settings.firstIndex);
	firstTimestamp = BNAV_get_timestamp(firstEntry);
	lastEntry = BNAV_Player_ReadEntry(instance->player, settings.lastIndex);
	lastTimestamp = BNAV_get_timestamp(lastEntry);
	printf("firstTimestamp:%ld, lastTimestamp:%ld\n", firstTimestamp, lastTimestamp);

	printf("prePosition:%lld\n", instance->prePosition);
	printf("cI:%ld, fI:%ld, lI:%ld\n", instance->currentIndex,
			settings.firstIndex, settings.lastIndex);
	if (instance->startTime >= firstTimestamp && instance->startTime <= lastTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, firstTimestamp);
		BNAV_Player_SetBounds(instance->player, startIndex, instance->currentIndex);
		BNAV_Player_SetCurrentIndex(instance->player, instance->currentIndex);
		printf("set currentIndex:%d\n", instance->currentIndex);
	} else if (instance->startTime > lastTimestamp) {
		startIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, firstTimestamp);
		endIndex = BNAV_Player_FindIndexFromTimestamp(instance->player, lastTimestamp);
		BNAV_Player_SetBounds(instance->player, startIndex, endIndex);
		BNAV_Player_SetCurrentIndex(instance->player, endIndex);
		printf("set endIndex:%d\n", endIndex);
	} 
	return IDX_SUCCESS;
}

int aci_hn_timeseek_setRate(int plId, int rate) {
	PlayInstance *instance = &instances[plId];
	instance->rate = rate;
	setCurrentIndex(instance);
}

int aci_hn_timeseek_getNextChunk(int plId, char** buf, int* bufsize) {
	BNAV_Player_PlayEntry entry;
	char packet[188+4];
	BNAV_Player_Position position;
	PlayInstance *instance = &instances[plId];
	int ret = -1;
start_getNextChunk:
	//printf("aci_hn_timeseek_getNextChunk\n");
	if (BNAV_Player_GetNextPlayEntry(instance->player, &entry, packet)) {
		printf("aci_hn_timeseek_getNextChunk\n");
		ret = setNextIndex(instance);
		printf("return value:%d\n", ret);
		if (ret == IDX_SUCCESS) {
			goto start_getNextChunk;
		} else {
			return -1;
		}
	}
	BNAV_Player_GetPosition(instance->player, &position);
	//instance->currentIndex = position.index;
	//printf("position:%d\n", position.index);
	if (entry.isInsertedPacket) {
		//printf("isInsertedPacket\n");
		*buf = packet;
		*bufsize = 188;
	} else {
		char* read_buf = (char*) malloc(entry.byteCount);
		off_t offset = ((off_t)entry.startOffsetHi)<<32 | entry.startOffset;
		printf("offset:%llu, count:%d, prePos:%llu, fp:%llu\n", offset, entry.byteCount,
				instance->prePosition, (offset-instance->prePosition));
		fseek(instance->tsfp, offset-(instance->prePosition), SEEK_SET);
	//	printf("file pointer:%ld\n", ftell(instance->tsfp));
		fread(read_buf, entry.byteCount, 1, instance->tsfp);
		*buf = read_buf;
		*bufsize = entry.byteCount;
	}
	return 0;
}

int aci_hn_timeseek_stopStream(int plId) {
	PlayInstance *instance = &instances[plId];
	if (instance->player != NULL) {
		BNAV_Player_Close(instance->player);
	}
	if (instance->indexfp != NULL) {
		fclose(instance->indexfp);
	}
	if (instance->tsfp != NULL) {
		fclose(instance->tsfp);
	}
	if (instance->fileEntries != NULL) {
		free(instance->fileEntries);
	}
	memset(instance, 0x0, sizeof(PlayInstance));
}
#endif
int main(int argc, char* argv[]) {
#if 1
	FILE *indexfp;
	B_Segmented_File_Entry* entry;
	int startTime = 0;
	int endTime = 0;
	int trickMode = 0;
	off_t prePosition = 0;
	char *buf;
	int bufSize;

	startTime = (long) atoi(argv[2]);
	endTime = (long) atoi(argv[3]);
	trickMode = (int) atoi(argv[4]);
	printf("startTime=%d, endTime:%d, trick:%d\n", startTime, endTime, trickMode);

	outfp = fopen("/out.ts", "wb");
	memset(instances, 0x0, 10*sizeof(PlayInstance));
	aci_hn_timeseek_playStream(argv[1], startTime, endTime);
	aci_hn_timeseek_setRate(0, trickMode);
	while (1) {
		if (aci_hn_timeseek_getNextChunk(0, &buf, &bufSize) < 0) {
			break;
		}
		fwrite(buf, bufSize, 1, outfp);
		free(buf);
	}
	aci_hn_timeseek_stopStream(0);
	fclose(outfp);
#endif
	//test();
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

	printf("prePosition:%ld\n", *prePosition);
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
	printf("set prePosition: %ld\n", *prePosition);
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
	fp = fopen("/data/media0_0/2139238/2139238.nav_0", "rb");
	tsfp = fopen("/data/media0_0/2139238/2139238.ts_0", "rb");
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
	int endIndex = 0;
	BNAV_Entry *firstEntry;
	BNAV_Entry *lastEntry;
	BNAV_Entry *curEntry;
	unsigned long firstTimestamp = 0;
	unsigned long lastTimestamp = 0;
	BNAV_Player_Position position;
	
	outfp = fopen("/output_-32x.ts", "wb");
#if 1
	BNAV_Player_GetPlayMode(player, &playMode);
	playMode.playMode = eBpPlayI;
	playMode.playModeModifier = -32;  // 0=x1, 1=2x, 2=x4, 3=x8, 4=x16
	BNAV_Player_SetPlayMode(player, &playMode);
#endif
	BNAV_Player_GetSettings(player, &settings);
	firstEntry = BNAV_Player_ReadEntry(player, settings.firstIndex);
	firstTimestamp = BNAV_get_timestamp(firstEntry);
	//firstTimestamp = BNAV_get_timestamp(firstEntry);
	lastEntry = BNAV_Player_ReadEntry(player, settings.lastIndex);
	lastTimestamp = BNAV_get_timestamp(lastEntry);
	printf("firstTimestamp:%ld, lastTimestamp:%ld\n", firstTimestamp, lastTimestamp);
#if 1
	startIndex = BNAV_Player_FindIndexFromTimestamp(player, firstTimestamp + (10*1000));
	printf("startIndex:%d\n" , startIndex);
	endIndex = BNAV_Player_FindIndexFromTimestamp(player, lastTimestamp);
	printf("endIndex:%d\n" , startIndex);
	BNAV_Player_SetCurrentIndex(player, endIndex);
#endif
	//curEntry = BNAV_Player_ReadEntry(player, startIndex);

#if 1
	while (1) {

		//printf("firstIndex:%d, lastIndex:%d\n", settings.firstIndex, settings.lastIndex);

		if (BNAV_Player_GetNextPlayEntry(player, &entry, packet)) {
			printf("end of file, break\n");
			break;
		}
		BNAV_Player_GetPosition(player, &position);
		//printf("index:%d, pts:%ld\n", position.index, position.pts);
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

