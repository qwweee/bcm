#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_parser_band.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_adj.h"
#include "nexus_video_input.h"
#include "nexus_spdif_output.h"
#include "nexus_component_output.h"
#if NEXUS_HAS_VIDEO_ENCODER && NEXUS_HAS_STREAM_MUX
#include "nexus_playback.h"
#include "nexus_record.h"
#include "nexus_file.h"
#include "nexus_video_encoder.h"
#include "nexus_audio_encoder.h"
#include "nexus_stream_mux.h"
#include "nexus_recpump.h"
#include "nexus_file_fifo.h"
#endif
#if NEXUS_NUM_HDMI_INPUTS
#include "nexus_hdmi_input.h"
#endif
#if NEXUS_DTV_PLATFORM
#include "nexus_platform_boardcfg.h"
#endif
#if NEXUS_HAS_SYNC_CHANNEL
#include "nexus_sync_channel.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bstd.h"
#include "bkni.h"

#define BTST_MUX_VIDEO_PID      (0x11)
#define BTST_MUX_AUDIO_PID      (0x12)
#define BTST_MUX_PCR_PID        (0x13)
#define BTST_MUX_PMT_PID        (0x55)
#define BTST_MUX_PAT_PID        (0x0)

#define BTST_ENCODER_STC_IDX                2
#define BTST_SOURCE_STC_IDX                 0

#define BTST_STREAM_MUX_VIDEO_PLAYPUMP_IDX  1
#define BTST_STREAM_MUX_AUDIO_PLAYPUMP_IDX  2
#define BTST_STREAM_MUX_PCR_PLAYPUMP_IDX    3

#define BTST_DISABLE_VIDEO_ENCODER_PES_PACING 0

BDBG_MODULE(encode_video_ts);

static const uint8_t s_auiTSPacket_PAT[188] =
{
	0x47,0x40,0x00,0x30,0xa6,0x40,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,'P','A','T',
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0x00,0x00,0xb0,0x0d,0x00,
	0x00,0x81,0x00,0x00,0x00,0x01,0xe0,0x55,
	0x65,0x80,0x5e,0xdc,
};

#define BTST_PMT_TRANSPORT_IDX BTST_SYSTEM_TRANSPORT_IDX
static uint8_t s_auiTSPacket_PMT[188] =
{
	0x47,0x40,BTST_MUX_PMT_PID,0x30,0x9c,0x40,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,'P','M','T',
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0x00,0x02,0xb0,0x17,0x00,0x01,0xc1,
	0x00,0x00,0xe0,0x13,0xf0,0x00,0x1b,0xe0,
	0x11,0xf0,0x00,0x81,0xe0,0x12,0xf0,0x00,
	0x3d,0x19,0x07,0x2f,
};

#if NEXUS_HAS_VIDEO_ENCODER && NEXUS_HAS_STREAM_MUX
typedef struct EncodeSettings {
	char                    fname[256];
	char                    indexfname[256];
	NEXUS_VideoFormat       displayFormat;
	NEXUS_VideoFrameRate    encoderFrameRate;
	int                     encoderBitrate;
	int                     encoderGopStructureFramesP;
	int                     encoderGopStructureFramesB;
	NEXUS_VideoCodec        encoderVideoCodec;
	NEXUS_VideoCodecProfile encoderProfile;
	NEXUS_VideoCodecLevel   encoderLevel;
	NEXUS_DisplayCustomFormatSettings customFormatSettings;
	bool                    bCustom;
	bool                    bAudioEncode;
	NEXUS_AudioCodec        encoderAudioCodec;
}  EncodeSettings;

typedef struct InputSettings{
	NEXUS_TransportType    eStreamType;
	NEXUS_VideoCodec       eVideoCodec;
	NEXUS_AudioCodec       eAudioCodec;
	int                    iVideoPid;
	int                    iAudioPid;
	int                    iPcrPid;
	int                    freq;
	NEXUS_FrontendQamMode  qamMode;
	bool                   bAudioInput;

} InputSettings;

typedef struct TranscodeContext {
	NEXUS_PlatformConfiguration platformConfig;

	/* xcoder handles */
	NEXUS_VideoEncoderHandle   videoEncoder;
	NEXUS_DisplayHandle        displayTranscode;
	NEXUS_VideoWindowHandle    windowTranscode;
	NEXUS_PlaypumpHandle       playpumpTranscodeVideo;
	NEXUS_PidChannelHandle     pidChannelTranscodeVideo;
	NEXUS_PlaypumpHandle       playpumpTranscodeAudio;
	NEXUS_PidChannelHandle     pidChannelTranscodeAudio;
	NEXUS_AudioEncoderHandle   audioEncoder;
	NEXUS_AudioMuxOutputHandle audioMuxOutput;
	NEXUS_StreamMuxHandle      streamMux;
	BKNI_EventHandle           finishEvent;
	NEXUS_PlaypumpHandle       playpumpTranscodePcr;
	NEXUS_StcChannelHandle     stcChannelTranscode;

	/* xcoder mux/record handles */
	NEXUS_FileRecordHandle     fileTranscode;
	NEXUS_RecpumpHandle        recpump;
	NEXUS_RecordHandle         record;
	NEXUS_PidChannelHandle     pidChannelTranscodePcr;
	NEXUS_PidChannelHandle     pidChannelTranscodePat;
	NEXUS_PidChannelHandle     pidChannelTranscodePmt;

	/* source local decode/display handles */
	NEXUS_StcChannelHandle     stcChannel;
	NEXUS_DisplayHandle        display;
	NEXUS_VideoWindowHandle    window;
	
	NEXUS_PidChannelHandle     videoPidChannel;
	NEXUS_VideoDecoderHandle   videoDecoder;
	NEXUS_PidChannelHandle     audioPidChannel;
	NEXUS_AudioDecoderHandle   audioDecoder, audioPassthrough;
#if NEXUS_HAS_SYNC_CHANNEL
	NEXUS_SyncChannelHandle    syncChannel;
#endif

	/* video encoder settings */
	EncodeSettings             encodeSettings;

	/* input settings */
	InputSettings              inputSettings;

	NEXUS_FrontendHandle       frontend;
} TranscodeContext;

/* global context 
 * function level context in this */
static TranscodeContext xcodeContext;

/* ============= utilities ==============*/
/* Generate a CRC for the specified data/length */
/* Initialize crc to 0 for new calculation.  Use an old result for subsequent calls. */
static uint32_t CRC32_mpeg(
    uint32_t crc,
    uint8_t *data,
    int length
) {
	int j;
	crc = ~crc;
	while (length--)
	{
		for (j=0; j<8; j++) 
			crc = (crc<<1) ^ ((((*data >> (7-j)) ^ (crc >> 31)) & 1) ? 0x04c11db7 : 0);
		data++;
	}
	return crc;
}

