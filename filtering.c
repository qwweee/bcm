#include "nexus_platform.h"
#include "nexus_packetsub.h"
#include "nexus_parser_band.h"
#include "nexus_recpump.h" /* for verification */
#include "nexus_record.h"
#include "b_dvr_mgr.h"
#include "b_dvr_datatypes.h"
#include "nexus_message.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE (5*1024*1024)
#define HN_BUFFER_SIZE (7 *188)
#define MAX_COUNT 2

typedef struct _FilteringItem {
	int pid;
	NEXUS_MessageFilter matchMask;
	int bufferSize;
	NEXUS_PidChannelHandle pidChannel;
	NEXUS_MessageHandle msg;
	void *buffer;
	int totalSize;
} FilteringItem;

BDBG_MODULE(packetsub);

static void addFilter(FilteringItem* item);
static void makePat(char* section, int pmtPid);
static void makePmt(char* section, int pmtPid, int vPid, int aPid); 
FilteringItem filterItem[2];
char pat[188];
char pmt[188];

pthread_mutex_t* mutexLock;

void dataready_callback(void *context, int param)
{
    BSTD_UNUSED(context);
    BSTD_UNUSED(param);
}
static void lock_callback(void *context, int param)
{
    NEXUS_FrontendHandle frontend = (NEXUS_FrontendHandle)context;
    NEXUS_FrontendQamStatus qamStatus;


    fprintf(stderr, "Lock callback, frontend 0x%08x\n", (unsigned)frontend);

    NEXUS_Frontend_GetQamStatus(frontend, &qamStatus);
    fprintf(stderr, "QAM Lock callback, frontend 0x%08x - lock status %d, %d\n", (unsigned)frontend,
            qamStatus.fecLock, qamStatus.receiverLock);
}

FILE *outputfile;
int count;
int main(void) {
    NEXUS_InputBand inputBand;
    NEXUS_ParserBand parserBand = NEXUS_ParserBand_e0;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_PidChannelHandle pidChannel;
    NEXUS_PidChannelSettings pidChannelSettings;
    NEXUS_PacketSubHandle packetSub, packetSub2;
    NEXUS_PacketSubOpenSettings packetSubOpenSettings;
    NEXUS_PacketSubSettings packetSubSettings;
    NEXUS_FrontendHandle frontend=NULL;
    NEXUS_FrontendQamSettings qamSettings;
    NEXUS_PlatformConfiguration platformConfig;
	int i = 0;
	NEXUS_PidChannelHandle videoPidChannel, audioPidChannel;
	NEXUS_PidChannelHandle patPidChannel, pmtPidChannel;
	// filtering

    NEXUS_Platform_Init(NULL);
    NEXUS_Platform_GetConfiguration(&platformConfig);
#if 0
    for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
    {
        NEXUS_FrontendCapabilities capabilities;
        frontend = platformConfig.frontend[i];
        if (frontend) {
            NEXUS_Frontend_GetCapabilities(frontend, &capabilities);
            /* Does this frontend support qam? */
            if ( capabilities.qam )
            {
				printf("qamtuner index:%d\n", frontend);
                //break;
            }
        }
    }
#endif
	//exit(1);
	frontend = platformConfig.frontend[1];

    if (NULL == frontend )
    {
        fprintf(stderr, "Unable to find QAM-capable frontend\n");
        return 0;
    }

	fprintf(stderr, "frontend:%d", frontend);

    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = 603 * 1000000;
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = frontend;
    parserBand = NEXUS_ParserBand_e1;
	NEXUS_Frontend_TuneQam(frontend, &qamSettings);


	//sleep(1);
#if 0
    NEXUS_Platform_GetStreamerInputBand(0, &inputBand);

    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputBand;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);
