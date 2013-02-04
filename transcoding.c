/* Nexus example app: playback and decode */

#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_video_adj.h"
#include "nexus_spdif_output.h"
#include "nexus_component_output.h"
#if NEXUS_HAS_PLAYBACK && NEXUS_HAS_STREAM_MUX
#include "nexus_playback.h"
#include "nexus_record.h"
#include "nexus_file.h"
#include "nexus_video_encoder.h"
#include "nexus_audio_encoder.h"
#include "nexus_audio_mixer.h"
#include "nexus_stream_mux.h"
#include "nexus_recpump.h"
#include "nexus_record.h"
#include "nexus_packetsub.h"
#endif
#if NEXUS_DTV_PLATFORM
#include "nexus_platform_boardcfg.h"
#endif
#if NEXUS_HAS_SYNC_CHANNEL
#include "nexus_sync_channel.h"
#endif

#include <stdio.h>
#include <assert.h>
#include "bstd.h"
#include "bkni.h"
#include "bdbg.h"

#define SEGMENTED_RECORD 1
#if SEGMENTED_RECORD
#include "b_dvr_const.h"
#include "b_dvr_datatypes.h"
#endif

#define BTST_ENABLE_USER_DATA_ENCODE     0
#define BTST_HAS_VIDEO_ENCODE_TEST       1
#define NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST  1
#define BTST_ENABLE_AUDIO_ENCODE         1

#define BTST_PMT_PID 0x0055

BDBG_MODULE(transcode_playback);
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
static void 
transcoderFinishCallback(void *context, int param)
{
    BKNI_EventHandle finishEvent = (BKNI_EventHandle)context;

    BSTD_UNUSED(param);
    BDBG_WRN(("Finish callback invoked, now stop the stream mux."));
    BKNI_SetEvent(finishEvent);
}

#define TEST_ITERATIONS 3

struct cmdSettings {
    NEXUS_VideoFormat displayFormat;
    NEXUS_VideoFrameRate encoderFrameRate;
    unsigned encoderBitrate;
    unsigned encoderGopStructureFramesP;
    unsigned encoderGopStructureFramesB;
    NEXUS_VideoCodec encoderVideoCodec;
    NEXUS_VideoCodecProfile encoderProfile;
    NEXUS_VideoCodecLevel encoderLevel;
}cmdSettings[TEST_ITERATIONS] = {
    {NEXUS_VideoFormat_e720p, NEXUS_VideoFrameRate_e29_97, 3*1000*1000, 23, 0, NEXUS_VideoCodec_eH264, NEXUS_VideoCodecProfile_eBaseline, NEXUS_VideoCodecLevel_e30},
    //{NEXUS_VideoFormat_e720p, NEXUS_VideoFrameRate_e23_976, 6*1000*1000, 23, 0, NEXUS_VideoCodec_eH264, NEXUS_VideoCodecProfile_eBaseline, NEXUS_VideoCodecLevel_e31},
    {NEXUS_VideoFormat_e1080i, NEXUS_VideoFrameRate_e29_97, 10*1000*1000, 23, 0, NEXUS_VideoCodec_eMpeg2, NEXUS_VideoCodecProfile_eMain, NEXUS_VideoCodecLevel_eHigh},
    {NEXUS_VideoFormat_e480p, NEXUS_VideoFrameRate_e29_97, 2*1000*1000, 29, 0, NEXUS_VideoCodec_eH264, NEXUS_VideoCodecProfile_eBaseline, NEXUS_VideoCodecLevel_e31}
};

struct streamSettings {
    const char *fname;
    uint16_t   videoPid;
    NEXUS_VideoCodec videoCodec;
    uint16_t   audioPid;
    NEXUS_AudioCodec audioCodec;  
}streamSettings = {
    "/data/media0_0/8135531/8"/*  "videos/WildChina_Short.ts"*/,
    0x01e1,
    NEXUS_VideoCodec_eH264,
    0x01e2,
	NEXUS_AudioCodec_eMpeg,
};