/* xcode window PQ setting */
static void window_dnr( 
	NEXUS_VideoWindowHandle  hWindow,
	bool bCustom )
{
	NEXUS_VideoWindowDnrSettings windowDnrSettings;

	NEXUS_VideoWindow_GetDnrSettings(hWindow, &windowDnrSettings);
	if(bCustom)
	{
		printf("DNR settings:\n");
		printf("MNR mode: (%d) Disable; (%d) Bypass; (%d) Enable; ",
			NEXUS_VideoWindowFilterMode_eDisable, NEXUS_VideoWindowFilterMode_eBypass, NEXUS_VideoWindowFilterMode_eEnable);
		scanf("%d", (int*)&windowDnrSettings.mnr.mode);
		if(windowDnrSettings.mnr.mode == NEXUS_VideoWindowFilterMode_eEnable)
		{
			printf("MNR level (valid range -100 ... 2^31; for filtering, adjQp = Qp * (level + 100) / 100: ");
			scanf("%d", (int*)&windowDnrSettings.mnr.level);
		}
		printf("BNR mode: (%d) Disable; (%d) Bypass; (%d) Enable; ",
			NEXUS_VideoWindowFilterMode_eDisable, NEXUS_VideoWindowFilterMode_eBypass, NEXUS_VideoWindowFilterMode_eEnable);
		scanf("%d", (int*)&windowDnrSettings.bnr.mode);
		if(windowDnrSettings.bnr.mode == NEXUS_VideoWindowFilterMode_eEnable)
		{
			printf("BNR level (valid range -100 ... 2^31): ");
			scanf("%d", (int*)&windowDnrSettings.bnr.level);
		}
		printf("DCR mode: (%d) Disable; (%d) Bypass; (%d) Enable; ",
			NEXUS_VideoWindowFilterMode_eDisable, NEXUS_VideoWindowFilterMode_eBypass, NEXUS_VideoWindowFilterMode_eEnable);
		scanf("%d", (int*)&windowDnrSettings.dcr.mode);
		if(windowDnrSettings.dcr.mode == NEXUS_VideoWindowFilterMode_eEnable)
		{
			printf("DCR level (valid range -100 ... 2^31): ");
			scanf("%d", (int*)&windowDnrSettings.dcr.level);
		}
		if((windowDnrSettings.dcr.mode == NEXUS_VideoWindowFilterMode_eEnable) ||
		   (windowDnrSettings.bnr.mode == NEXUS_VideoWindowFilterMode_eEnable) ||
		   (windowDnrSettings.mnr.mode == NEXUS_VideoWindowFilterMode_eEnable))
		{
			printf("User Qp (default 0 means calculated from decoder source, non-zero is user override): ");
			scanf("%u", &windowDnrSettings.qp);
		}
	}
	else /* default */
	{
		windowDnrSettings.mnr.mode = NEXUS_VideoWindowFilterMode_eEnable;
		windowDnrSettings.bnr.mode = NEXUS_VideoWindowFilterMode_eEnable;
		windowDnrSettings.dcr.mode = NEXUS_VideoWindowFilterMode_eEnable;
		windowDnrSettings.mnr.level = 0;
		windowDnrSettings.bnr.level = 0;
		windowDnrSettings.dcr.level = 0;
		windowDnrSettings.qp = 0;
	}
	NEXUS_VideoWindow_SetDnrSettings(hWindow, &windowDnrSettings);
}

static void window_mad( 
	NEXUS_VideoWindowHandle  hWindow,
	bool bCustom )
{
	NEXUS_VideoWindowMadSettings windowMadSettings;
	
	NEXUS_VideoWindow_GetMadSettings(hWindow, &windowMadSettings);
	if(bCustom)
	{
		printf("MAD settings:\n");
		printf("Enable? (0) Disable; (1) Enable; ");
		scanf("%d", (int*)&windowMadSettings.deinterlace);
		if(windowMadSettings.deinterlace)
		{
			printf("3:2 reverse pulldown? (0) Disable; (1) Enable; ");
			scanf("%d", (int*)&windowMadSettings.enable32Pulldown);
			printf("2:2 reverse pulldown? (0) Disable; (1) Enable; ");
			scanf("%d", (int*)&windowMadSettings.enable22Pulldown);
			printf("Game Mode: (%d) Off (2-field delay, best quality); (%d) 1-field delay; (%d) 0-field delay; ",
				NEXUS_VideoWindowGameMode_eOff, NEXUS_VideoWindowGameMode_e4Fields_1Delay, NEXUS_VideoWindowGameMode_e4Fields_0Delay);
			scanf("%d", (int*)&windowMadSettings.gameMode);
		}
	}
	else /* default */
	{
		windowMadSettings.deinterlace	   = true;
		windowMadSettings.enable32Pulldown = true;
		windowMadSettings.enable22Pulldown = true;
		windowMadSettings.gameMode = NEXUS_VideoWindowGameMode_eOff;
	}
	NEXUS_VideoWindow_SetMadSettings(hWindow, &windowMadSettings);
}
static void
transcoderFinishCallback(void *context, int param)
{
	BKNI_EventHandle finishEvent = (BKNI_EventHandle)context;

	BSTD_UNUSED(param);
	BDBG_WRN(("Finish callback invoked, now stop the stream mux."));
	BKNI_SetEvent(finishEvent);
}

static void lock_callback(void *context, int param)
{
	NEXUS_FrontendHandle frontend = (NEXUS_FrontendHandle)context;
	NEXUS_FrontendQamStatus qamStatus;

	BSTD_UNUSED(param);

	fprintf(stderr, "Lock callback, frontend 0x%08x\n", (unsigned)frontend);

	NEXUS_Frontend_GetQamStatus(frontend, &qamStatus);
	fprintf(stderr, "QAM Lock callback, frontend 0x%08x - lock status %d, %d\n", (unsigned)frontend,
		qamStatus.fecLock, qamStatus.receiverLock);
}

static void tune_qam( TranscodeContext  *pContext )
{
	NEXUS_FrontendQamSettings qamSettings;
	printf("\n Front End QAM freq (Mhz) "); 						 scanf("%d", (int32_t*)&pContext->inputSettings.freq);
	printf("\n Front End QAM Mode (%d) 64 (%d) 256 (%d) 1024	", 
		NEXUS_FrontendQamMode_e64, NEXUS_FrontendQamMode_e256, NEXUS_FrontendQamMode_e1024); 
	scanf("%d", (int32_t*)&pContext->inputSettings.qamMode);

	NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
	qamSettings.frequency = pContext->inputSettings.freq* 1000000;
	qamSettings.mode = pContext->inputSettings.qamMode;
	switch (pContext->inputSettings.qamMode) {
				case NEXUS_FrontendQamMode_e64: qamSettings.symbolRate = 5056900; break;
				case NEXUS_FrontendQamMode_e256: qamSettings.symbolRate = 5360537; break;
				case NEXUS_FrontendQamMode_e1024: qamSettings.symbolRate = 0; /* TODO */ break;
				default: fprintf(stderr, "Invalid QAM mode!\n"); return;
	}
	qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
	qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
	qamSettings.lockCallback.callback = lock_callback;
	qamSettings.lockCallback.context = pContext->frontend;
	NEXUS_Frontend_TuneQam(pContext->frontend, &qamSettings);
}