#endif

	outputfile = (FILE*) fopen("output.ts", "w");
	if (outputfile == NULL) {
		printf("fail to open file\n");
		return -1;
	}
	makePat(pat, 111);
	makePmt(pmt, 111, 289, 290);

	mutexLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));

	memset(filterItem, 0x0, sizeof(FilteringItem) * 2);
	filterItem[0].bufferSize = BUFFER_SIZE;
	filterItem[0].pid = 481;
	pidChannel = NEXUS_PidChannel_Open(parserBand, 481, NULL);
	filterItem[0].pidChannel = pidChannel;
	addFilter(&filterItem[0]);

	filterItem[1].bufferSize = BUFFER_SIZE;
	filterItem[1].pid = 482;
	pidChannel = NEXUS_PidChannel_Open(parserBand, 482, NULL);
	filterItem[1].pidChannel = pidChannel;
	addFilter(&filterItem[1]);


	//audioPidChannel = NEXUS_PidChannel_Open(parserBand, 290, NULL);


	printf("press ENTER to stop filtering\n");
	getchar();

	sleep(3);
	pthread_mutex_unlock(mutexLock);
	for (i = 0; i < MAX_COUNT; i++) {
		NEXUS_Message_Stop(filterItem[i].msg);
		NEXUS_Memory_Free(filterItem[i].buffer);
		NEXUS_PidChannel_Close(filterItem[i].pidChannel);
	}
	fclose(outputfile);

	pthread_mutex_destroy(mutexLock);

	//NEXUS_PidChannel_Close(pidChannel);
	//NEXUS_PidChannel_Close(audioPidChannel);
    NEXUS_Platform_Uninit();
    return 0;
}

static void sectionCallback(void *context) {
	FilteringItem *filter = (FilteringItem*) context;
	const void *buffer;
	unsigned int size, total = 0;

	pthread_mutex_lock(mutexLock);
	/*
	filter->totalSize += size;
	if (filter->totalSize >= HN_BUFFER_SIZE) {
		filter->totalSize = 0;
		sleep(1);
	}
	*/
	/*
	while (1) {
		if (size < HN_BUFFER_SIZE) {
			printf("goto sleep\n");
			sleep(2);
			NEXUS_Message_ReadComplete(filter->msg, 0);
			continue;
		} else {
			break;
		}
	}
	*/
start:
		if (NEXUS_Message_GetBuffer(filter->msg, &buffer, &size) != NEXUS_SUCCESS) {
			printf("failed to get buffer data\n");
		}
	if (size < HN_BUFFER_SIZE) {
		printf("--> goto sleep\n");
		sleep(1);
		printf("--> pid=%d, size=%d\n", filter->pid, size);
		NEXUS_Message_ReadComplete(filter->msg, 188);
		//goto start;
	} else {
		printf("1pid=%d, size=%d\n", filter->pid, size);
		NEXUS_Message_ReadComplete(filter->msg, HN_BUFFER_SIZE);
	}


	/*
	fwrite(buffer, size, 1, outputfile);
	if (count % 1000 == 0) {
		fwrite(pat, 188, 1, outputfile);
	}
	if (count % 100 == 0) {
		fwrite(pmt, 188, 1, outputfile); 
	}
	*/
	pthread_mutex_unlock(mutexLock);
	count++;
}

static void overflowCallback(void *context) {
}

// filtering
static void addFilter(FilteringItem* item) {
	NEXUS_MessageSettings settings;
	NEXUS_MessageStartSettings startSettings;
	NEXUS_MemoryAllocationSettings allocSettings;

	NEXUS_Message_GetDefaultSettings(&settings);
	settings.dataReady.callback = (NEXUS_Callback) sectionCallback;
	settings.dataReady.context = (void*) item;
	settings.overflow.callback = (NEXUS_Callback) overflowCallback;
	settings.overflow.context = (void*) item;
	settings.maxContiguousMessageSize = 0;
	settings.bufferSize = 1024;

	item->msg = NEXUS_Message_Open(&settings);

	NEXUS_Message_GetDefaultStartSettings(item->msg, &startSettings);
	startSettings.pidChannel = item->pidChannel;
	startSettings.format = NEXUS_MessageFormat_eTs;
	startSettings.bufferMode = NEXUS_MessageBufferMode_eContinuous;
	startSettings.filterGroup = false;
	startSettings.bufferSize = item->bufferSize;
	memcpy(&startSettings.filter, &item->matchMask, sizeof(startSettings.filter));
	NEXUS_Memory_GetDefaultAllocationSettings(&allocSettings);
	allocSettings.alignment = 1024;
	printf("bufferSize = %d\n", item->bufferSize);
	NEXUS_Memory_Allocate(item->bufferSize, &allocSettings, &item->buffer);
	startSettings.buffer = item->buffer;
	
	NEXUS_Message_Start(item->msg, &startSettings);

}	