int main(int argc, char* argv[])  
{
#if NEXUS_HAS_PLAYBACK && NEXUS_HAS_STREAM_MUX
    NEXUS_PlatformSettings platformSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_StcChannelHandle stcChannel;
    NEXUS_StcChannelSettings stcSettings;
    NEXUS_PidChannelHandle videoPidChannel;
    NEXUS_DisplayHandle display;
    NEXUS_VideoWindowHandle window;
    NEXUS_VideoWindowMadSettings windowMadSettings;
    NEXUS_VideoDecoderHandle videoDecoder;
    NEXUS_VideoDecoderStartSettings videoProgram;
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_AudioMixerSettings audioMixerSettings;
    NEXUS_AudioMixerHandle audioMixer;
    NEXUS_AudioDecoderHandle audioDecoder, audioPassthrough;
    NEXUS_AudioDecoderStartSettings audioProgram;
    NEXUS_PidChannelHandle audioPidChannel;
    NEXUS_AudioMuxOutputHandle audioMuxOutput;
    NEXUS_AudioMuxOutputDelayStatus audioDelayStatus;
	NEXUS_AudioMuxOutputStartSettings audioMuxStartSettings;
    NEXUS_PlaypumpHandle playpumpTranscodeAudio;
    NEXUS_PidChannelHandle pidChannelTranscodeAudio;
#if BTST_ENABLE_AUDIO_ENCODE 
    NEXUS_AudioEncoderSettings encoderSettings;
    NEXUS_AudioEncoderHandle audioEncoder;
#endif
    NEXUS_AudioCodec audioCodec;
#endif    
    NEXUS_FilePlayHandle file;
    NEXUS_PlaypumpHandle playpump;
    NEXUS_PlaybackHandle playback;
    NEXUS_PlaybackSettings playbackSettings;
    NEXUS_PlaybackPidChannelSettings playbackPidSettings;
    NEXUS_DisplayHandle displayTranscode;
    NEXUS_VideoWindowHandle windowTranscode;
#if BTST_HAS_VIDEO_ENCODE_TEST
    NEXUS_VideoEncoderHandle videoEncoder;
    NEXUS_VideoEncoderSettings videoEncoderConfig;
    NEXUS_VideoEncoderStartSettings videoEncoderStartConfig;
    NEXUS_VideoEncoderDelayRange videoDelay;
    NEXUS_PlaypumpHandle playpumpTranscodeVideo;
    NEXUS_PidChannelHandle pidChannelTranscodeVideo;
    NEXUS_RecordPidChannelSettings recordPidSettings;
#endif    
    NEXUS_StreamMuxHandle streamMux;
    NEXUS_StreamMuxCreateSettings muxCreateSettings;
    BKNI_EventHandle finishEvent;
    NEXUS_StreamMuxStartSettings muxConfig;
    NEXUS_StreamMuxOutput muxOutput;
    NEXUS_PlaypumpOpenSettings playpumpConfig;
    NEXUS_PlaypumpHandle playpumpTranscodePcr;
    NEXUS_FileRecordHandle fileTranscode;
    NEXUS_StcChannelHandle stcChannelTranscode;

#if NEXUS_HAS_SYNC_CHANNEL
    NEXUS_SyncChannelSettings syncChannelSettings;
    NEXUS_SyncChannelHandle syncChannel;
#endif
    NEXUS_RecpumpHandle recpump;
    NEXUS_RecordHandle record;
    NEXUS_RecordSettings recordSettings;
    NEXUS_PidChannelHandle pidChannelTranscodePcr;
    NEXUS_PidChannelHandle pidChannelTranscodePat;
    NEXUS_PidChannelHandle pidChannelTranscodePmt;
    void *pat;
    void *pmt;

    NEXUS_DisplaySettings displaySettings;
	int duration = 60 * 10;

    int i = 0;
    int iteration = 1;
    char key;
	int index_end = 0;
	int ts_end = 0;
	int audio_codec = 0;
	int video_codec = 0;

#if SEGMENTED_RECORD
	B_DVR_SegmentedPlayHandle segmentedPlay;
	B_DVR_SegmentedPlayHandle segmentedRecord;
	B_DVR_SegmentedPlaySettings settings;
	B_DVR_Mgr_Handle sDvrMgrHandle = NULL;
	B_DVR_Mgr_Settings dvrMgrSettings;

	B_Os_Init();
	BKNI_Memset(&dvrMgrSettings, 0, sizeof(dvrMgrSettings));
	dvrMgrSettings.bseg_rec = true;
	dvrMgrSettings.btimeshift = true;
	sDvrMgrHandle = B_DVR_Mgr_Open(&dvrMgrSettings);


#endif
	if (argc < 8) {
		printf("Usage : transcode filename duration videoPid audioPid index_size ts_size audioCodec videoCodec\n");
		printf("        audioCode = 0-mpeg, 1-ac3\n");
		printf("        audioCode = 0-Mpeg2, 1-H264\n");
	}
	duration = atoi(argv[2]);
	printf("duration:%d\n", duration);
	streamSettings.videoPid = atoi(argv[3]);
	streamSettings.audioPid = atoi(argv[4]);
	index_end = atoi(argv[5]);
	ts_end = atoi(argv[6]);
	audio_codec = atoi(argv[7]);
	if (audio_codec == 1) {
		streamSettings.audioCodec = NEXUS_AudioCodec_eAc3;
	} else if (audio_codec == 0) {
		streamSettings.audioCodec = NEXUS_AudioCodec_eMpeg;
	} else {
		streamSettings.audioCodec = audio_codec;
	}
	video_codec = atoi(argv[8]);
	if (video_codec == 1) {
		streamSettings.videoCodec = NEXUS_VideoCodec_eH264;
	} else if (video_codec == 0) {
		streamSettings.videoCodec = NEXUS_VideoCodec_eMpeg2;
	} else {
		streamSettings.videoCodec = video_codec;
	}

	printf("videoPid:%d, audioPid:%d\n", streamSettings.videoPid, streamSettings.audioPid);

    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    platformSettings.openFrontend = false;
    NEXUS_Platform_Init(&platformSettings);
    NEXUS_Platform_GetConfiguration(&platformConfig);

again:
    BDBG_WRN(("Setting up transcode pipeline: format %d, fr %d, bitrate %u, gopP %d, codec %d",
                cmdSettings[i].displayFormat,
                cmdSettings[i].encoderFrameRate,
                cmdSettings[i].encoderBitrate,
                cmdSettings[i].encoderGopStructureFramesP,
                cmdSettings[i].encoderVideoCodec));
    playpump = NEXUS_Playpump_Open(0, NULL);
    assert(playpump);
    playback = NEXUS_Playback_Create();
    assert(playback);
	if (argc > 1) {
		streamSettings.fname = argv[1];
	}
#if SEGMENTED_RECORD
	printf("fname:%s\n", streamSettings.fname);
	{
		char ts_name[255];
		char index_name[255];
		sprintf(ts_name, "/data/media0_0/%s/%s.ts", streamSettings.fname, streamSettings.fname);
		sprintf(index_name, "/data/media0_0/%s/%s.nav", streamSettings.fname, streamSettings.fname);
		segmentedPlay = B_DVR_SegmentedPlay_Open(ts_name, index_name, NULL);
		assert(segmentedPlay);
		B_DVR_SegmentedPlay_GetSettings(segmentedPlay, &settings);
		settings.index_start = 0;
		//settings.index_end = 2227904;
		settings.index_end = index_end;
		settings.media_start = 0;
		settings.media_end = ts_end;
		//settings.media_end = 1508167680;
		B_DVR_SegmentedPlay_SetSettings(segmentedPlay, &settings);
		file = B_DVR_SegmentedPlay_Get_NexusPlayFile(segmentedPlay);
		assert(file);
	}
#else
    file = NEXUS_FilePlay_OpenPosix(streamSettings.fname, NULL);
    if (!file) {
        fprintf(stderr, "can't open file:%s\n", streamSettings.fname);
        return -1;
    }
#endif

#if NEXUS_HAS_SYNC_CHANNEL
    /* create a sync channel */
    NEXUS_SyncChannel_GetDefaultSettings(&syncChannelSettings);
    syncChannel = NEXUS_SyncChannel_Create(&syncChannelSettings);
#endif


    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    stcChannel = NEXUS_StcChannel_Open(0, &stcSettings);

    /* encoders/mux require different STC broadcast mode from decoder */
    NEXUS_StcChannel_GetDefaultSettings(1, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;/* should be the same timebase for end-to-end locking */
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    stcSettings.pcrBits = NEXUS_StcChannel_PcrBits_eFull42;/* ViCE2 requires 42-bit STC broadcast */
    stcChannelTranscode = NEXUS_StcChannel_Open(1, &stcSettings);


    NEXUS_Playback_GetSettings(playback, &playbackSettings);
    playbackSettings.playpump = playpump;
    /* set a stream format, it could be any audio video transport type or file format, i.e NEXUS_TransportType_eMp4, NEXUS_TransportType_eAvi ... */
    playbackSettings.playpumpSettings.transportType = NEXUS_TransportType_eTs;
    playbackSettings.stcChannel = stcChannel;
    NEXUS_Playback_SetSettings(playback, &playbackSettings);


    /* Bring up video display and outputs */
    NEXUS_Display_GetDefaultSettings(&displaySettings);
    displaySettings.format = NEXUS_VideoFormat_e480p;
    display = NEXUS_Display_Open(0, &displaySettings);
#if NEXUS_DTV_PLATFORM
    NEXUS_Display_AddOutput(display, NEXUS_PanelOutput_GetConnector(platformConfig.outputs.panel[0]));
    NEXUS_BoardCfg_ConfigurePanel(true, true, true);
#else
#if NEXUS_NUM_COMPONENT_OUTPUTS
   if(platformConfig.outputs.component[0]){
    	NEXUS_Display_AddOutput(display, NEXUS_ComponentOutput_GetConnector(platformConfig.outputs.component[0]));
   }
#endif 
#if NEXUS_NUM_HDMI_OUTPUTS
    NEXUS_Display_AddOutput(display, NEXUS_HdmiOutput_GetVideoConnector(platformConfig.outputs.hdmi[0]));
#endif
#endif
    window = NEXUS_VideoWindow_Open(display, 0);
    /* enable deinterlacer to improve quality */
    NEXUS_VideoWindow_GetMadSettings(window, &windowMadSettings);
    windowMadSettings.deinterlace = true;
    windowMadSettings.enable22Pulldown = true;
    windowMadSettings.enable32Pulldown = true;
    NEXUS_VideoWindow_SetMadSettings(window, &windowMadSettings);

#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    /* Open the audio decoder */
    audioDecoder = NEXUS_AudioDecoder_Open(0, NULL);
    audioPassthrough = NEXUS_AudioDecoder_Open(1, NULL);

    /* Open the audio and pcr pid channel */
    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eAudio;
    playbackPidSettings.pidTypeSettings.audio.primary = audioDecoder; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.audio.secondary = audioPassthrough;
    audioPidChannel = NEXUS_Playback_OpenPidChannel(playback, streamSettings.audioPid, &playbackPidSettings);
    
    /* Set up decoder Start structures now. We need to know the audio codec to properly set up
    the audio outputs. */
    NEXUS_AudioDecoder_GetDefaultStartSettings(&audioProgram);
    audioProgram.codec = streamSettings.audioCodec;
    audioProgram.pidChannel = audioPidChannel;
    audioProgram.stcChannel = stcChannel;

    /* Connect audio decoders to outputs */
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]),
        NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    NEXUS_AudioOutput_AddInput(
        NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]),
        NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));

	/* Open audio mixer.  The mixer can be left running at all times to provide continuous audio output despite input discontinuities.  */
	NEXUS_AudioMixer_GetDefaultSettings(&audioMixerSettings);
	audioMixerSettings.mixUsingDsp = true;
	audioMixer = NEXUS_AudioMixer_Open(&audioMixerSettings);
	assert(audioMixer);

    /* Open audio mux output */
    audioMuxOutput = NEXUS_AudioMuxOutput_Create(NULL);
    assert(audioMuxOutput);