/* for dynamic encode settings */
static void keyHandler( TranscodeContext  *pContext )
{
	NEXUS_VideoEncoderSettings videoEncoderConfig;
	NEXUS_DisplaySettings displaySettings;
	NEXUS_DisplayCustomFormatSettings customFormatSettings;
	char key[256];
	int choice;
	NEXUS_Error rc;

	for(;;)
	{
		printf("Menu:\n");
		printf("1) change video encoder resolution\n");
		printf("2) change video encoder bitrate\n");
		printf("3) change video encoder frame rate\n");
		printf("4) change xcode video window MAD settings\n");
		printf("5) change xcode video window DNR settings\n");
		printf("6) retune QAM channel\n");

		printf("99) change DEBUG module setting\n");
		printf("\nEnter 'q' to quit\n\n");
		scanf("%s", key);
		if(!strcmp(key, "q")) return;
		choice = strtoul(key, NULL, 0);
		switch(choice)
		{
#if BTST_ENABLE_XCODE_LOOPBACK
			case 0: /* xcoder loopback trick play control */
				/* TODO: add loopback display for HDMI source */
				if(pContext->inputSettings.resource != BTST_RESOURCE_HDMI)
					loopbackPlayer(pContext);
				break;
#endif
			case 1: /* resolution change */
				printf("xcode resolution change:   ");
				printf("\n custom format (0) No (1) Yes "); 
				scanf("%d", (int32_t*)&pContext->encodeSettings.bCustom);
				if(!pContext->encodeSettings.bCustom)
				{
					printf("\n displayFormat (%d) 480i (%d) 480p (%d) 1080i60 (%d) 1080p24 (%d) 1080p30 (%d) 720p60 (%d) 720p24Hz (%d) 720p30Hz: ",
						(NEXUS_VideoFormat_eNtsc),
						(NEXUS_VideoFormat_e480p),
						(NEXUS_VideoFormat_e1080i),
						(NEXUS_VideoFormat_e1080p24hz),
						(NEXUS_VideoFormat_e1080p30hz),
						(NEXUS_VideoFormat_e720p),
						(NEXUS_VideoFormat_e720p24hz),
						(NEXUS_VideoFormat_e720p30hz)); 
					scanf("%d", (int32_t*)&pContext->encodeSettings.displayFormat);
					NEXUS_Display_GetSettings(pContext->displayTranscode, &displaySettings);
					displaySettings.format = pContext->encodeSettings.displayFormat;
					NEXUS_Display_SetSettings(pContext->displayTranscode, &displaySettings);
				}
				else
				{
					printf("\n Resolution width & height	");
					scanf("%d %d", (int32_t*)&pContext->encodeSettings.customFormatSettings.width, (int32_t*)&pContext->encodeSettings.customFormatSettings.height);
					printf("\n Interlaced? (0) No (1) Yes	"); 
					scanf("%d", (int32_t*)&pContext->encodeSettings.customFormatSettings.interlaced);
					printf("\n vertical refresh rate (in 1/1000th Hz) 	"); 
					scanf("%d", (int32_t*)&pContext->encodeSettings.customFormatSettings.refreshRate);
					printf("\n Aspect Ratio (0) Auto (1) 4x3 (2) 16x9	"); 
					scanf("%d", (int32_t*)&pContext->encodeSettings.customFormatSettings.aspectRatio);
					NEXUS_Display_GetDefaultCustomFormatSettings(&customFormatSettings);
					customFormatSettings.width = pContext->encodeSettings.customFormatSettings.width;
					customFormatSettings.height = pContext->encodeSettings.customFormatSettings.height;
					customFormatSettings.refreshRate = pContext->encodeSettings.customFormatSettings.refreshRate;
					customFormatSettings.interlaced = pContext->encodeSettings.customFormatSettings.interlaced;
					customFormatSettings.aspectRatio = pContext->encodeSettings.customFormatSettings.aspectRatio;
					customFormatSettings.dropFrameAllowed = true;
					rc = NEXUS_Display_SetCustomFormatSettings(pContext->displayTranscode, NEXUS_VideoFormat_eCustom2, &customFormatSettings);
					assert(!rc);
				}
				/* resolution change typically could come with bit rate change; separate for now */
				break;
			case 2: /* bitrate change */ 
				printf("Bit rate change from %u to:   ", pContext->encodeSettings.encoderBitrate);
				printf("\n Bitrate (bps)	"); 
				scanf("%d", (int32_t*)&pContext->encodeSettings.encoderBitrate);
				NEXUS_VideoEncoder_GetSettings(pContext->videoEncoder, &videoEncoderConfig);
				videoEncoderConfig.bitrateMax = pContext->encodeSettings.encoderBitrate;
				NEXUS_VideoEncoder_SetSettings(pContext->videoEncoder, &videoEncoderConfig);
				break;
			case 3: /* frame rate change */
				printf("frame rate change:   ");
				printf("\n Frame rate (1) 23.976 (2) 24 (4) 29.97Hz (5) 30 Hz (7) 59.94 (8) 60	"); 
				scanf("%d", (int32_t*)&pContext->encodeSettings.encoderFrameRate);
				NEXUS_VideoEncoder_GetSettings(pContext->videoEncoder, &videoEncoderConfig);
				videoEncoderConfig.frameRate = pContext->encodeSettings.encoderFrameRate;
				NEXUS_VideoEncoder_SetSettings(pContext->videoEncoder, &videoEncoderConfig);
				break;
			case 4: /* MAD setting */
				/* window PQ custom setting */
				window_mad(pContext->windowTranscode, true);
				break;
			case 5: /* DNR setting */
				/* window PQ custom setting */
				window_dnr(pContext->windowTranscode, true);
				break;
			case 6: /* retune source channel */
				tune_qam(pContext);
				break;
			case 99: /* debug module setting */
#if BDBG_DEBUG_BUILD
{
				int  iDbgLevel;
				char achName[256];
				printf("\nPlease enter the debug module name: ");
				scanf("%s", achName);
				printf("(%d)Trace (%d)Message (%d)Warning (%d)Error\n",
					BDBG_eTrace, BDBG_eMsg, BDBG_eWrn, BDBG_eErr);
				printf("Which debug level do you want to set it to? ");
				scanf("%d", &iDbgLevel);
				BDBG_SetModuleLevel(achName, (BDBG_Level)iDbgLevel); 
}
#endif				
				break;
			default:
				break;
		}
	}
}

