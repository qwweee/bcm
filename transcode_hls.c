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
#include "nexus_file_mux.h"
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

#ifndef NEXUS_ENCODER_DISPLAY_IDX
#define NEXUS_ENCODER_DISPLAY_IDX   3
#endif

#define BTST_HAS_VIDEO_ENCODE_TEST       1
#define NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST 1
#define BTST_ENABLE_AUDIO_ENCODE        1

#define BTST_AC3_TRANSCODER_PASSTHRU    1
#define BTST_MPG_TRANSCODER_PASSTHRU    0
#define BTST_AAC_TRANSCODER_PASSTHRU    0

BDBG_MODULE(transcode_playback);
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
    {NEXUS_VideoFormat_e720p24hz, NEXUS_VideoFrameRate_e23_976, 6*1000*1000, 23, 0, NEXUS_VideoCodec_eH264, NEXUS_VideoCodecProfile_eBaseline, NEXUS_VideoCodecLevel_e31},
    {NEXUS_VideoFormat_e1080p24hz, NEXUS_VideoFrameRate_e23_976, 10*1000*1000, 23, 0, NEXUS_VideoCodec_eH264, NEXUS_VideoCodecProfile_eMain, NEXUS_VideoCodecLevel_e40},
    {NEXUS_VideoFormat_e480p, NEXUS_VideoFrameRate_e29_97, 2*1000*1000, 29, 0, NEXUS_VideoCodec_eMpeg4Part2, NEXUS_VideoCodecProfile_eAdvancedSimple, NEXUS_VideoCodecLevel_e30}
};

int main(void)  
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
    NEXUS_VideoWindowSettings windowSettings;

    NEXUS_VideoDecoderHandle videoDecoder;
    NEXUS_VideoDecoderStartSettings videoProgram;
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_AudioDecoderHandle audioDecoder, audioPassthrough;
    NEXUS_AudioDecoderStartSettings audioProgram;
    NEXUS_PidChannelHandle audioPidChannel;
    NEXUS_AudioMuxOutputHandle audioMuxOutput;
    NEXUS_AudioMuxOutputDelayStatus audioDelayStatus;
	NEXUS_AudioMuxOutputStartSettings audioMuxStartSettings;
#if BTST_ENABLE_AUDIO_ENCODE 
    NEXUS_AudioEncoderSettings audioEncoderSettings;
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
#endif    
    NEXUS_FileMuxHandle fileMux;
    NEXUS_MuxFileHandle muxOutput;
    NEXUS_FileMuxCreateSettings muxCreateSettings;
    BKNI_EventHandle finishEvent;
    NEXUS_FileMuxStartSettings muxConfig;
    NEXUS_StcChannelHandle stcChannelTranscode;

#if NEXUS_HAS_SYNC_CHANNEL
    NEXUS_SyncChannelSettings syncChannelSettings;
    NEXUS_SyncChannelHandle syncChannel;
#endif

    NEXUS_DisplaySettings displaySettings;
#if BTST_AC3_TRANSCODER_PASSTHRU /* AC3 */   
    const char *fname = "/data/record.ts"/*  "videos/WildChina_Short.ts"*/;
#elif BTST_MPG_TRANSCODER_PASSTHRU /* MPEG */   
    const char *fname = "videos/cnnticker.mpg";
#else /* AAC */
    const char *fname = "videos/testjun26_188.mpg";
#endif
    const char *mp4File = "/data/stream.ts";

    int i = 0;
    int iteration = 1;
    char key;

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

    file = NEXUS_FilePlay_OpenPosix(fname, NULL);
    if (!file) {
        fprintf(stderr, "can't open file:%s\n", fname);
        return -1;
    }

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
#if BTST_AC3_TRANSCODER_PASSTHRU /* AC3 */   
    audioPidChannel = NEXUS_Playback_OpenPidChannel(playback, 290, &playbackPidSettings);
#elif BTST_MPG_TRANSCODER_PASSTHRU /* MPG */   
    audioPidChannel = NEXUS_Playback_OpenPidChannel(playback, 0x22, &playbackPidSettings);
#else /* AAC */
    audioPidChannel = NEXUS_Playback_OpenPidChannel(playback, 0x110, &playbackPidSettings);
#endif
    
    /* Set up decoder Start structures now. We need to know the audio codec to properly set up
    the audio outputs. */
    NEXUS_AudioDecoder_GetDefaultStartSettings(&audioProgram);