#if BTST_ENABLE_AUDIO_ENCODE 
    /* Open audio encoder */
    NEXUS_AudioEncoder_GetDefaultSettings(&encoderSettings);
    encoderSettings.codec = NEXUS_AudioCodec_eAac;
    audioCodec = encoderSettings.codec;
    audioEncoder = NEXUS_AudioEncoder_Open(&encoderSettings);
    assert(audioEncoder);

	/* Connect decoder to mixer and set as master */
	NEXUS_AudioMixer_AddInput(audioMixer,
		NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
	audioMixerSettings.master = NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo);
	NEXUS_AudioMixer_SetSettings(audioMixer, &audioMixerSettings);
	/* Connect mixer to encoder */
	NEXUS_AudioEncoder_AddInput(audioEncoder, NEXUS_AudioMixer_GetConnector(audioMixer));
    /* Connect mux to encoder */
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput), NEXUS_AudioEncoder_GetConnector(audioEncoder));
#else    
	NEXUS_AudioMixer_AddInput(audioMixer,
		NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
	audioMixerSettings.master = NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed);
	NEXUS_AudioMixer_SetSettings(audioMixer, &audioMixerSettings);
	NEXUS_AudioOutput_AddInput(
		NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput),
		NEXUS_AudioMixer_GetConnector(audioMixer));

    audioCodec = audioProgram.codec;
