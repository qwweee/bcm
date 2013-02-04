#include "nexus_platform.h"
#include "nexus_packetsub.h"
#include "nexus_parser_band.h"
#include "nexus_recpump.h" /* for verification */
#include "nexus_record.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdio.h>


#define HN_BUFFER_SIZE (1024*188)
#define MAX_PID_CHANNEL 10

typedef struct _CciInstance {
	int id;
	NEXUS_RecpumpHandle recpump;
	NEXUS_PidChannelHandle pidChannel[MAX_PID_CHANNEL];
	FILE* output;
	int pidSize;
	pthread_t thread;
	char pat[188];
	char pmt[188];
} CciInstance;

typedef struct _TunerInfo {
	NEXUS_ParserBand parserBand;
	NEXUS_FrontendHandle tunerHandle;
} TunerInfo;

void feeding_thread(void* args); 
void startFiltering(NEXUS_ParserBand parserBand, int id); 
static void makePat(char* section, int pmtPid);
static void makePmt(char* section, int pmtPid, int vPid, int aPid); 

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


CciInstance _cciInstance[2];
TunerInfo _tunerInfo[NEXUS_MAX_FRONTENDS];
int main(void) {
    NEXUS_InputBand inputBand;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_PidChannelHandle pidChannel[10];
    NEXUS_PidChannelSettings pidChannelSettings;
    NEXUS_PacketSubHandle packetSub, packetSub2;
    NEXUS_PacketSubOpenSettings packetSubOpenSettings;
    NEXUS_PacketSubSettings packetSubSettings;
    NEXUS_FrontendHandle frontend;
    NEXUS_FrontendQamSettings qamSettings;
    NEXUS_PlatformConfiguration platformConfig;
    unsigned pid = 0x0;
    uint8_t *pkt, *pkt2;
    unsigned size, size2;
    NEXUS_Error rc;
    unsigned total_data = 0;
	int i = 0;
	NEXUS_PidChannelHandle  videoPidChannel, audioPidChannel;
	NEXUS_PidChannelHandle patPidChannel, pmtPidChannel;
	NEXUS_RecordHandle record;
	NEXUS_RecordSettings recordSettings;
	NEXUS_RecordPidChannelSettings pidSettings;
	NEXUS_FileRecordHandle file;
	int sectionNum = 16;
	int flag = 0;
	CciInstance* instance;
	int index;

    NEXUS_Platform_Init(NULL);
    NEXUS_Platform_GetConfiguration(&platformConfig);
	NEXUS_FrontendUserParameters frontParams;
	int ibTunerNum = 0;

	printf("maximum tuners:%d\n", NEXUS_MAX_FRONTENDS);
    for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
    {
        NEXUS_FrontendCapabilities capabilities;
        frontend = platformConfig.frontend[i];
        if (frontend) {
            NEXUS_Frontend_GetCapabilities(frontend, &capabilities);
            /* Does this frontend support qam? */
            if ( capabilities.qam )
            {
				NEXUS_Frontend_GetUserParameters(frontend, &frontParams);
				_tunerInfo[ibTunerNum].tunerHandle = frontend;
				_tunerInfo[ibTunerNum].parserBand = frontParams.param1;
				ibTunerNum++;
				printf("qam tuner:%d\n", i);
            }
        }
    }

    if (NULL == frontend )
    {
        fprintf(stderr, "Unable to find QAM-capable frontend\n");
        return 0;
    }

	for (i = 0; i < ibTunerNum; i++) {
		printf("tuner[%d].tunerHandle=%d\n", i, _tunerInfo[i].tunerHandle);
		printf("tuner[%d].parserBand=%d\n", i, _tunerInfo[i].parserBand);
	}

	printf("frontend:%d\n", frontend);
	index = 0;
	printf("tune to frequency\n");
    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = 603 * 1000000;
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = _tunerInfo[index].tunerHandle;
	NEXUS_Frontend_TuneQam(_tunerInfo[index].tunerHandle, &qamSettings);
	printf("222tune to frequency\n");

#if 0
	index++;
	printf("tune to frequency\n");
    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = 603 * 1000000;
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = _tunerInfo[index].tunerHandle;
	NEXUS_Frontend_TuneQam(_tunerInfo[index].tunerHandle, &qamSettings);
	printf("333tune to frequency\n");
#endif
	/*
    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = 605 * 1000000;
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = frontend;
	*/
	//NEXUS_Frontend_TuneQam(frontend[1], &qamSettings);
	//printf("333tune to frequency\n");

    NEXUS_Platform_GetStreamerInputBand(0, &inputBand);
	index = 0;
    NEXUS_ParserBand_GetSettings(_tunerInfo[index].parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputBand;
	parserBandSettings.transportType = NEXUS_TransportType_eTs;
    NEXUS_ParserBand_SetSettings(_tunerInfo[index].parserBand, &parserBandSettings);
	
	index++;
    NEXUS_ParserBand_GetSettings(_tunerInfo[index].parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputBand;
	parserBandSettings.transportType = NEXUS_TransportType_eTs;
    NEXUS_ParserBand_SetSettings(_tunerInfo[index].parserBand, &parserBandSettings);

	printf("start filtering\n");
	startFiltering(_tunerInfo[0].parserBand, 0);
	//startFiltering(_tunerInfo[0].parserBand, 1);
	//startFiltering(_tunerInfo[0].parserBand, 2);
	//startFiltering(_tunerInfo[0].parserBand, 3);
	//startFiltering(_tunerInfo[0].parserBand, 4);

	getchar();
	_cciInstance[0].id = -1;
	_cciInstance[1].id = -1;
	sleep(2);
    NEXUS_Platform_Uninit();
	return 0;
}

void startFiltering(NEXUS_ParserBand parserBand, int id) {
    NEXUS_RecpumpSettings recpumpSettings;
    NEXUS_Error rc;
	CciInstance* instance = &_cciInstance[id];	
	int index;
	int i;
	char fname[255];
	NEXUS_RecpumpOpenSettings recpumpOpenSettings;

	instance->id = id;
#if 0
    NEXUS_Recpump_GetDefaultOpenSettings(&recpumpOpenSettings);
    recpumpOpenSettings.data.bufferSize = (188/2)*4096*12 + 68;
    //recpumpOpenSettings.data.bufferSize = 16 * 1024*1024;
    //recpumpOpenSettings.data.alignment = 12 * 2; /* 4K alignment */
    recpumpOpenSettings.data.dataReadyThreshold = (188/2)*4096;//BUFFER_SIZE;
    instance->recpump = NEXUS_Recpump_Open(instance->id, &recpumpOpenSettings);
#else
    instance->recpump = NEXUS_Recpump_Open(instance->id, NULL);
#endif
	
	sprintf(fname, "multiple_recpump%d.ts", instance->id);
    instance->output= fopen(fname, "wb");
    NEXUS_Recpump_GetSettings(instance->recpump, &recpumpSettings);
    recpumpSettings.data.dataReady.callback = dataready_callback;
    NEXUS_Recpump_SetSettings(instance->recpump, &recpumpSettings);

	index = 0;
	instance->pidChannel[index++] = 
		NEXUS_PidChannel_Open(parserBand, 1027, NULL);
	instance->pidChannel[index++] = 
		NEXUS_PidChannel_Open(parserBand, 1028, NULL);
	/*
	instance->pidChannel[index++] = 
		NEXUS_PidChannel_Open(parserBand, 0, NULL);
		*/
	/*
	instance->pidChannel[index++] = 
		NEXUS_PidChannel_Open(parserBand, 1027, NULL);
	*/
	instance->pidSize = index;

	for (i = 0; i < instance->pidSize; i++) {
		rc = NEXUS_Recpump_AddPidChannel(
				instance->recpump, instance->pidChannel[i], NULL);
		BDBG_ASSERT(!rc);
	}
	makePat(instance->pat, 480);
	makePmt(instance->pmt, 480, 1027, 1028);
	pthread_create(&instance->thread, NULL, feeding_thread, (void*) instance);
}

void feeding_thread(void* args) {
    NEXUS_Error rc;
	char psi[HN_BUFFER_SIZE];
	CciInstance* instance = (CciInstance*) args;
	int i;
	printf("start feeding : %d\n", instance->id);
	printf("count: %d\n", (HN_BUFFER_SIZE/188));
	for (i = 0; i < HN_BUFFER_SIZE/188; i++) {
		memcpy(psi + (i * 188), instance->pat, 188);
	}
	printf("1count: %d\n", (HN_BUFFER_SIZE/188));
	fwrite(psi, HN_BUFFER_SIZE, 1, instance->output);
#if 1
	for (i = 0; i < HN_BUFFER_SIZE/188; i++) {
		memcpy(psi+ (i * 188), instance->pmt, 188);
	}
	printf("2count: %d\n", (HN_BUFFER_SIZE/188));
	fwrite(psi, HN_BUFFER_SIZE, 1, instance->output);
#endif

	printf("start feeding PMT, PMT\n");
    rc = NEXUS_Recpump_Start(instance->recpump);
    BDBG_ASSERT(!rc);
    while (instance->id > -1) {
        const void *buffer;
        unsigned size;

        rc = NEXUS_Recpump_GetDataBuffer(instance->recpump, &buffer, &size);
        BDBG_ASSERT(!rc);
        if (!size || size < HN_BUFFER_SIZE) {
            BKNI_Sleep(1);
        //rc = NEXUS_Recpump_DataReadComplete(instance->recpump, 0);
            continue;
        }
			printf("---->size:%d\n", size);
#if 1
		//printf("%d, write \n", instance->id);
        //fwrite(buffer, HN_BUFFER_SIZE, 1, instance->output);
        rc = NEXUS_Recpump_DataReadComplete(instance->recpump, HN_BUFFER_SIZE);
#else
        //fwrite(buffer, HN_BUFFER_SIZE, 1, data_file);
        rc = NEXUS_Recpump_DataReadComplete(instance->recpump, HN_BUFFER_SIZE);
#endif
        BDBG_ASSERT(!rc);
    }
    NEXUS_Recpump_Stop(instance->recpump);
    NEXUS_Recpump_RemoveAllPidChannels(instance->recpump);
	for (i = 0; i < instance->pidSize; i++) {
		NEXUS_PidChannel_Close(instance->pidChannel[i]);
	}
    NEXUS_Recpump_Close(instance->recpump);
    fclose(instance->output);
    return 0;
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
