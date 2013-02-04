#include "nexus_platform.h"
#include "nexus_packetsub.h"
#include "nexus_parser_band.h"
#include "nexus_recpump.h" /* for verification */
#include "nexus_record.h"
#include "b_dvr_mgr.h"
#include "b_dvr_datatypes.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdio.h>

#define SEGMENTED_RECORD 1

BDBG_MODULE(packetsub);
extern void B_DVR_SegmentedRecord_Trim_Start_WithTime(B_DVR_SegmentedRecordHandle segmentedRecord, int time);
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
    unsigned pid = 0x0;
    uint8_t *pkt, *pkt2;
    unsigned size, size2;
    NEXUS_Error rc;
    NEXUS_RecpumpHandle recpump;
    NEXUS_RecpumpSettings recpumpSettings;
    unsigned total_data = 0;
    FILE *data_file;
	int i = 0;
	NEXUS_PidChannelHandle videoPidChannel, audioPidChannel;
	NEXUS_PidChannelHandle patPidChannel, pmtPidChannel;
	const char *fname = "videos/stream1.mpg";
	const char *index = "videos/stream1.nav";
	NEXUS_RecordHandle record;
	NEXUS_RecordSettings recordSettings;
	NEXUS_RecordPidChannelSettings pidSettings;
	NEXUS_FileRecordHandle file;
	B_DVR_SegmentedRecordSettings segmentRecordSettings;
	int sectionNum = 16;
	int flag = 0;
	BNAV_Indexer_Settings cfg;

#if SEGMENTED_RECORD
	B_DVR_SegmentedRecordHandle segmentedRecord;
	B_DVR_Mgr_Handle sDvrMgrHandle = NULL;
	B_DVR_Mgr_Settings dvrMgrSettings;

	B_Os_Init();
	BKNI_Memset(&dvrMgrSettings, 0, sizeof(dvrMgrSettings));
	dvrMgrSettings.bseg_rec = true;
	dvrMgrSettings.btimeshift = true;
	sDvrMgrHandle = B_DVR_Mgr_Open(&dvrMgrSettings);
#endif

    NEXUS_Platform_Init(NULL);
#if 1
    NEXUS_Platform_GetConfiguration(&platformConfig);

    for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
    {
        NEXUS_FrontendCapabilities capabilities;
        frontend = platformConfig.frontend[i];
        if (frontend) {
            NEXUS_Frontend_GetCapabilities(frontend, &capabilities);
            /* Does this frontend support qam? */
            if ( capabilities.qam )
            {
                break;
            }
        }
    }

    if (NULL == frontend )
    {
        fprintf(stderr, "Unable to find QAM-capable frontend\n");
        return 0;
    }

	fprintf(stderr, "frontend:%d", frontend);

    NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
    qamSettings.frequency = 489 * 1000000;
	fprintf(stderr, "frequency:%d", frontend);
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = frontend;
    parserBand = NEXUS_ParserBand_e0;
	NEXUS_Frontend_TuneQam(frontend, &qamSettings);

#endif

	//sleep(1);

    NEXUS_Platform_GetStreamerInputBand(0, &inputBand);

    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputBand;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);

	recpump = NEXUS_Recpump_Open(0, NULL);
	record = NEXUS_Record_Create();
	NEXUS_Record_GetSettings(record, &recordSettings);
	recordSettings.recpump = recpump;
	recordSettings.indexFormat = NEXUS_RecordIndexType_eBcm;
	NEXUS_Record_SetSettings(record, &recordSettings);

	BNAV_Indexer_GetDefaultSettings(&cfg);
	printf("version:%d\n", cfg.navVersion);