#endif
	NEXUS_AudioOutput_AddInput(
		NEXUS_AudioDummyOutput_GetConnector(platformConfig.outputs.audioDummy[0]),
		NEXUS_AudioMixer_GetConnector(audioMixer));
#endif

    /* bring up decoder and connect to local display */
    videoDecoder = NEXUS_VideoDecoder_Open(0, NULL); /* take default capabilities */

    /* NOTE: must open video encoder before display; otherwise open will init ViCE2 core
     * which might cause encoder display GISB error since encoder display would
     * trigger RDC to program mailbox registers in ViCE2;
     */
#if BTST_HAS_VIDEO_ENCODE_TEST
    videoEncoder = NEXUS_VideoEncoder_Open(0, NULL);
    assert(videoEncoder);
#endif

    /* Bring up video encoder display */
    NEXUS_Display_GetDefaultSettings(&displaySettings);
    displaySettings.displayType = NEXUS_DisplayType_eAuto;
    displaySettings.timingGenerator = NEXUS_DisplayTimingGenerator_eEncoder;
    displaySettings.format = cmdSettings[i].displayFormat;/* source is 60hz */
    displaySettings.frameRateMaster = NULL;/* disable frame rate tracking for now */

    displaySettings.format = cmdSettings[i].displayFormat;
    displayTranscode = NEXUS_Display_Open(NEXUS_ENCODER_DISPLAY_IDX, &displaySettings);/* cmp3 for transcoder */
    assert(displayTranscode);

    windowTranscode = NEXUS_VideoWindow_Open(displayTranscode, 0);
    assert(windowTranscode);
    /* enable deinterlacer to improve quality */
    NEXUS_VideoWindow_GetMadSettings(windowTranscode, &windowMadSettings);
    windowMadSettings.deinterlace = true;
    windowMadSettings.enable22Pulldown = true;
    windowMadSettings.enable32Pulldown = true;
    NEXUS_VideoWindow_SetMadSettings(windowTranscode, &windowMadSettings);

    /* connect same decoder to the encoder display;
     * NOTE: simul display + transcode mode might have limitation in audio pathre;
     * here is for video transcode bringup purpose;
     */
    NEXUS_VideoWindow_AddInput(windowTranscode, NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoWindow_AddInput(window, NEXUS_VideoDecoder_GetConnector(videoDecoder));

    /* Open the video pid channel */
    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
    playbackPidSettings.pidTypeSettings.video.codec = streamSettings.videoCodec; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.video.index = true;
    playbackPidSettings.pidTypeSettings.video.decoder = videoDecoder;
    videoPidChannel = NEXUS_Playback_OpenPidChannel(playback, streamSettings.videoPid, &playbackPidSettings);

    /* Set up decoder Start structures now. We need to know the audio codec to properly set up
    the audio outputs. */
    NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
    videoProgram.codec = streamSettings.videoCodec;
    videoProgram.pidChannel = videoPidChannel;
    videoProgram.stcChannel = stcChannel;

#if BTST_HAS_VIDEO_ENCODE_TEST
    NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
    videoEncoderConfig.variableFrameRate = true; /* encoder can detect film content and follow CET */
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e30;
    videoEncoderConfig.frameRate = cmdSettings[i].encoderFrameRate;
    videoEncoderConfig.bitrateMax = cmdSettings[i].encoderBitrate;
    videoEncoderConfig.streamStructure.trackInput = false;
    videoEncoderConfig.streamStructure.framesP = cmdSettings[i].encoderGopStructureFramesP;
    videoEncoderConfig.streamStructure.framesB = cmdSettings[i].encoderGopStructureFramesB;


    NEXUS_VideoEncoder_GetDefaultStartSettings(&videoEncoderStartConfig);
    videoEncoderStartConfig.codec = cmdSettings[i].encoderVideoCodec;
    videoEncoderStartConfig.profile = cmdSettings[i].encoderProfile;
    videoEncoderStartConfig.level = cmdSettings[i].encoderLevel;
    videoEncoderStartConfig.input = displayTranscode;
    videoEncoderStartConfig.stcChannel = stcChannelTranscode;
    videoEncoderStartConfig.encodeUserData = true;

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

    /* encode setting and startSetting to be set after end-to-end delay is determined */
    
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
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
        unsigned Dee;
#endif
        /* NOTE: video encoder delay is in 27MHz ticks */
        NEXUS_VideoEncoder_GetDelayRange(videoEncoder, &videoEncoderConfig, &videoEncoderStartConfig, &videoDelay);
        printf("\n\tVideo encoder end-to-end delay = %u ms; maximum allowed: %u ms\n", videoDelay.min/27000, videoDelay.max/27000);
    
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
        NEXUS_AudioMuxOutput_GetDelayStatus(audioMuxOutput, audioCodec, &audioDelayStatus);
        printf("\tAudio codec %d end-to-end delay = %u ms\n", audioCodec, audioDelayStatus.endToEndDelay);

        Dee = audioDelayStatus.endToEndDelay * 27000; /* in 27MHz ticks */
        if(Dee > videoDelay.min)
        {
            if(Dee > videoDelay.max)
            {
                BDBG_ERR(("\tAudio Dee is way too big! Use video Dee max!"));
                Dee = videoDelay.max;
            }
            else
            {
                printf("\tUse audio Dee %u ms %u ticks@27Mhz!\n", Dee/27000, Dee);
            }
        }
        else
        {
            Dee = videoDelay.min;
            printf("\tUse video Dee %u ms or %u ticks@27Mhz!\n\n", Dee/27000, Dee);
        }
        videoEncoderConfig.encoderDelay = Dee;

        /* Start audio mux output */
        NEXUS_AudioMuxOutput_GetDefaultStartSettings(&audioMuxStartSettings);
        audioMuxStartSettings.stcChannel = stcChannelTranscode;
        audioMuxStartSettings.presentationDelay = Dee/27000;/* in ms */
        NEXUS_AudioMuxOutput_Start(audioMuxOutput, &audioMuxStartSettings);
#else
        videoEncoderConfig.encoderDelay = videoDelay.min;
#endif
    }
	/* Note: video encoder SetSettings needs to be called after the encoder delay is determined; */
    NEXUS_VideoEncoder_SetSettings(videoEncoder, &videoEncoderConfig);

    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpConfig);
    playpumpConfig.fifoSize = 16384; /* reduce FIFO size allocated for playpump */
    playpumpConfig.numDescriptors = 64; /* set number of descriptors */
    playpumpConfig.streamMuxCompatible = true;
    playpumpTranscodeVideo = NEXUS_Playpump_Open(1, &playpumpConfig);
    assert(playpumpTranscodeVideo);