int makeCrc32(int crc, char* data, int length) {
	int j;
	crc = ~crc;
	while (length--) {
		for (j=0; j < 8; j++) 
			crc = (crc<<1) ^ ((((*data >> (7-j)) ^ (crc >> 31)) & 1) ?
					0x04c11db7 : 0);
		data++;
	}
	return crc;
}

static void makePat(char* section, int pid) {
	char *pat, *length, *start, *end = section;
	int crc = 0;
	memset(section, 0xff, 188);	
	*end++ = 0x47;
	*end++ = 0x40;
	*end++ = 0x00;
	*end++ = 0x10;
	*end++ = 0x00;
	pat = end;
	*end++ = 0x00; /*table_id=0, PAT*/
	*end++ = 0xB0;
	length = end;
	end++;
	start = end;
	*end++ = 0x00;
	*end++ = 0x01;
	*end++ = 0xc1; /* version number */
	*end++ = 0x00;
	*end++ = 0x00;

	*end++ = 0x00; /* program number */
	*end++ = 0x01;

	*end++ = 0xe0 | ((pid >> 8) & 0x1f);
	*end++ = (pid & 0xff);

	*length = 4 + (int)end - (int)start;
	crc = (makeCrc32(0, pat, (int)end - (int)pat));
	*end++ = crc >> 24;
	*end++ = crc >> 16;
	*end++ = crc >> 8;
	*end++ = crc >> 0;
}

static void makePmt(char* section, int pmtPid, int vPid, int aPid) {
	char *pmt, *length, *start, *end = section;
	int crc = 0;
	int i;

	memset(section , 0xff, 188);
	*end++ = 0x47;
	*end++ = 0x40 | ((pmtPid >> 8) & 0x1f);
	*end++ = (pmtPid & 0xff);
	*end++ = 0x10;
	*end++ = 0x00;
	
	pmt = end;
	*end++ = 0x02;   /* tabld_id=2, PMT */
	*end++ = 0xB0;
	
	length = end;
	end++;
	start = end;

	*end++ = 0x00; /* program number */
	*end++ = 0x01;

	*end++ = 0xc1; /* version number */

	*end++ = 0x00; /* section number start */
	*end++ = 0x00; /* last section number */

	*end++ = (vPid >> 8) & 0x01; /* PCR PID */
	*end++ = vPid & 0xff;

	*end++ = 0xf0;
	*end++ = 0x00; /* program info length */

	/* video info */
	*end++ = 0x1b;  /* H.264 */
	*end++ = 0xe0 | ((vPid >> 8) & 0x1f);
	*end++ = vPid & 0xff;
	*end++ = 0xf0;
	*end++ = 0x00;

	/* audio info */
	*end++ = 0x81; /* ATSC AC3 audio */
	*end++ = 0xe0 | ((aPid >> 8) & 0x1f);
	*end++ = aPid & 0xff;
	*end++ = 0xf0;
	*end++ = 0x00;

	*length = 4 + (int)end - (int)start;
	crc = makeCrc32(0, pmt, (int)end - (int)pmt);

	*end++ = crc >> 24;
	*end++ = crc >> 16;
	*end++ = crc >>  8;
	*end++ = crc >>  0;
}