#if BTST_AC3_TRANSCODER_PASSTHRU /* AC3 */   
    audioProgram.codec = NEXUS_AudioCodec_eAc3;
#elif BTST_MPG_TRANSCODER_PASSTHRU /* MPG */   
    audioProgram.codec = NEXUS_AudioCodec_eMpeg;
#else /* AAC */
    audioProgram.codec = NEXUS_AudioCodec_eAac;
#endif
    audioProgram.pidChannel = audioPidChannel;
    audioProgram.stcChannel = stcChannel;

    /* Connect audio decoders to outputs */
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]),
        NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    NEXUS_AudioOutput_AddInput(
        NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]),
        NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));

    /* Open audio mux output */
    audioMuxOutput = NEXUS_AudioMuxOutput_Create(NULL);
    assert(audioMuxOutput);
#if BTST_ENABLE_AUDIO_ENCODE 
    /* Open audio encoder */
    NEXUS_AudioEncoder_GetDefaultSettings(&audioEncoderSettings);
    audioEncoderSettings.codec = NEXUS_AudioCodec_eAac;
    audioCodec = audioEncoderSettings.codec;
    audioEncoder = NEXUS_AudioEncoder_Open(&audioEncoderSettings);
    assert(audioEncoder);
    /* Connect encoder to decoder */
    NEXUS_AudioEncoder_AddInput(audioEncoder,
                                NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    /* Connect mux to encoder */
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput), NEXUS_AudioEncoder_GetConnector(audioEncoder));
#else    
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput),
        NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));

    audioCodec = audioProgram.codec;
#endif
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
    displaySettings.format = NEXUS_VideoFormat_e720p;/* source is 60hz */
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

    /* enable automatic aspect ratio correction mode to convert dynamic source aspect ratio to fixed display aspect ratio since it seems
       MP4 container doesn't tolerate dynamic aspect ratio.*/
    NEXUS_VideoWindow_GetSettings(windowTranscode, &windowSettings);
    windowSettings.contentMode = NEXUS_VideoWindowContentMode_eBox;
    NEXUS_VideoWindow_SetSettings(windowTranscode, &windowSettings);

    /* connect same decoder to the encoder display;
     * NOTE: simul display + transcode mode might have limitation in audio pathre;
     * here is for video transcode bringup purpose;
     */
    NEXUS_VideoWindow_AddInput(windowTranscode, NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoWindow_AddInput(window, NEXUS_VideoDecoder_GetConnector(videoDecoder));

    /* Open the video pid channel */
    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
#if BTST_AC3_TRANSCODER_PASSTHRU /* avatar */  
    playbackPidSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eH264; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.video.index = true;
    playbackPidSettings.pidTypeSettings.video.decoder = videoDecoder;
    videoPidChannel = NEXUS_Playback_OpenPidChannel(playback, /*0x201*/289, &playbackPidSettings);
#elif BTST_MPG_TRANSCODER_PASSTHRU /* cnnticker */   
    playbackPidSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eMpeg2; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.video.index = true;
    playbackPidSettings.pidTypeSettings.video.decoder = videoDecoder;
    videoPidChannel = NEXUS_Playback_OpenPidChannel(playback, 0x21, &playbackPidSettings);
#else /* testjun26_188 */
    playbackPidSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eMpeg2; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.video.index = true;
    playbackPidSettings.pidTypeSettings.video.decoder = videoDecoder;
    videoPidChannel = NEXUS_Playback_OpenPidChannel(playback, 0x100, &playbackPidSettings);
#endif

    /* Set up decoder Start structures now. We need to know the audio codec to properly set up
    the audio outputs. */
    NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
#if BTST_AC3_TRANSCODER_PASSTHRU /* avatar */  
    videoProgram.codec = NEXUS_VideoCodec_eH264;
#elif BTST_MPG_TRANSCODER_PASSTHRU /* cnnticker */   
    videoProgram.codec = NEXUS_VideoCodec_eMpeg2;
#else /* testjun26_188 */
    videoProgram.codec = NEXUS_VideoCodec_eMpeg2;
#endif
    videoProgram.pidChannel = videoPidChannel;
    videoProgram.stcChannel = stcChannel;

#if BTST_HAS_VIDEO_ENCODE_TEST
    NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
    videoEncoderConfig.variableFrameRate = true;
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
#endif

    
    BKNI_CreateEvent(&finishEvent);
    NEXUS_FileMux_GetDefaultCreateSettings(&muxCreateSettings);
    muxCreateSettings.finished.callback = transcoderFinishCallback;
    muxCreateSettings.finished.context = finishEvent;
    fileMux = NEXUS_FileMux_Create(&muxCreateSettings);
    NEXUS_FileMux_GetDefaultStartSettings(&muxConfig, NEXUS_TransportType_eTs);

#if BTST_HAS_VIDEO_ENCODE_TEST
    muxConfig.video[0].track = 1;
    muxConfig.video[0].codec = videoEncoderStartConfig.codec;
    muxConfig.video[0].encoder = videoEncoder;
#endif    
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    muxConfig.audio[0].track = 2;
    muxConfig.audio[0].codec = audioProgram.codec;
#if BTST_ENABLE_AUDIO_ENCODE 
    muxConfig.audio[0].codec = audioEncoderSettings.codec;
#endif
    muxConfig.audio[0].muxOutput = audioMuxOutput;
#endif
    snprintf(muxConfig.tempDir, sizeof(muxConfig.tempDir), "/data/");
    muxOutput = NEXUS_MuxFile_OpenPosix(mp4File);
    if (!muxOutput) {
        fprintf(stderr, "can't open file:%s\n", mp4File);
        return -1;
    }

    /* start mux */
    NEXUS_FileMux_Start(fileMux,&muxConfig, muxOutput);

#if NEXUS_HAS_SYNC_CHANNEL
    /* connect sync channel */
    NEXUS_SyncChannel_GetSettings(syncChannel, &syncChannelSettings);
    syncChannelSettings.videoInput = NEXUS_VideoDecoder_GetConnector(videoDecoder);
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    syncChannelSettings.audioInput[0] = NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo);
    syncChannelSettings.audioInput[1] = NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed);