#endif

#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_Playpump_GetDefaultOpenSettings(&playpumpConfig);
    playpumpConfig.fifoSize = 16384; /* reduce FIFO size allocated for playpump */
    playpumpConfig.numDescriptors = 64; /* set number of descriptors */
    playpumpConfig.streamMuxCompatible = true;
    playpumpTranscodeAudio = NEXUS_Playpump_Open(2, &playpumpConfig);
    assert(playpumpTranscodeAudio);
#endif    
    playpumpTranscodePcr = NEXUS_Playpump_Open(3, &playpumpConfig);
    assert(playpumpTranscodePcr);
    
    BKNI_CreateEvent(&finishEvent);
    NEXUS_StreamMux_GetDefaultCreateSettings(&muxCreateSettings);
    muxCreateSettings.finished.callback = transcoderFinishCallback;
    muxCreateSettings.finished.context = finishEvent;
    streamMux = NEXUS_StreamMux_Create(&muxCreateSettings);
    NEXUS_StreamMux_GetDefaultStartSettings(&muxConfig);
    muxConfig.transportType = NEXUS_TransportType_eTs;
    muxConfig.stcChannel = stcChannelTranscode;

#if BTST_HAS_VIDEO_ENCODE_TEST
    muxConfig.video[0].pid = 0x11;
    muxConfig.video[0].encoder = videoEncoder;
    muxConfig.video[0].playpump = playpumpTranscodeVideo;
