#include "nexus_platform.h"
#include "nexus_packetsub.h"
#include "nexus_parser_band.h"
#include "nexus_recpump.h" /* for verification */
#include "nexus_record.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdio.h>

BDBG_MODULE(packetsub);

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
	int sectionNum = 16;
	int flag = 0;

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
    qamSettings.frequency = 603 * 1000000;
    qamSettings.mode = NEXUS_FrontendQamMode_e256;
	qamSettings.symbolRate = 5360537;
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = frontend;
    parserBand = NEXUS_ParserBand_e0;
	NEXUS_Frontend_TuneQam(frontend, &qamSettings);

#endif

    NEXUS_Platform_GetStreamerInputBand(0, &inputBand);

    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = inputBand;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);

    NEXUS_PidChannel_GetDefaultSettings(&pidChannelSettings);

    recpump = NEXUS_Recpump_Open(0, NULL);
	record = NEXUS_Record_Create();
	NEXUS_Record_GetSettings(record, &recordSettings);
	recordSettings.recpump = recpump;
	NEXUS_Record_SetSettings(record, &recordSettings);

	file = NEXUS_FileRecord_OpenPosix(fname, index);

	NEXUS_Record_GetDefaultPidChannelSettings(&pidSettings);
	pidSettings.recpumpSettings.pidType = NEXUS_PidType_eVideo;
	pidSettings.recpumpSettings.pidTypeSettings.video.index = true;
	pidSettings.recpumpSettings.pidTypeSettings.video.pid = 0x303;
	pidSettings.recpumpSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eMpeg2;
	videoPidChannel = NEXUS_PidChannel_Open(parserBand, 0x303, NULL);
	NEXUS_Record_AddPidChannel(record, videoPidChannel, &pidSettings);
	
	audioPidChannel = NEXUS_PidChannel_Open(parserBand, 0x304, NULL);
	NEXUS_Record_AddPidChannel(record, audioPidChannel, NULL);
	patPidChannel = NEXUS_PidChannel_Open(parserBand, 0x0, NULL);
	pmtPidChannel = NEXUS_PidChannel_Open(parserBand, 0x20, NULL);
	NEXUS_Record_AddPidChannel(record, patPidChannel, NULL);
	NEXUS_Record_AddPidChannel(record, pmtPidChannel, NULL);

	NEXUS_Record_Start(record, file);

	printf("press ENTER to stop record\n");
	getchar();

	NEXUS_Record_Stop(record);
	NEXUS_Record_RemoveAllPidChannels(record);
	NEXUS_PidChannel_Close(videoPidChannel);
	NEXUS_PidChannel_Close(audioPidChannel);
	NEXUS_PidChannel_Close(patPidChannel);
	NEXUS_PidChannel_Close(pmtPidChannel);
	NEXUS_FileRecord_Close(file);
	NEXUS_Record_Destroy(record);
	NEXUS_Recpump_Close(recpump);

#if 0
    /**
    Use a recpump to record the result for verification
    **/
    recpump = NEXUS_Recpump_Open(0, NULL);
    data_file = fopen("packetsub_results.ts", "wb");
    NEXUS_Recpump_GetSettings(recpump, &recpumpSettings);
    recpumpSettings.data.dataReady.callback = dataready_callback;
    NEXUS_Recpump_SetSettings(recpump, &recpumpSettings);

	videoPidChannel = NEXUS_PidChannel_Open(parserBand, 0x70, NULL);
	audioPidChannel = NEXUS_PidChannel_Open(parserBand, 0x71, NULL);

    rc = NEXUS_Recpump_AddPidChannel(recpump, videoPidChannel, NULL);
    rc = NEXUS_Recpump_AddPidChannel(recpump, audioPidChannel, NULL);
    BDBG_ASSERT(!rc);
    rc = NEXUS_Recpump_Start(recpump);
    BDBG_ASSERT(!rc);
    while (total_data < 5 * 1024 * 1024) {
        const void *buffer;
        unsigned size;

        rc = NEXUS_Recpump_GetDataBuffer(recpump, &buffer, &size);
        BDBG_ASSERT(!rc);
        if (!size) {
            BKNI_Sleep(1);
            continue;
        }
        fwrite(buffer, size, 1, data_file);
        rc = NEXUS_Recpump_DataReadComplete(recpump, size);
        BDBG_ASSERT(!rc);
        printf("writing %d bytes\n", size);
        total_data += size;
    }
    NEXUS_Recpump_Stop(recpump);
    NEXUS_Recpump_RemoveAllPidChannels(recpump);
    NEXUS_Recpump_Close(recpump);
    fclose(data_file);
#endif
    NEXUS_Platform_Uninit();
    return 0;
}