static int transcode_ts(
	TranscodeContext  *pContext )
{
	NEXUS_FrontendUserParameters userParams;
	NEXUS_FrontendQamSettings qamSettings;

	NEXUS_ParserBand parserBand;
	NEXUS_ParserBandSettings parserBandSettings;

	NEXUS_PlatformSettings platformSettings;
	NEXUS_StcChannelSettings stcSettings;
	NEXUS_VideoDecoderStartSettings videoProgram;

	NEXUS_AudioDecoderStartSettings audioProgram;
	NEXUS_AudioMuxOutputDelayStatus audioDelayStatus;
	NEXUS_AudioMuxOutputStartSettings audioMuxStartSettings;
	NEXUS_AudioEncoderSettings encoderSettings;
	NEXUS_AudioCodec audioCodec;

	NEXUS_VideoEncoderSettings videoEncoderConfig;
	NEXUS_VideoEncoderStartSettings videoEncoderStartConfig;
	NEXUS_VideoEncoderDelayRange videoDelay;
	NEXUS_RecordPidChannelSettings recordPidSettings;
	NEXUS_RecordSettings recordSettings;

	NEXUS_StreamMuxCreateSettings muxCreateSettings;
	NEXUS_StreamMuxStartSettings muxConfig;
	NEXUS_StreamMuxOutput muxOutput;
	NEXUS_PlaypumpOpenSettings playpumpConfig;

#if NEXUS_HAS_SYNC_CHANNEL
	NEXUS_SyncChannelSettings syncChannelSettings;
#endif
	void *pat;
	void *pmt;
	unsigned i;

	NEXUS_DisplaySettings displaySettings;
	NEXUS_DisplayCustomFormatSettings customFormatSettings;
	NEXUS_Error rc;
	EncodeSettings  *pEncodeSettings = &pContext->encodeSettings;
	InputSettings   *pInputSettings = &pContext->inputSettings; 


	NEXUS_Platform_GetDefaultSettings(&platformSettings);
	platformSettings.openFrontend = true;
	NEXUS_Platform_Init(&platformSettings);
	NEXUS_Platform_GetConfiguration(&pContext->platformConfig);

#if NEXUS_HAS_SYNC_CHANNEL
	/* create a sync channel */
	NEXUS_SyncChannel_GetDefaultSettings(&syncChannelSettings);
	pContext->syncChannel = NEXUS_SyncChannel_Create(&syncChannelSettings);
#endif

	NEXUS_StcChannel_GetDefaultSettings(BTST_SOURCE_STC_IDX, &stcSettings);
	stcSettings.timebase = NEXUS_Timebase_e0;
	stcSettings.mode = NEXUS_StcChannelMode_eAuto;
	pContext->stcChannel = NEXUS_StcChannel_Open(BTST_SOURCE_STC_IDX, &stcSettings);

	/* encoders/mux require different STC broadcast mode from decoder */
	NEXUS_StcChannel_GetDefaultSettings(BTST_ENCODER_STC_IDX, &stcSettings);
	stcSettings.timebase = NEXUS_Timebase_e0;/* should be the same timebase for end-to-end locking */
	stcSettings.mode = NEXUS_StcChannelMode_eAuto;
	stcSettings.pcrBits = NEXUS_StcChannel_PcrBits_eFull42;/* ViCE2 requires 42-bit STC broadcast */
	pContext->stcChannelTranscode = NEXUS_StcChannel_Open(2, &stcSettings);
	
	/* bring up decoder and connect to local display */
	pContext->videoDecoder = NEXUS_VideoDecoder_Open(0, NULL); /* take default capabilities */

	/****************************************
	 * set up xcoder source
	 */
	for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
	{
		NEXUS_FrontendCapabilities capabilities;
		pContext->frontend = pContext->platformConfig.frontend[i];
		if (pContext->frontend) {
			NEXUS_Frontend_GetCapabilities(pContext->frontend, &capabilities);
			/* Does this frontend support qam? */
			if ( capabilities.qam )
			{
				break;
			}
		}
	}
	
	if (NULL == pContext->frontend )
	{
		fprintf(stderr, "Unable to find QAM-capable frontend\n");
		return -1;
	}
	
	NEXUS_Frontend_GetDefaultQamSettings(&qamSettings);
	qamSettings.frequency = pInputSettings->freq* 1000000;
	qamSettings.mode = pInputSettings->qamMode;
	switch (pInputSettings->qamMode) {
				case NEXUS_FrontendQamMode_e64: qamSettings.symbolRate = 5056900; break;
				case NEXUS_FrontendQamMode_e256: qamSettings.symbolRate = 5360537; break;
				case NEXUS_FrontendQamMode_e1024: qamSettings.symbolRate = 0; /* TODO */ break;
				default: fprintf(stderr, "Invalid QAM mode!\n"); return -1;
	}
	qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
	qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
	qamSettings.lockCallback.callback = lock_callback;
	qamSettings.lockCallback.context = pContext->frontend;
	
	NEXUS_Frontend_GetUserParameters(pContext->frontend, &userParams);
	
	/* Map a parser band to the demod's input band. */
	parserBand = NEXUS_ParserBand_e0;
	NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
	parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
	parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
	parserBandSettings.transportType = NEXUS_TransportType_eTs;
	NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);
	pContext->videoPidChannel = NEXUS_PidChannel_Open(parserBand, pInputSettings->iVideoPid, NULL);

	/* Tuner input has live PCR locked timebase */
	NEXUS_StcChannel_GetSettings(pContext->stcChannel, &stcSettings);
	stcSettings.mode = NEXUS_StcChannelMode_ePcr; /* live */
	stcSettings.modeSettings.pcr.pidChannel = pContext->videoPidChannel; /* PCR happens to be on video pid */
	NEXUS_StcChannel_SetSettings(pContext->stcChannel, &stcSettings);
	NEXUS_Frontend_TuneQam(pContext->frontend, &qamSettings);
 
	/* Set up decoder Start structures now. We need to know the audio codec to properly set up
	the audio outputs. */
	NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
	videoProgram.codec = (NEXUS_VideoCodec)pInputSettings->eVideoCodec;
	videoProgram.pidChannel = pContext->videoPidChannel;
	videoProgram.stcChannel = pContext->stcChannel;

	/****************************************
	 * Bring up local video display and outputs 
	 */
	NEXUS_Display_GetDefaultSettings(&displaySettings);
	displaySettings.format = NEXUS_VideoFormat_e480p;
	pContext->display = NEXUS_Display_Open(0, &displaySettings);
#if NEXUS_NUM_COMPONENT_OUTPUTS
	if(pContext->platformConfig.outputs.component[0]){
		NEXUS_Display_AddOutput(pContext->display, NEXUS_ComponentOutput_GetConnector(pContext->platformConfig.outputs.component[0]));
	}
#endif
#if NEXUS_NUM_HDMI_OUTPUTS
	if(pContext->platformConfig.outputs.hdmi[0]){
		NEXUS_Display_AddOutput(pContext->display, NEXUS_HdmiOutput_GetVideoConnector(pContext->platformConfig.outputs.hdmi[0]));
	}