#endif    
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    muxConfig.audio[0].pid = 0x12;
    muxConfig.audio[0].muxOutput = audioMuxOutput;
    muxConfig.audio[0].playpump = playpumpTranscodeAudio;
#endif
    muxConfig.pcr.pid = 0x13;
    muxConfig.pcr.playpump = playpumpTranscodePcr;
    muxConfig.pcr.interval = 50;

    /* open PidChannels */
    pidChannelTranscodePcr = NEXUS_Playpump_OpenPidChannel(playpumpTranscodePcr, muxConfig.pcr.pid, NULL);
    assert(pidChannelTranscodePcr);
    pidChannelTranscodePmt = NEXUS_Playpump_OpenPidChannel(playpumpTranscodePcr, BTST_PMT_PID, NULL);
    assert(pidChannelTranscodePmt);
    pidChannelTranscodePat = NEXUS_Playpump_OpenPidChannel(playpumpTranscodePcr, 0, NULL);
    assert(pidChannelTranscodePat);

    /* start mux */
    NEXUS_StreamMux_Start(streamMux,&muxConfig, &muxOutput);
#if BTST_HAS_VIDEO_ENCODE_TEST
    pidChannelTranscodeVideo = muxOutput.video[0];
#endif
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    pidChannelTranscodeAudio = muxOutput.audio[0];
#endif

    recpump = NEXUS_Recpump_Open(0, NULL);
    assert(recpump);

    record = NEXUS_Record_Create();
    assert(record);

    NEXUS_Record_GetSettings(record, &recordSettings);
    recordSettings.recpump = recpump;
    NEXUS_Record_SetSettings(record, &recordSettings);

#if BTST_HAS_VIDEO_ENCODE_TEST
    /* configure the video pid for indexing */
    NEXUS_Record_GetDefaultPidChannelSettings(&recordPidSettings);
    recordPidSettings.recpumpSettings.pidType = NEXUS_PidType_eVideo;
    recordPidSettings.recpumpSettings.pidTypeSettings.video.index = true;
    recordPidSettings.recpumpSettings.pidTypeSettings.video.codec = cmdSettings[i].encoderVideoCodec;

    /* add multiplex data to the same record */
    NEXUS_Record_AddPidChannel(record, pidChannelTranscodeVideo, &recordPidSettings);
