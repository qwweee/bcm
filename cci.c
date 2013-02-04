#include "nexus_platform.h"
#include "nexus_packetsub.h"
#include "nexus_parser_band.h"
#include "nexus_recpump.h" /* for verification */
#include "nexus_record.h"

#include "bstd.h"
#include "bkni.h"
#include "bkni_multi.h"
#include <stdio.h>


#define HN_BUFFER_SIZE (4*1024)
//#define HN_BUFFER_SIZE (7*188)

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
    NEXUS_PidChannelHandle pidChannel[10];
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
	NEXUS_PidChannelHandle  videoPidChannel, audioPidChannel;
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

    /**
    Use a recpump to record the result for verification
    **/
    recpump = NEXUS_Recpump_Open(0, NULL);
    data_file = fopen("results.ts", "wb");
    NEXUS_Recpump_GetSettings(recpump, &recpumpSettings);
    recpumpSettings.data.dataReady.callback = dataready_callback;
	//recpumpSettings.data.useBufferSize = HN_BUFFER_SIZE;
    NEXUS_Recpump_SetSettings(recpump, &recpumpSettings);

	pidChannel[0] = NEXUS_PidChannel_Open(parserBand, 481, NULL);
	pidChannel[1] = NEXUS_PidChannel_Open(parserBand, 482, NULL);
	pidChannel[2] = NEXUS_PidChannel_Open(parserBand, 0, NULL);
	pidChannel[3] = NEXUS_PidChannel_Open(parserBand, 480, NULL);

    rc = NEXUS_Recpump_AddPidChannel(recpump, pidChannel[0], NULL);
    BDBG_ASSERT(!rc);
    rc = NEXUS_Recpump_AddPidChannel(recpump, pidChannel[1], NULL);
    BDBG_ASSERT(!rc);
    rc = NEXUS_Recpump_AddPidChannel(recpump, pidChannel[2], NULL);
    BDBG_ASSERT(!rc);
    rc = NEXUS_Recpump_AddPidChannel(recpump, pidChannel[3], NULL);
    BDBG_ASSERT(!rc);
    rc = NEXUS_Recpump_Start(recpump);
    BDBG_ASSERT(!rc);
    while (total_data < 5 * 1024 * 1024) {
        const void *buffer;
        unsigned size;

        rc = NEXUS_Recpump_GetDataBuffer(recpump, &buffer, &size);
        BDBG_ASSERT(!rc);
        //printf("read size:%d \n", size);
        if (!size) {
            BKNI_Sleep(1);
            continue;
        }
#if 0
        fwrite(buffer, size, 1, data_file);
        rc = NEXUS_Recpump_DataReadComplete(recpump, size);
#else
        fwrite(buffer, HN_BUFFER_SIZE, 1, data_file);
        rc = NEXUS_Recpump_DataReadComplete(recpump, HN_BUFFER_SIZE);
#endif
        BDBG_ASSERT(!rc);
        total_data += size;
    }
    NEXUS_Recpump_Stop(recpump);
    NEXUS_Recpump_RemoveAllPidChannels(recpump);
	NEXUS_PidChannel_Close(pidChannel[0]);
	NEXUS_PidChannel_Close(pidChannel[1]);
	NEXUS_PidChannel_Close(pidChannel[2]);
	NEXUS_PidChannel_Close(pidChannel[3]);
    NEXUS_Recpump_Close(recpump);
    fclose(data_file);
    NEXUS_Platform_Uninit();
    return 0;
}