#endif
	pContext->window = NEXUS_VideoWindow_Open(pContext->display, 0);

	/* NOTE: must open video encoder before display; otherwise open will init ViCE2 core
	* which might cause encoder display GISB error since encoder display would
	* trigger RDC to program mailbox registers in ViCE2;
	*/
	pContext->videoEncoder = NEXUS_VideoEncoder_Open(0, NULL);
	assert(pContext->videoEncoder);

	/* Bring up video encoder display */
	NEXUS_Display_GetDefaultSettings(&displaySettings);
	displaySettings.displayType = NEXUS_DisplayType_eAuto;
	displaySettings.timingGenerator = NEXUS_DisplayTimingGenerator_eEncoder;
	displaySettings.frameRateMaster = NULL;/* disable frame rate tracking for now */

 	pContext->displayTranscode = NEXUS_Display_Open(NEXUS_ENCODER_DISPLAY_IDX, &displaySettings);
	assert(pContext->displayTranscode);
	pContext->windowTranscode = NEXUS_VideoWindow_Open(pContext->displayTranscode, 0);
	assert(pContext->windowTranscode);

	NEXUS_Display_GetDefaultCustomFormatSettings(&customFormatSettings);
	customFormatSettings.width = pEncodeSettings->customFormatSettings.width;
	customFormatSettings.height = pEncodeSettings->customFormatSettings.height;
	customFormatSettings.refreshRate = pEncodeSettings->customFormatSettings.refreshRate;
	customFormatSettings.interlaced = pEncodeSettings->customFormatSettings.interlaced;
	customFormatSettings.aspectRatio = pEncodeSettings->customFormatSettings.aspectRatio;
	customFormatSettings.dropFrameAllowed = true;
	rc = NEXUS_Display_SetCustomFormatSettings(pContext->displayTranscode, NEXUS_VideoFormat_eCustom2, &customFormatSettings);
	assert(!rc);

 	/* connect same decoder to the encoder display;
	* NOTE: simul display + transcode mode might have limitation in audio path;
	* here is for video transcode bringup/debug purpose;
	*/
	NEXUS_VideoWindow_AddInput(pContext->windowTranscode, NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder));
	NEXUS_VideoWindow_AddInput(pContext->window, NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder));

	/* window PQ default setting */
	window_mad(pContext->window, false);
	window_mad(pContext->windowTranscode, false);
	window_dnr(pContext->windowTranscode, false);

	/**************************************
	 * encoder settings 
	 */
	NEXUS_VideoEncoder_GetSettings(pContext->videoEncoder, &videoEncoderConfig);
	videoEncoderConfig.variableFrameRate = false;
	videoEncoderConfig.frameRate = pEncodeSettings->encoderFrameRate;
	videoEncoderConfig.bitrateMax = pEncodeSettings->encoderBitrate;
	videoEncoderConfig.streamStructure.trackInput = false;
	videoEncoderConfig.streamStructure.framesP = pEncodeSettings->encoderGopStructureFramesP;
	videoEncoderConfig.streamStructure.framesB = pEncodeSettings->encoderGopStructureFramesB;

	NEXUS_VideoEncoder_GetDefaultStartSettings(&videoEncoderStartConfig);
	videoEncoderStartConfig.codec = pEncodeSettings->encoderVideoCodec;
	videoEncoderStartConfig.profile = pEncodeSettings->encoderProfile;
	videoEncoderStartConfig.level = pEncodeSettings->encoderLevel;
	videoEncoderStartConfig.input = pContext->displayTranscode;
	videoEncoderStartConfig.stcChannel = pContext->stcChannelTranscode;

	/*********************************
	 * Set up audio xcoder 
	 */
	if(pInputSettings->bAudioInput)
	{
		/* Open the audio decoder */
		pContext->audioDecoder = NEXUS_AudioDecoder_Open(0, NULL);
		pContext->audioPassthrough = NEXUS_AudioDecoder_Open(1, NULL);

		/* Open the audio and pcr pid channel */
		pContext->audioPidChannel = NEXUS_PidChannel_Open(parserBand, pInputSettings->iAudioPid, NULL);

		/* Set up decoder Start structures now. We need to know the audio codec to properly set up
		the audio outputs. */
		NEXUS_AudioDecoder_GetDefaultStartSettings(&audioProgram);

		audioProgram.codec = pInputSettings->eAudioCodec;
		audioProgram.pidChannel = pContext->audioPidChannel;
		audioProgram.stcChannel = pContext->stcChannel;

		/* Connect audio decoders to outputs */
		NEXUS_AudioOutput_AddInput(
			NEXUS_AudioDac_GetConnector(pContext->platformConfig.outputs.audioDacs[0]),
			NEXUS_AudioDecoder_GetConnector(pContext->audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
		NEXUS_AudioOutput_AddInput(
			NEXUS_SpdifOutput_GetConnector(pContext->platformConfig.outputs.spdif[0]),
			NEXUS_AudioDecoder_GetConnector(pContext->audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));

		/* Open audio mux output */
		pContext->audioMuxOutput = NEXUS_AudioMuxOutput_Create(NULL);
		assert(pContext->audioMuxOutput);

		if(pEncodeSettings->bAudioEncode)
		{
			/* Open audio encoder */
			NEXUS_AudioEncoder_GetDefaultSettings(&encoderSettings);
			encoderSettings.codec = pEncodeSettings->encoderAudioCodec;
			audioCodec = pEncodeSettings->encoderAudioCodec;
			pContext->audioEncoder = NEXUS_AudioEncoder_Open(&encoderSettings);
			assert(pContext->audioEncoder);
			/* Connect encoder to decoder */
			NEXUS_AudioEncoder_AddInput(pContext->audioEncoder,
				NEXUS_AudioDecoder_GetConnector(pContext->audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
			/* Connect mux to encoder */
			NEXUS_AudioOutput_AddInput(
				NEXUS_AudioMuxOutput_GetConnector(pContext->audioMuxOutput), NEXUS_AudioEncoder_GetConnector(pContext->audioEncoder));
		}
		else
		{
			NEXUS_AudioOutput_AddInput(
				NEXUS_AudioMuxOutput_GetConnector(pContext->audioMuxOutput),
				NEXUS_AudioDecoder_GetConnector(pContext->audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
			audioCodec = audioProgram.codec;
		}

		/* audio playpump here is for ts muxer */
		NEXUS_Playpump_GetDefaultOpenSettings(&playpumpConfig);
		playpumpConfig.fifoSize = 16384; /* reduce FIFO size allocated for playpump */
		playpumpConfig.numDescriptors = 64; /* set number of descriptors */
		playpumpConfig.streamMuxCompatible = true;
		pContext->playpumpTranscodeAudio = NEXUS_Playpump_Open(BTST_STREAM_MUX_AUDIO_PLAYPUMP_IDX, &playpumpConfig);
		assert(pContext->playpumpTranscodeAudio);
	}

	/******************************************
	 * add configurable delay to video path 
	 */
	/* NOTE: ITFP is encoder feature to detect and lock on 3:2/2:2 cadence in the video content to help 
	 * efficient coding for interlaced formats; disabling ITFP will impact the bit efficiency but reduce the encode delay. */
	videoEncoderConfig.enableFieldPairing = true;

	/* 0 to use default 750ms rate buffer delay; TODO: allow user to adjust it to lower encode delay at cost of quality reduction! */
	videoEncoderStartConfig.rateBufferDelay = 0;

	/* to allow 23.976p passthru; TODO: allow user to configure minimum framerate to achieve lower delay!
	 * Note: lower minimum framerate means longer encode delay */
	videoEncoderStartConfig.bounds.inputFrameRate.min = NEXUS_VideoFrameRate_e23_976;

	/* to allow 24 ~ 60p dynamic frame rate coding TODO: allow user to config higher minimum frame rate for lower delay! */
	videoEncoderStartConfig.bounds.outputFrameRate.min = NEXUS_VideoFrameRate_e23_976;
	videoEncoderStartConfig.bounds.outputFrameRate.max = NEXUS_VideoFrameRate_e60;

	/* max encode size allows 1080p encode; TODO: allow user to choose lower max resolution for lower encode delay */
	videoEncoderStartConfig.bounds.inputDimension.max.width = 1920;
	videoEncoderStartConfig.bounds.inputDimension.max.height = 1088;

	/************************************
	 * Set up encoder AV sync.
	 * encode setting and startSetting to be set after end-to-end delay is determined */
	/* get end-to-end delay (Dee) for audio and video encoders;
	* TODO: match AV delay! In other words,
	*   if (aDee > vDee) {
	*       vDee' = aDee' = aDee;
	*   }
	*   else {
	*       vDee' = aDee' = vDee;
	*   }
	*/
	{
		unsigned Dee;

		/* NOTE: video encoder delay is in 27MHz ticks */
		NEXUS_VideoEncoder_GetDelayRange(pContext->videoEncoder, &videoEncoderConfig, &videoEncoderStartConfig, &videoDelay);
		BDBG_WRN(("\n\tVideo encoder end-to-end delay = [%u ~ %u] ms", videoDelay.min/27000, videoDelay.max/27000));

		if(pInputSettings->bAudioInput)
		{
			NEXUS_AudioMuxOutput_GetDelayStatus(pContext->audioMuxOutput, audioCodec, &audioDelayStatus);
			BDBG_WRN(("\n\tAudio codec %d end-to-end delay = %u ms", audioCodec, audioDelayStatus.endToEndDelay));

			Dee = audioDelayStatus.endToEndDelay * 27000; /* in 27MHz ticks */
			if(Dee > videoDelay.min)
			{
				if(Dee > videoDelay.max)
				{
					BDBG_ERR(("Audio Dee is way too big! Use video Dee max!"));
					Dee = videoDelay.max;
				}
				else
				{
					BDBG_WRN(("Use audio Dee %u ms %u ticks@27Mhz!", Dee/27000, Dee));
				}
			}
			else
			{
				Dee = videoDelay.min;
				BDBG_WRN(("Use video Dee %u ms %u ticks@27Mhz!", Dee/27000, Dee));
			}
			videoEncoderConfig.encoderDelay = Dee;

			/* Start audio mux output */
			NEXUS_AudioMuxOutput_GetDefaultStartSettings(&audioMuxStartSettings);
			audioMuxStartSettings.stcChannel = pContext->stcChannelTranscode;
			audioMuxStartSettings.presentationDelay = Dee/27000;/* in ms */
			NEXUS_AudioMuxOutput_Start(pContext->audioMuxOutput, &audioMuxStartSettings);
		}
		else
			videoEncoderConfig.encoderDelay = videoDelay.min;
	}

	NEXUS_VideoEncoder_SetSettings(pContext->videoEncoder, &videoEncoderConfig);
	NEXUS_VideoEncoder_Start(pContext->videoEncoder, &videoEncoderStartConfig);

	/************************************************
	 * Set up xcoder stream_mux
	 */
	NEXUS_Playpump_GetDefaultOpenSettings(&playpumpConfig);
	playpumpConfig.fifoSize = 16384; /* reduce FIFO size allocated for playpump */
	playpumpConfig.numDescriptors = 64; /* set number of descriptors */
	playpumpConfig.streamMuxCompatible = true;
	pContext->playpumpTranscodeVideo = NEXUS_Playpump_Open(BTST_STREAM_MUX_VIDEO_PLAYPUMP_IDX, &playpumpConfig);
	assert(pContext->playpumpTranscodeVideo);
#if BTST_DISABLE_VIDEO_ENCODER_PES_PACING
	NEXUS_Playpump_SuspendPacing(pContext->playpumpTranscodeVideo, true);
#endif

	pContext->playpumpTranscodePcr = NEXUS_Playpump_Open(BTST_STREAM_MUX_PCR_PLAYPUMP_IDX, &playpumpConfig);
	assert(pContext->playpumpTranscodePcr);

	BKNI_CreateEvent(&pContext->finishEvent);
	NEXUS_StreamMux_GetDefaultCreateSettings(&muxCreateSettings);
	muxCreateSettings.finished.callback = transcoderFinishCallback;
	muxCreateSettings.finished.context = pContext->finishEvent;
	pContext->streamMux = NEXUS_StreamMux_Create(&muxCreateSettings);
	NEXUS_StreamMux_GetDefaultStartSettings(&muxConfig);
	muxConfig.transportType = NEXUS_TransportType_eTs;
	muxConfig.stcChannel = pContext->stcChannelTranscode;

	muxConfig.video[0].pid = BTST_MUX_VIDEO_PID;
	muxConfig.video[0].encoder = pContext->videoEncoder;
	muxConfig.video[0].playpump = pContext->playpumpTranscodeVideo;

	if(pInputSettings->bAudioInput)
	{
		muxConfig.audio[0].pid = BTST_MUX_AUDIO_PID;
		muxConfig.audio[0].muxOutput = pContext->audioMuxOutput;
		muxConfig.audio[0].playpump = pContext->playpumpTranscodeAudio;
	}

	muxConfig.pcr.pid = BTST_MUX_PCR_PID;
	muxConfig.pcr.playpump = pContext->playpumpTranscodePcr;
	muxConfig.pcr.interval = 50;

	/* open PidChannels */
	pContext->pidChannelTranscodePcr = NEXUS_Playpump_OpenPidChannel(pContext->playpumpTranscodePcr, muxConfig.pcr.pid, NULL);
	assert(pContext->pidChannelTranscodePcr);
	pContext->pidChannelTranscodePmt = NEXUS_Playpump_OpenPidChannel(pContext->playpumpTranscodePcr, BTST_MUX_PMT_PID, NULL);
	assert(pContext->pidChannelTranscodePmt);
	pContext->pidChannelTranscodePat = NEXUS_Playpump_OpenPidChannel(pContext->playpumpTranscodePcr, BTST_MUX_PAT_PID, NULL);
	assert(pContext->pidChannelTranscodePat);

	/* start mux */
	NEXUS_StreamMux_Start(pContext->streamMux,&muxConfig, &muxOutput);
	pContext->pidChannelTranscodeVideo = muxOutput.video[0];

	/******************************************8
	 * Set up xcoder record output
	 */
	pContext->recpump = NEXUS_Recpump_Open(0, NULL);
	assert(pContext->recpump);

	pContext->record = NEXUS_Record_Create();
	assert(pContext->record);

	NEXUS_Record_GetSettings(pContext->record, &recordSettings);
	recordSettings.recpump = pContext->recpump;
	NEXUS_Record_SetSettings(pContext->record, &recordSettings);

	/* configure the video pid for indexing */
	NEXUS_Record_GetDefaultPidChannelSettings(&recordPidSettings);
	recordPidSettings.recpumpSettings.pidType = NEXUS_PidType_eVideo;
	recordPidSettings.recpumpSettings.pidTypeSettings.video.index = true;
	recordPidSettings.recpumpSettings.pidTypeSettings.video.codec = pEncodeSettings->encoderVideoCodec;

	/* add multiplex data to the same record */
	NEXUS_Record_AddPidChannel(pContext->record, pContext->pidChannelTranscodeVideo, &recordPidSettings);

	if(pInputSettings->bAudioInput)
	{
		pContext->pidChannelTranscodeAudio = muxOutput.audio[0];
		NEXUS_Record_AddPidChannel(pContext->record, pContext->pidChannelTranscodeAudio, NULL);
	}

	NEXUS_Record_AddPidChannel(pContext->record, pContext->pidChannelTranscodePcr, NULL);
	NEXUS_Record_AddPidChannel(pContext->record, pContext->pidChannelTranscodePat, NULL);
	NEXUS_Record_AddPidChannel(pContext->record, pContext->pidChannelTranscodePmt, NULL);

	/* set record index file name and open the record file handle */
	pContext->fileTranscode = NEXUS_FileRecord_OpenPosix(pEncodeSettings->fname, pEncodeSettings->indexfname);
	assert(pContext->fileTranscode);

	/* Start record of stream mux output */
	NEXUS_Record_Start(pContext->record, pContext->fileTranscode);

	/****************************
	 * set up decoder AV sync
	 */
#if NEXUS_HAS_SYNC_CHANNEL
	/* connect sync channel */
	NEXUS_SyncChannel_GetSettings(pContext->syncChannel, &syncChannelSettings);
	syncChannelSettings.videoInput = NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder);
	if(pInputSettings->bAudioInput)
	{
		syncChannelSettings.audioInput[0] = NEXUS_AudioDecoder_GetConnector(pContext->audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo);
		syncChannelSettings.audioInput[1] = NEXUS_AudioDecoder_GetConnector(pContext->audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed);
	}

	NEXUS_SyncChannel_SetSettings(pContext->syncChannel, &syncChannelSettings);
#endif

	/* Start decoder */
	NEXUS_VideoDecoder_Start(pContext->videoDecoder, &videoProgram);
	if(pInputSettings->bAudioInput)
	{
		NEXUS_AudioDecoder_Start(pContext->audioDecoder, &audioProgram);
		NEXUS_AudioDecoder_Start(pContext->audioPassthrough, &audioProgram);
	}
	
	/*******************************
	 * Add system data to stream_mux
	 */
	{
		NEXUS_StreamMuxSystemData psi[2];
        /* Get CRC right to be playable by VLCplayer etc 3rd party SW */
        uint32_t uiCRC = CRC32_mpeg(0, (uint8_t *) s_auiTSPacket_PAT + 184 - (8+4*1), 8+4*1);
        fprintf(stderr, "PAT crc=%x\n", uiCRC);
        NEXUS_Memory_Allocate(188, NULL, &pat);
        NEXUS_Memory_Allocate(188, NULL, &pmt);
        BKNI_Memcpy(pat, s_auiTSPacket_PAT, sizeof(s_auiTSPacket_PAT));
        BKNI_Memcpy(pmt, s_auiTSPacket_PMT, sizeof(s_auiTSPacket_PMT));
        ((uint8_t*)pat)[184] = (uiCRC >> 24) & 0xFF;
        ((uint8_t*)pat)[185] = (uiCRC >> 16) & 0xFF;
        ((uint8_t*)pat)[186] = (uiCRC >>  8) & 0xFF;
        ((uint8_t*)pat)[187] = (uiCRC >>  0) & 0xFF;

		/* video codec */
		switch(videoEncoderStartConfig.codec)
		{
			case NEXUS_VideoCodec_eMpeg2:         ((uint8_t *) pmt)[184-2*5] = 0x2; break;
			case NEXUS_VideoCodec_eMpeg4Part2:    ((uint8_t *) pmt)[184-2*5] = 0x10; break;
			case NEXUS_VideoCodec_eH264:          ((uint8_t *) pmt)[184-2*5] = 0x1b; break;
			case NEXUS_VideoCodec_eVc1SimpleMain: ((uint8_t *) pmt)[184-2*5] = 0xea; break;
			default:
				BDBG_ERR(("Video encoder codec %d is not supported!\n", videoEncoderStartConfig.codec));
				BDBG_ASSERT(0);
		}
        /* audio stream type */
		switch(audioCodec)
		{
			case NEXUS_AudioCodec_eMpeg:         ((uint8_t *) pmt)[184-5] = 0x4; break;
			case NEXUS_AudioCodec_eMp3:          ((uint8_t *) pmt)[184-5] = 0x4; break;
			case NEXUS_AudioCodec_eAac    :      ((uint8_t *) pmt)[184-5] = 0xf; break; /* ADTS */
			case NEXUS_AudioCodec_eAacPlus:      ((uint8_t *) pmt)[184-5] = 0x11; break;/* LOAS */
			/* MP2TS doesn't allow 14496-3 AAC+ADTS; here is placeholder to test AAC-HE before LOAS encode is supported; */
			case NEXUS_AudioCodec_eAacPlusAdts:  ((uint8_t *) pmt)[184-5] = 0x11; break;
			case NEXUS_AudioCodec_eAc3:          ((uint8_t *) pmt)[184-5] = 0x81; break;
			default:
				BDBG_ERR(("Audio encoder codec %d is not supported!\n", audioCodec));
		}
        /* A+V two ES */
        uiCRC = CRC32_mpeg(0, (uint8_t *) pmt + 184 - (12+5*2), 12+5*2);
        fprintf(stderr, "PMT crc=%x\n", uiCRC);

        ((uint8_t*)pmt)[184] = (uiCRC >> 24) & 0xFF;
        ((uint8_t*)pmt)[185] = (uiCRC >> 16) & 0xFF;
        ((uint8_t*)pmt)[186] = (uiCRC >>  8) & 0xFF;
        ((uint8_t*)pmt)[187] = (uiCRC >>  0) & 0xFF;
		NEXUS_Memory_FlushCache(pat, sizeof(s_auiTSPacket_PAT));
		NEXUS_Memory_FlushCache(pmt, sizeof(s_auiTSPacket_PMT));
		BKNI_Memset(psi, 0, sizeof(psi));
		psi[0].size = 188;
		psi[0].pData = pat;
		psi[0].timestampDelta = 100;
		psi[1].size = 188;
		psi[1].pData = pmt;
		psi[1].timestampDelta = 100;
		NEXUS_StreamMux_AddSystemDataBuffer(pContext->streamMux, &psi[0]);
		NEXUS_StreamMux_AddSystemDataBuffer(pContext->streamMux, &psi[1]);
	}

	/* wait for 'q' to exit the test */
	keyHandler( pContext );


	/************************************
	 * Bring down system 
	 */

	/**************************************************
	 * NOTE: stop sequence should be in front->back order
	 */
	NEXUS_VideoDecoder_Stop(pContext->videoDecoder);
	if(pInputSettings->bAudioInput)
	{
		NEXUS_AudioDecoder_Stop(pContext->audioPassthrough);
	
		NEXUS_AudioDecoder_Stop(pContext->audioDecoder);
		NEXUS_AudioMuxOutput_Stop(pContext->audioMuxOutput);
	}
#if NEXUS_HAS_SYNC_CHANNEL
	/* disconnect sync channel after decoders stop */
	NEXUS_SyncChannel_GetSettings(pContext->syncChannel, &syncChannelSettings);
	syncChannelSettings.videoInput = NULL;
	syncChannelSettings.audioInput[0] = NULL;
	syncChannelSettings.audioInput[1] = NULL;
	NEXUS_SyncChannel_SetSettings(pContext->syncChannel, &syncChannelSettings);
#endif

	
	NEXUS_VideoEncoder_Stop(pContext->videoEncoder);

	/* finish stream mux to clean shutdown */
	NEXUS_StreamMux_Finish(pContext->streamMux);
	if(BKNI_WaitForEvent(pContext->finishEvent, 2000)!=BERR_SUCCESS) {
		fprintf(stderr, "TIMEOUT\n");
	}
	BKNI_DestroyEvent(pContext->finishEvent);
	NEXUS_Record_Stop(pContext->record);
	/*****************************************
	 * Note: remove all record PID channels before stream 
	 * mux stop since streammux would close the A/V PID channels 
	 */
	NEXUS_Record_RemoveAllPidChannels(pContext->record);
	NEXUS_StreamMux_Stop(pContext->streamMux);

	NEXUS_Record_Destroy(pContext->record);
	NEXUS_Recpump_Close(pContext->recpump);
	NEXUS_FileRecord_Close(pContext->fileTranscode);

	if(pInputSettings->bAudioInput)
		NEXUS_PidChannel_Close(pContext->audioPidChannel);
	NEXUS_PidChannel_Close(pContext->videoPidChannel);

	NEXUS_Playpump_ClosePidChannel(pContext->playpumpTranscodePcr, pContext->pidChannelTranscodePcr);
	NEXUS_Playpump_ClosePidChannel(pContext->playpumpTranscodePcr, pContext->pidChannelTranscodePat);
	NEXUS_Playpump_ClosePidChannel(pContext->playpumpTranscodePcr, pContext->pidChannelTranscodePmt);

	/******************************************
	 * nexus kernel mode requires explicit remove/shutdown video inputs before close windows/display
	 */
	NEXUS_VideoWindow_RemoveInput(pContext->window, NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder));
	NEXUS_VideoWindow_RemoveInput(pContext->windowTranscode, NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder));
	NEXUS_VideoInput_Shutdown(NEXUS_VideoDecoder_GetConnector(pContext->videoDecoder));
	NEXUS_VideoDecoder_Close(pContext->videoDecoder);
	NEXUS_VideoWindow_Close(pContext->window);
	NEXUS_VideoWindow_Close(pContext->windowTranscode);
	NEXUS_Display_Close(pContext->display);
	NEXUS_Display_Close(pContext->displayTranscode);

	NEXUS_StreamMux_Destroy(pContext->streamMux);

	NEXUS_Playpump_Close(pContext->playpumpTranscodePcr);
	
	NEXUS_Playpump_Close(pContext->playpumpTranscodeVideo);
	NEXUS_VideoEncoder_Close(pContext->videoEncoder);
	
	if(pInputSettings->bAudioInput)
	{
		if(pEncodeSettings->bAudioEncode)
		{
			NEXUS_AudioOutput_RemoveInput(
				NEXUS_AudioMuxOutput_GetConnector(pContext->audioMuxOutput), NEXUS_AudioEncoder_GetConnector(pContext->audioEncoder));
			NEXUS_AudioEncoder_RemoveInput(pContext->audioEncoder,
										NEXUS_AudioDecoder_GetConnector(pContext->audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
			NEXUS_AudioInput_Shutdown(NEXUS_AudioEncoder_GetConnector(pContext->audioEncoder));
			NEXUS_AudioOutput_Shutdown(NEXUS_AudioMuxOutput_GetConnector(pContext->audioMuxOutput));
			NEXUS_AudioEncoder_Close(pContext->audioEncoder);
		}
		else
		{
			NEXUS_AudioOutput_RemoveInput(
				NEXUS_AudioMuxOutput_GetConnector(pContext->audioMuxOutput),
				NEXUS_AudioDecoder_GetConnector(pContext->audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
		}
		NEXUS_AudioOutput_RemoveAllInputs(NEXUS_AudioDac_GetConnector(pContext->platformConfig.outputs.audioDacs[0]));
		NEXUS_AudioOutput_RemoveAllInputs(NEXUS_SpdifOutput_GetConnector(pContext->platformConfig.outputs.spdif[0]));
	
		NEXUS_Playpump_Close(pContext->playpumpTranscodeAudio);
		NEXUS_AudioMuxOutput_Destroy(pContext->audioMuxOutput);
	
		NEXUS_AudioInput_Shutdown(NEXUS_AudioDecoder_GetConnector(pContext->audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
		NEXUS_AudioInput_Shutdown(NEXUS_AudioDecoder_GetConnector(pContext->audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
		NEXUS_AudioOutput_Shutdown(NEXUS_AudioDac_GetConnector(pContext->platformConfig.outputs.audioDacs[0]));
		NEXUS_AudioOutput_Shutdown(NEXUS_SpdifOutput_GetConnector(pContext->platformConfig.outputs.spdif[0]));
	
		NEXUS_AudioDecoder_Close(pContext->audioDecoder);
		NEXUS_AudioDecoder_Close(pContext->audioPassthrough);
	}

#if NEXUS_HAS_SYNC_CHANNEL
	NEXUS_SyncChannel_Destroy(pContext->syncChannel);
#endif

	NEXUS_StcChannel_Close(pContext->stcChannel);
	NEXUS_StcChannel_Close(pContext->stcChannelTranscode);
	NEXUS_Memory_Free(pat);
	NEXUS_Memory_Free(pmt);

	NEXUS_Platform_Uninit();

	return (0);
}
#endif

int main(int argc, char **argv)
{
#if NEXUS_HAS_VIDEO_ENCODER && NEXUS_HAS_STREAM_MUX
	EncodeSettings  *pEncodeSettings = &xcodeContext.encodeSettings;
	InputSettings   *pInputSettings = &xcodeContext.inputSettings; 
    unsigned freq = 603;
    NEXUS_FrontendQamMode qamMode = NEXUS_FrontendQamMode_e256;
	unsigned videopid = 257;
	unsigned audiopid = 258;
	unsigned vidcodec = NEXUS_VideoCodec_eH264;
	unsigned audcodec = NEXUS_AudioCodec_eAc3;

    /* allow cmdline freq & qam mode for simple test */
    if (argc > 1) {
        freq = atoi(argv[1]);
    }
    if (argc > 2) {
        unsigned mode = atoi(argv[2]);
        switch (mode) {
        case 64: 
			qamMode = NEXUS_FrontendQamMode_e64; 
			break;
        case 256: 
			qamMode = NEXUS_FrontendQamMode_e256; 
			break;
        case 1024: 
			qamMode = NEXUS_FrontendQamMode_e1024; 
			break;
        default: printf("unknown qam mode %d\n", mode); 
			return -1;
        }
    }
    if (argc > 3) {
        videopid = atoi(argv[3]);
		if (argc > 4) {
			audiopid = atoi(argv[4]);
			if (argc > 5) {
				vidcodec = atoi(argv[5]);
				if (argc > 6) {
					audcodec = atoi(argv[6]);
				}
			}
		}
	}
 	fprintf(stderr, "\n videopid %d audiopid %d vidcodec %d audcodec %d\n", videopid, audiopid, vidcodec, audcodec);
	if (vidcodec == (unsigned)-1)
		vidcodec = NEXUS_VideoCodec_eH264;
	else {
		switch(vidcodec)
		{
		case 2:
			vidcodec = NEXUS_VideoCodec_eMpeg2;
			break;
		case 4:
			vidcodec = NEXUS_VideoCodec_eH264;
			break;
		default:
			vidcodec = NEXUS_VideoCodec_eH264;
			break;
		}
	}
	if (audcodec == (unsigned)-1)
		audcodec = NEXUS_AudioCodec_eAc3;
	else {
		switch(audcodec)
		{
		case 2:
			audcodec = NEXUS_AudioCodec_eMpeg;
			break;
		case 3:
			audcodec = NEXUS_AudioCodec_eAc3;
			break;
		case 4:
			audcodec = NEXUS_AudioCodec_eAac;
			break;
		default:
			audcodec = NEXUS_AudioCodec_eAc3;
			break;
		}
	}

	BKNI_Memset(&xcodeContext, 0, sizeof(xcodeContext));

	pInputSettings->freq    = freq; /* MHz */
	pInputSettings->qamMode = qamMode;
	pInputSettings->eStreamType = NEXUS_TransportType_eTs;
	pInputSettings->eVideoCodec = vidcodec;
	pInputSettings->iVideoPid   = videopid;
	pInputSettings->iPcrPid     = videopid;

	pInputSettings->bAudioInput = true;
	if(pInputSettings->bAudioInput)
	{
		pInputSettings->iAudioPid   = audiopid;
		pInputSettings->eAudioCodec = NEXUS_AudioCodec_eAc3;
	}

	snprintf(pEncodeSettings->fname, sizeof(pEncodeSettings->fname), "videos/qam.ts");
	snprintf(pEncodeSettings->indexfname, sizeof(pEncodeSettings->indexfname), "videos/qam.nav");
	fprintf(stderr, "\t record transcoder output TS to videos/qam.ts...\n\n");
	pEncodeSettings->displayFormat = NEXUS_VideoFormat_eCustom2;
	pEncodeSettings->customFormatSettings.width  = 640;
	pEncodeSettings->customFormatSettings.height = 480;
	pEncodeSettings->customFormatSettings.interlaced  = false;
	pEncodeSettings->customFormatSettings.refreshRate = 30000; /* in 1/1000th Hz */
	pEncodeSettings->customFormatSettings.aspectRatio = NEXUS_DisplayAspectRatio_e4x3;

	pEncodeSettings->encoderFrameRate = NEXUS_VideoFrameRate_e30;
	pEncodeSettings->encoderBitrate   = 2000000;
	pEncodeSettings->encoderGopStructureFramesP = 29;/* 29 P frames per gop */
	pEncodeSettings->encoderGopStructureFramesB = 0; /* IP gop */
	pEncodeSettings->encoderVideoCodec = NEXUS_VideoCodec_eH264;
	pEncodeSettings->encoderProfile    = NEXUS_VideoCodecProfile_eBaseline;
	pEncodeSettings->encoderLevel      = NEXUS_VideoCodecLevel_e31;

	if(pInputSettings->bAudioInput)
	{
		pEncodeSettings->bAudioEncode = true;
		if(pEncodeSettings->bAudioEncode)
		{
			pEncodeSettings->encoderAudioCodec = NEXUS_AudioCodec_eAac;
		}
	}

	transcode_ts(&xcodeContext);

#endif
	return 0;
}

/* End of file */