#if SEGMENTED_RECORD
	printf("start open segmented file.\n");
	/* Timeshift recording */
	B_DVR_SegmentedRecord_GetDefaultSettings(&segmentRecordSettings);
	printf("media_segment_size:%d \n", segmentRecordSettings.media_segment_size);
	printf("index_segment_size:%d \n", segmentRecordSettings.index_segment_size);
	printf("min_duration:%d sec\n", segmentRecordSettings.min_duration);
	printf("max_duration:%d sec\n", segmentRecordSettings.max_duration);
	segmentRecordSettings.min_duration = 10; /* 1sec */
	//segmentRecordSettings.max_duration = 10; /* 10sec */
	printf("after min_duration:%d sec\n", segmentRecordSettings.min_duration);
	printf("after max_duration:%d sec\n", segmentRecordSettings.max_duration);
	segmentedRecord = B_DVR_SegmentedRecord_Open("/data/temp/tsb.ts", "/data/temp/tsb.nav", 
			&segmentRecordSettings, false);
	if (segmentedRecord == NULL) {
		printf("failed to open segmented record.\n");
		return -1;
	}
	file = B_DVR_SegmentedRecord_Get_NexusRecordFile(segmentedRecord);
	if (file == NULL) {
		printf("failed to open file record.\n");
		return -2;
	}
#else
	printf("start open file.\n");
	file = NEXUS_FileRecord_OpenPosix("/data/temp/tsb.ts","/data/temp/tsb.nav");
	//file = NEXUS_FileRecord_OpenPosix(fname, index);
#endif
	BNAV_Indexer_GetDefaultSettings(&cfg);
	printf("version:%d\n", cfg.navVersion);
	printf("getSegmented file to store.\n");
	/* set video pid infomation */
	NEXUS_Record_GetDefaultPidChannelSettings(&pidSettings);
	pidSettings.recpumpSettings.pidType = NEXUS_PidType_eVideo;
	pidSettings.recpumpSettings.pidTypeSettings.video.index = true;
	//pidSettings.recpumpSettings.pidTypeSettings.video.pid = ;
	pidSettings.recpumpSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eMpeg2;
	videoPidChannel = NEXUS_PidChannel_Open(parserBand, 0xe30, NULL);
	NEXUS_Record_AddPidChannel(record, videoPidChannel, &pidSettings);
	printf("added video pid.\n");

	/* set audio pid infomation */
	audioPidChannel = NEXUS_PidChannel_Open(parserBand, 0x331, NULL);
	NEXUS_Record_AddPidChannel(record, audioPidChannel, NULL);
	printf("added audio pid.\n");
	/* set PAT */
	patPidChannel = NEXUS_PidChannel_Open(parserBand, 0x0, NULL);
	NEXUS_Record_AddPidChannel(record, patPidChannel, NULL);
	pmtPidChannel = NEXUS_PidChannel_Open(parserBand, 0x1100, NULL);
	NEXUS_Record_AddPidChannel(record, pmtPidChannel, NULL);
	/* set PMT */

	
	BNAV_Indexer_GetDefaultSettings(&cfg);
	printf("version:%d\n", cfg.navVersion);

	printf("start recording.\n");
	if (NEXUS_Record_Start(record, file) != NEXUS_SUCCESS) {
		printf("fail to start recording\n");
		return -3;
	}
#if SEGMENTED_RECORD
	//B_DVR_SegmentedRecord_Trim_Start(segmentedRecord);
	B_DVR_SegmentedRecord_Trim_Start_WithTime(segmentedRecord, 10*1000);
#endif

	printf("press ENTER to stop recording\n");
	getchar();

	NEXUS_Record_Stop(record);
	NEXUS_Record_RemoveAllPidChannels(record);
	NEXUS_PidChannel_Close(videoPidChannel);
	NEXUS_PidChannel_Close(audioPidChannel);
#if SEGMENTED_RECORD
	B_DVR_SegmentedRecord_Close(segmentedRecord);
#endif
	NEXUS_FileRecord_Close(file);
	NEXUS_Record_Destroy(record);
    NEXUS_Platform_Uninit();
    return 0;
}