#endif
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_Record_AddPidChannel(record, pidChannelTranscodeAudio, NULL);
#endif
    NEXUS_Record_AddPidChannel(record, pidChannelTranscodePcr, NULL);
    NEXUS_Record_AddPidChannel(record, pidChannelTranscodePat, NULL);
    NEXUS_Record_AddPidChannel(record, pidChannelTranscodePmt, NULL);
#if SEGMENTED_RECORD
	{
		char ts_name[255];
		char index_name[255];
		sprintf(ts_name, "/data/hls/%s/%s.ts", streamSettings.fname, streamSettings.fname);
		sprintf(index_name, "/data/hls/%s/%s.nav", streamSettings.fname, streamSettings.fname);
		segmentedRecord = B_DVR_SegmentedRecord_Open(ts_name, index_name,
				NULL, false);
		assert(segmentedRecord);
	}
	fileTranscode = B_DVR_SegmentedRecord_Get_NexusRecordFile(segmentedRecord);
#else
    fileTranscode = NEXUS_FileRecord_OpenPosix("videos/Avatar.ts", "videos/stream.nav");
#endif
    assert(fileTranscode);

    /* Start record of stream mux output */
    NEXUS_Record_Start(record, fileTranscode);

#if SEGMENTED_RECORD
	B_DVR_SegmentedRecord_Trim_Start(segmentedRecord);
#endif

#if NEXUS_HAS_SYNC_CHANNEL
    /* connect sync channel */
    NEXUS_SyncChannel_GetSettings(syncChannel, &syncChannelSettings);
    syncChannelSettings.videoInput = NEXUS_VideoDecoder_GetConnector(videoDecoder);
    syncChannelSettings.audioInput[0] = NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo);
    syncChannelSettings.audioInput[1] = NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed);
    NEXUS_SyncChannel_SetSettings(syncChannel, &syncChannelSettings);
#endif

    /* Start decoder */
    NEXUS_VideoDecoder_Start(videoDecoder, &videoProgram);
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_AudioDecoder_Start(audioDecoder, &audioProgram);
    NEXUS_AudioDecoder_Start(audioPassthrough, &audioProgram);
