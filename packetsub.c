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
	const char *fname = "videos/stream.mpg";
	const char *index = "videos/stream.nav";
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

    patPidChannel = NEXUS_PidChannel_Open(parserBand, 0x99, NULL);
    NEXUS_PacketSub_GetDefaultOpenSettings(&packetSubOpenSettings);
    packetSubOpenSettings.fifoSize = sectionNum * 188 + 4; //1024;

    packetSub = NEXUS_PacketSub_Open(0, &packetSubOpenSettings);

    NEXUS_PacketSub_GetSettings(packetSub, &packetSubSettings);
	packetSubSettings.outputRate = 188 * 8 * 2 * 16;
    packetSubSettings.pidChannel = patPidChannel;
    packetSubSettings.loop = true;
    rc = NEXUS_PacketSub_SetSettings(packetSub, &packetSubSettings);
    BDBG_ASSERT(!rc);

    rc = NEXUS_PacketSub_GetBuffer(packetSub, (void **)&pkt, &size);
    BDBG_ASSERT(!rc);
    BDBG_ASSERT(size >= 188);
	printf("buffer size:%d\n", size);
    /* create new packet to replace the PAT */
	printf("address : %x\n", pkt);
	sleep(1);
	memset(pkt, 0xFF, 188*sectionNum);

	//pkt[0] = 0x47;
#if 0
	for (i = 0; i < sectionNum; i+= 188) {
		//if (flag == 0) {
			pkt[i+0] = 0x47;   pkt[i+1] = 0x40; pkt[i+2] = 0x00; pkt[i+3] = 0x10;
			pkt[i+4] = 0x00; pkt[i+5] = 0x00; pkt[i+6] = 0xB0; pkt[i+7] = 0x0D;
			pkt[i+8] = 0x00; pkt[i+9] = 0x01; pkt[i+10] = 0xE5; pkt[i+11] = 0x00;
			pkt[i+12] = 0x00; pkt[i+13] = 0x00; pkt[i+14] = 0xAB; pkt[i+15] = 0xE1;
			pkt[i+16] = 0x00; pkt[i+17] = 0xC3; pkt[i+18] = 0x76; pkt[i+19] = 0xFF;
			pkt[i+20] = 0xF7;
			/*
			flag = 1;
		} else {
			pkt[i+0]=0x47; pkt[i+1]=0x41; pkt[i+2]=0x00; pkt[i+3]=0x10; 
			pkt[i+4]=0x00; pkt[i+5]=0x02; pkt[i+6]=0xB0; pkt[i+7]=0x17; 
			pkt[i+8]=0x00; pkt[i+9]=0xAB; pkt[i+10]=0xE5; pkt[i+11]=0x00; 
			pkt[i+12]=0x00; pkt[i+13]=0xE0; pkt[i+14]=0x70; pkt[i+15]=0xF0;
			pkt[i+16]=0x00; pkt[i+17]=0x02; pkt[i+18]=0xE0; pkt[i+19]=0x70;
			pkt[i+20]=0xF0; pkt[i+21]=0x00; pkt[i+22]=0x04; pkt[i+23]=0xE0;
			pkt[i+24]=0x71; pkt[i+25]=0xF0; pkt[i+26]=0x00; pkt[i+27]=0x20;
			pkt[i+28]=0xE5; pkt[i+29]=0x67; pkt[i+30]=0x42; 
			flag = 0;
		}
		*/
	}