#endif
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

    NEXUS_VideoEncoder_Start(videoEncoder, &videoEncoderStartConfig);
    /* Playback state machine is driven from inside Nexus. */
    printf("Press ENTER to continue; type 'q' to quit\n");
    key = getchar();

    /* Bring down system */
    NEXUS_Playback_Stop(playback);
    NEXUS_VideoDecoder_Stop(videoDecoder);
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_AudioDecoder_Stop(audioPassthrough);

    NEXUS_AudioDecoder_Stop(audioDecoder);
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
    NEXUS_FileMux_Finish(fileMux);
    /* wait for encode buffer to be drained; double delay margin */
    while(BKNI_WaitForEvent(finishEvent, 3000)!=BERR_SUCCESS) {
        fprintf(stderr, "File mux unfinished! Wait for another 3 seconds...\n");
    }
    BKNI_DestroyEvent(finishEvent);
    NEXUS_FileMux_Stop(fileMux);
    NEXUS_MuxFile_Close(muxOutput);

	
#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
    NEXUS_Playback_ClosePidChannel(playback, audioPidChannel);
#endif
    NEXUS_Playback_ClosePidChannel(playback, videoPidChannel);
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

    NEXUS_FileMux_Destroy(fileMux);

#if BTST_HAS_VIDEO_ENCODE_TEST
	NEXUS_VideoEncoder_Close(videoEncoder);
#endif    

#if NEXUS_HAS_AUDIO_MUX_OUTPUT_TEST    
#if BTST_ENABLE_AUDIO_ENCODE 
	NEXUS_AudioOutput_RemoveInput(
		NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput), NEXUS_AudioEncoder_GetConnector(audioEncoder));
	NEXUS_AudioEncoder_RemoveInput(audioEncoder,
								NEXUS_AudioDecoder_GetConnector(audioDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    NEXUS_AudioInput_Shutdown(NEXUS_AudioEncoder_GetConnector(audioEncoder));
    NEXUS_AudioOutput_Shutdown(NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput));
    NEXUS_AudioEncoder_Close(audioEncoder);
#else    
	NEXUS_AudioOutput_RemoveInput(
		NEXUS_AudioMuxOutput_GetConnector(audioMuxOutput),
		NEXUS_AudioDecoder_GetConnector(audioPassthrough, NEXUS_AudioDecoderConnectorType_eCompressed));
#endif
    NEXUS_AudioOutput_RemoveAllInputs(NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]));
    NEXUS_AudioOutput_RemoveAllInputs(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]));

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
    
    if(key != 'q')
    {
        i = iteration++%TEST_ITERATIONS;
        BDBG_WRN(("Start %d iteration.....", iteration));
        goto again;
    }

    NEXUS_Platform_Uninit();

#endif
    return 0;
}