#endif
    /* Start playback */
    NEXUS_Playback_Start(playback, file, NULL);
	{
		int started;
		NEXUS_PacketSubHandle packetSub, packetSub2;
		NEXUS_PacketSubOpenSettings packetSubOpenSettings;
		NEXUS_PacketSubSettings packetSubSettings;
		NEXUS_PidChannelHandle patPidChannel, pmtPidChannel;
		NEXUS_Error rc;
		int sectionNum = 16;
		int i = 0;
		uint8_t *pkt, *pkt2;
		unsigned size;
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
			0xff,0xff,0xff,0xff,0xff,'P', 'A', 'T',
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
		static const uint8_t s_auiTSPacket_PMT[188] =
		{
			0x47,0x40,BTST_PMT_PID,0x30,0x9c,0x40,0xff,0xff,
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
			0xff,0xff,0xff,0xff,0xff,'P', 'M', 'T',
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
        NEXUS_StreamMuxSystemData psi[2];
		int count;
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

#if BTST_HAS_VIDEO_ENCODE_TEST
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
#endif
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
		NEXUS_StreamMux_AddSystemDataBuffer(streamMux, &psi[0]);
		NEXUS_StreamMux_AddSystemDataBuffer(streamMux, &psi[1]);
		NEXUS_VideoEncoder_Start(videoEncoder, &videoEncoderStartConfig);
	}
#if 1
    /* Playback state machine is driven from inside Nexus. */
    //printf("Press ENTER to continue; type 'q' to quit\n");
    //key = getchar();
    //printf("sleep:%d\n", duration);
	sleep(duration);
#endif


    /* Bring down system */
    NEXUS_Playback_Stop(playback);
    NEXUS_VideoDecoder_Stop(videoDecoder);
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_AudioDecoder_Stop(audioPassthrough);

    NEXUS_AudioDecoder_Stop(audioDecoder);
    NEXUS_AudioMixer_Stop(audioMixer);
    NEXUS_AudioMuxOutput_Stop(audioMuxOutput);
#endif
#if NEXUS_HAS_SYNC_CHANNEL
    /* disconnect sync channel */
    NEXUS_SyncChannel_GetSettings(syncChannel, &syncChannelSettings);
    syncChannelSettings.videoInput = NULL;
    syncChannelSettings.audioInput[0] = NULL;
    syncChannelSettings.audioInput[1] = NULL;
    NEXUS_SyncChannel_SetSettings(syncChannel, &syncChannelSettings);
#endif

    
#if BTST_HAS_VIDEO_ENCODE_TEST
    NEXUS_VideoEncoder_Stop(videoEncoder);
#endif
    NEXUS_StreamMux_Finish(streamMux);
    /* wait for the encoder buffer model's data to be drained */
    if(BKNI_WaitForEvent(finishEvent, (videoEncoderConfig.encoderDelay/27000)*2)!=BERR_SUCCESS) {
        fprintf(stderr, "TIMEOUT\n");
    }
    BKNI_DestroyEvent(finishEvent);
    NEXUS_Record_Stop(record);
	/* Note: remove all record PID channels before stream mux stop since streammux would close the A/V PID channels */
    NEXUS_Record_RemoveAllPidChannels(record);
    NEXUS_StreamMux_Stop(streamMux);

	
    NEXUS_Record_Destroy(record);
    NEXUS_Recpump_Close(recpump);
    NEXUS_FileRecord_Close(fileTranscode);

    NEXUS_Playback_ClosePidChannel(playback, audioPidChannel);
    NEXUS_Playback_ClosePidChannel(playback, videoPidChannel);
    NEXUS_Playpump_ClosePidChannel(playpumpTranscodePcr, pidChannelTranscodePcr);
    NEXUS_Playpump_ClosePidChannel(playpumpTranscodePcr, pidChannelTranscodePat);
    NEXUS_Playpump_ClosePidChannel(playpumpTranscodePcr, pidChannelTranscodePmt);
    NEXUS_FilePlay_Close(file);
    NEXUS_Playback_Destroy(playback);
    NEXUS_Playpump_Close(playpump);

    NEXUS_VideoWindow_RemoveInput(window, NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoWindow_RemoveInput(windowTranscode, NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoInput_Shutdown(NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoDecoder_Close(videoDecoder);
    NEXUS_VideoWindow_Close(window);
    NEXUS_VideoWindow_Close(windowTranscode);
    NEXUS_Display_Close(display);
    NEXUS_Display_Close(displayTranscode);

    NEXUS_StreamMux_Destroy(streamMux);

    NEXUS_Playpump_Close(playpumpTranscodePcr);
	
#if BTST_HAS_VIDEO_ENCODE_TEST
	NEXUS_Playpump_Close(playpumpTranscodeVideo);
	NEXUS_VideoEncoder_Close(videoEncoder);
#endif    

#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
	NEXUS_AudioOutput_RemoveAllInputs(NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput));
#if BTST_ENABLE_AUDIO_ENCODE 
	NEXUS_AudioEncoder_RemoveAllInputs(audioEncoder);
    NEXUS_AudioInput_Shutdown(NEXUS_AudioEncoder_GetConnector(audioEncoder));
    NEXUS_AudioEncoder_Close(audioEncoder);
#endif
	NEXUS_AudioMixer_RemoveAllInputs(audioMixer);
	NEXUS_AudioOutput_RemoveAllInputs(NEXUS_AudioDummyOutput_GetConnector(platformConfig.outputs.audioDummy[0]));
	NEXUS_AudioOutput_Shutdown(NEXUS_AudioDummyOutput_GetConnector(platformConfig.outputs.audioDummy[0]));
	NEXUS_AudioInput_Shutdown(NEXUS_AudioMixer_GetConnector(audioMixer));
	NEXUS_AudioMixer_Close(audioMixer);
    NEXUS_AudioOutput_RemoveAllInputs(NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]));
    NEXUS_AudioOutput_RemoveAllInputs(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]));

    NEXUS_Playpump_Close(playpumpTranscodeAudio);
    NEXUS_AudioOutput_Shutdown(NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput));
    NEXUS_AudioMuxOutput_Destroy(audioMuxOutput);

    NEXUS_AudioInput_Shutdown(NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    NEXUS_AudioInput_Shutdown(NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
    NEXUS_AudioOutput_Shutdown(NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]));
    NEXUS_AudioOutput_Shutdown(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]));

    NEXUS_AudioDecoder_Close(audioDecoder);
    NEXUS_AudioDecoder_Close(audioPassthrough);
#endif

#if NEXUS_HAS_SYNC_CHANNEL
	NEXUS_SyncChannel_Destroy(syncChannel);
#endif

	NEXUS_StcChannel_Close(stcChannel);
	NEXUS_StcChannel_Close(stcChannelTranscode);
	NEXUS_Memory_Free(pat);
	NEXUS_Memory_Free(pmt);
#if 0
	if(key != 'q')
{
	i = iteration++%TEST_ITERATIONS;
	BDBG_WRN(("Start %d iteration.....", iteration));
	goto again;
}
#endif

NEXUS_Platform_Uninit();

#endif
return 0;
}