/*
	pkt[189]=0x47; pkt[190]=0x41; pkt[191]=0x00; pkt[192]=0x10; 
	pkt[193]=0x00; pkt[194]=0x02; pkt[195]=0xB0; pkt[196]=0x17; 
	pkt[197]=0x00; pkt[198]=0xAB; pkt[199]=0xE5; pkt[200]=0x00; 
	pkt[201]=0x00; pkt[202]=0xE0; pkt[203]=0x70; pkt[204]=0xF0;
	pkt[205]=0x00; pkt[206]=0x02; pkt[207]=0xE0; pkt[208]=0x70;
	pkt[209]=0xF0; pkt[210]=0x00; pkt[211]=0x04; pkt[212]=0xE0;
	pkt[213]=0x71; pkt[214]=0xF0; pkt[215]=0x00; pkt[216]=0x20;
	pkt[217]=0xE5; pkt[218]=0x67; pkt[219]=0x42; 
	*/

    rc = NEXUS_PacketSub_WriteComplete(packetSub, 188 * sectionNum);
    BDBG_ASSERT(!rc);

    rc = NEXUS_PacketSub_Start(packetSub);
    BDBG_ASSERT(!rc);
	
	// make packetSub2
    pmtPidChannel = NEXUS_PidChannel_Open(parserBand, 0x98, NULL);

    packetSub2 = NEXUS_PacketSub_Open(1, &packetSubOpenSettings);

    NEXUS_PacketSub_GetSettings(packetSub2, &packetSubSettings);
	packetSubSettings.outputRate = 188 * 8 * 2 * 16;
    packetSubSettings.pidChannel = pmtPidChannel;
	//packetSubSettings.fullRateOutput = true;
    packetSubSettings.loop = true;
    rc = NEXUS_PacketSub_SetSettings(packetSub2, &packetSubSettings);
    BDBG_ASSERT(!rc);

    rc = NEXUS_PacketSub_GetBuffer(packetSub2, (void **)&pkt2, &size2);
    BDBG_ASSERT(!rc);
    BDBG_ASSERT(size2 >= 188);
	printf("buffer size2:%d\n", size2);

    /* create new packet to replace the PAT */
	memset(pkt2, 0xFF, 188*sectionNum);
	for (i = 0; i < sectionNum; i += 188) {
	pkt2[i+0]=0x47; pkt2[i+1]=0x41; pkt2[i+2]=0x00; pkt2[i+3]=0x10; 
	pkt2[i+4]=0x00; pkt2[i+5]=0x02; pkt2[i+6]=0xB0; pkt2[i+7]=0x17; 
	pkt2[i+8]=0x00; pkt2[i+9]=0xAB; pkt2[i+10]=0xE5; pkt2[i+11]=0x00; 
	pkt2[i+12]=0x00; pkt2[i+13]=0xE0; pkt2[i+14]=0x70; pkt2[i+15]=0xF0;
	pkt2[i+16]=0x00; pkt2[i+17]=0x02; pkt2[i+18]=0xE0; pkt2[i+19]=0x70;
	pkt2[i+20]=0xF0; pkt2[i+21]=0x00; pkt2[i+22]=0x04; pkt2[i+23]=0xE0;
	pkt2[i+24]=0x71; pkt2[i+25]=0xF0; pkt2[i+26]=0x00; pkt2[i+27]=0x20;
	pkt2[i+28]=0xE5; pkt2[i+29]=0x67; pkt2[i+30]=0x42; 
	}

    rc = NEXUS_PacketSub_WriteComplete(packetSub2, 188 * sectionNum);
    BDBG_ASSERT(!rc);

    rc = NEXUS_PacketSub_Start(packetSub2);

    BDBG_ASSERT(!rc);
    recpump = NEXUS_Recpump_Open(0, NULL);
	record = NEXUS_Record_Create();
	NEXUS_Record_GetSettings(record, &recordSettings);
	recordSettings.recpump = recpump;
	NEXUS_Record_SetSettings(record, &recordSettings);

	file = NEXUS_FileRecord_OpenPosix(fname, index);

	NEXUS_Record_GetDefaultPidChannelSettings(&pidSettings);
	pidSettings.recpumpSettings.pidType = NEXUS_PidType_eVideo;
	pidSettings.recpumpSettings.pidTypeSettings.video.index = true;
	pidSettings.recpumpSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eMpeg2;
	videoPidChannel = NEXUS_PidChannel_Open(parserBand, 0x203, NULL);
	NEXUS_Record_AddPidChannel(record, videoPidChannel, &pidSettings);
	
	audioPidChannel = NEXUS_PidChannel_Open(parserBand, 0x204, NULL);
	NEXUS_Record_AddPidChannel(record, audioPidChannel, NULL);

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
    NEXUS_PacketSub_Stop(packetSub);
    NEXUS_PacketSub_Close(packetSub);
    NEXUS_PacketSub_Stop(packetSub2);
    NEXUS_PacketSub_Close(packetSub2);
//    NEXUS_PidChannel_Close(pidChannel);
#endif
    NEXUS_Platform_Uninit();
    return 0;
}
