/* Nexus example app: playback and decode */

#include "nexus_platform.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_spdif_output.h"
#include "nexus_component_output.h"
#if NEXUS_HAS_PLAYBACK && NEXUS_HAS_VIDEO_ENCODER
#include "nexus_video_encoder_output.h"
#include "nexus_playback.h"
#include "nexus_file.h"
#include "nexus_video_encoder.h"
#endif
#if NEXUS_DTV_PLATFORM
#include "nexus_platform_boardcfg.h"
#endif

#include <stdio.h>
#include <assert.h>
#include "bstd.h"
#include "bkni.h"
#include "bdbg.h"

BDBG_MODULE(encode_video);
int main(void)  {
#if NEXUS_HAS_PLAYBACK && NEXUS_HAS_VIDEO_ENCODER
    NEXUS_PlatformSettings platformSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_StcChannelHandle stcChannel, stcChannelEncoder;
    NEXUS_StcChannelSettings stcSettings;
    NEXUS_PidChannelHandle videoPidChannel;
    NEXUS_VideoDecoderHandle videoDecoder;
    NEXUS_VideoDecoderStartSettings videoProgram;
    NEXUS_FilePlayHandle file;
    NEXUS_PlaypumpHandle playpump;
    NEXUS_PlaybackHandle playback;
    NEXUS_PlaybackSettings playbackSettings;
    NEXUS_PlaybackPidChannelSettings playbackPidSettings;

    NEXUS_DisplayHandle displayTranscode;
    NEXUS_VideoWindowHandle windowTranscode;
    NEXUS_VideoEncoderHandle videoEncoder;
    NEXUS_VideoEncoderSettings videoEncoderConfig;
    NEXUS_VideoEncoderDelayRange videoDelay;
    NEXUS_VideoEncoderStartSettings videoEncoderStartConfig;
    NEXUS_VideoEncoderStatus videoEncoderStatus;
    FILE *fout;
    size_t bytes;


    NEXUS_DisplaySettings displaySettings;
    const char *fname = "/data/stream/H264_ac3.ts";
    FILE *fdesc;

    fdesc = fopen("/data/stream/video_desc.txt","w");
	if (fdesc == NULL) {
		printf("failed to open /data/stream/video_desc.txt\n");
		return -1;
	}
    fprintf(fdesc, "flags(h) origPts(h) pts(Lh)     escr(h) ticksPerBit(u) shr(d) offset(h) length(h)\n");

    NEXUS_Platform_GetDefaultSettings(&platformSettings);
    platformSettings.openFrontend = false;
    NEXUS_Platform_Init(&platformSettings);
    NEXUS_Platform_GetConfiguration(&platformConfig);

    playpump = NEXUS_Playpump_Open(0, NULL);
    assert(playpump);
    playback = NEXUS_Playback_Create();
    assert(playback);

    file = NEXUS_FilePlay_OpenPosix(fname, NULL);
    if (!file) {
        fprintf(stderr, "can't open file:%s\n", fname);
        return -1;
    }

    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    stcChannel = NEXUS_StcChannel_Open(0, &stcSettings);

    NEXUS_Playback_GetSettings(playback, &playbackSettings);
    playbackSettings.playpump = playpump;
    /* set a stream format, it could be any audio video transport type or file format, i.e NEXUS_TransportType_eMp4, NEXUS_TransportType_eAvi ... */
    playbackSettings.playpumpSettings.transportType = NEXUS_TransportType_eTs;
    playbackSettings.stcChannel = stcChannel;
    NEXUS_Playback_SetSettings(playback, &playbackSettings);


    /* bring up decoder and connect to local display */
    videoDecoder = NEXUS_VideoDecoder_Open(0, NULL); /* take default capabilities */

    /* NOTE: must open video encoder before display; otherwise open will init ViCE2 core
     * which might cause encoder display GISB error since encoder display would
     * trigger RDC to program mailbox registers in ViCE2;
     */
    videoEncoder = NEXUS_VideoEncoder_Open(0, NULL);
    assert(videoEncoder);

    /* Bring up video encoder display */
    NEXUS_Display_GetDefaultSettings(&displaySettings);
    displaySettings.displayType = NEXUS_DisplayType_eAuto;
    displaySettings.timingGenerator = NEXUS_DisplayTimingGenerator_eEncoder;
    displaySettings.format = NEXUS_VideoFormat_e720p24hz;/* bring up 480p first */
    displayTranscode = NEXUS_Display_Open(NEXUS_ENCODER_DISPLAY_IDX, &displaySettings);
    assert(displayTranscode);

    windowTranscode = NEXUS_VideoWindow_Open(displayTranscode, 0);
    assert(windowTranscode);

    /* connect same decoder to encoder display 
     * This simul mode is for video encoder bringup only; audio path may have limitation
     * for simul display+transcode mode;
     */
    NEXUS_VideoWindow_AddInput(windowTranscode, NEXUS_VideoDecoder_GetConnector(videoDecoder));

    /* Open the video pid channel */
    NEXUS_Playback_GetDefaultPidChannelSettings(&playbackPidSettings);
    playbackPidSettings.pidSettings.pidType = NEXUS_PidType_eVideo;
    playbackPidSettings.pidTypeSettings.video.codec = NEXUS_VideoCodec_eH264; /* must be told codec for correct handling */
    playbackPidSettings.pidTypeSettings.video.index = true;
    playbackPidSettings.pidTypeSettings.video.decoder = videoDecoder;
    videoPidChannel = NEXUS_Playback_OpenPidChannel(playback, 0x0101, &playbackPidSettings);

    /* Set up decoder Start structures now. We need to know the audio codec to properly set up
    the audio outputs. */
    NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
    videoProgram.codec = NEXUS_VideoCodec_eH264;
    videoProgram.pidChannel = videoPidChannel;
    videoProgram.stcChannel = stcChannel;
 
     NEXUS_VideoEncoder_GetSettings(videoEncoder, &videoEncoderConfig);
     videoEncoderConfig.variableFrameRate = true;
    videoEncoderConfig.frameRate = NEXUS_VideoFrameRate_e24;
    videoEncoderConfig.bitrateMax = 6*1000*1000;
    videoEncoderConfig.streamStructure.trackInput = false;
    videoEncoderConfig.streamStructure.framesP = 29; /* IPP GOP size = 30 */
    videoEncoderConfig.streamStructure.framesB = 0;

    /* Start decoders */
    NEXUS_VideoDecoder_Start(videoDecoder, &videoProgram);

    /* Start playback */
    NEXUS_Playback_Start(playback, file, NULL);

    BKNI_Sleep(1000);

    /* encoder requires different STC broadcast mode from decoder */
    NEXUS_StcChannel_GetDefaultSettings(1, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcSettings.mode = NEXUS_StcChannelMode_eAuto;
    stcSettings.pcrBits = NEXUS_StcChannel_PcrBits_eFull42;/* ViCE2 requires 42-bit STC broadcast */
    stcChannelEncoder = NEXUS_StcChannel_Open(1, &stcSettings);

    NEXUS_VideoEncoder_GetDefaultStartSettings(&videoEncoderStartConfig);
    videoEncoderStartConfig.codec = NEXUS_VideoCodec_eH264;
    videoEncoderStartConfig.profile = NEXUS_VideoCodecProfile_eBaseline;
    videoEncoderStartConfig.level = NEXUS_VideoCodecLevel_e31;
    videoEncoderStartConfig.input = displayTranscode;
    videoEncoderStartConfig.stcChannel = stcChannelEncoder;

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
	videoEncoderStartConfig.bounds.inputDimension.max.width = 720;
	videoEncoderStartConfig.bounds.inputDimension.max.height = 480;

    /* NOTE: video encoder delay is in 27MHz ticks */
    NEXUS_VideoEncoder_GetDelayRange(videoEncoder, &videoEncoderConfig, &videoEncoderStartConfig, &videoDelay);
    BDBG_WRN(("\n\tVideo encoder end-to-end delay = [%u ~ %u] ms", videoDelay.min/27000, videoDelay.max/27000));
    videoEncoderConfig.encoderDelay = videoDelay.min;

    /* note the Dee is set by SetSettings */
    NEXUS_VideoEncoder_SetSettings(videoEncoder, &videoEncoderConfig);
    NEXUS_VideoEncoder_Start(videoEncoder, &videoEncoderStartConfig);
    NEXUS_VideoEncoder_GetStatus(videoEncoder, &videoEncoderStatus);

    fout = fopen("/data/stream/converted.ts","wb");
    assert(fout);

    for(bytes=0;;) {
        size_t size[2];
        const NEXUS_VideoEncoderDescriptor *desc[2];
        unsigned i,j;
        unsigned descs;

        
        NEXUS_VideoEncoder_GetBuffer(videoEncoder, &desc[0], &size[0], &desc[1], &size[1]);
        if(size[0]==0 && size[1]==0) {
            NEXUS_VideoDecoderStatus vstatus;

            NEXUS_VideoDecoder_GetStatus(videoDecoder, &vstatus);
            fflush(fout);
            fprintf(stderr, "written %u bytes.... decode:%u\t\r", bytes, vstatus.pts);
            BKNI_Sleep(30);
            continue;
        }
        for(descs=0,j=0;j<2;j++) {
            descs+=size[j];
            for(i=0;i<size[j];i++) {
                if(desc[j][i].length > 0)
                {
                fwrite((const uint8_t *)videoEncoderStatus.bufferBase + desc[j][i].offset, desc[j][i].length, 1, fout);
#if 0
                fprintf(fdesc, "%8x %8x   %x%08x %08x     %5u   %5d   %8x %8x\n", desc[j][i].flags, desc[j][i].originalPTS, 
                    (uint32_t)(desc[j][i].pts>>32), (uint32_t)(desc[j][i].pts & 0xffffffff), desc[j][i].escr, 
                    desc[j][i].ticksPerBit, desc[j][i].shr, desc[j][i].offset, desc[j][i].length);
#endif
                bytes+= desc[j][i].length;
                if(desc[j][i].length > 0x100000)
                {
                    BDBG_ERR(("++++ desc[%d][%d] length = 0x%x, offset=0x%x", j,i, desc[j][i].length, desc[j][i].offset));
                }
                   
                }
            }
        }
        NEXUS_VideoEncoder_ReadComplete(videoEncoder, descs);
    }
    fclose(fout);

    /* Bring down system */
    NEXUS_VideoDecoder_Stop(videoDecoder);
    NEXUS_Playback_Stop(playback);
    NEXUS_FilePlay_Close(file);
    NEXUS_Playback_Destroy(playback);
    NEXUS_Playpump_Close(playpump);
    NEXUS_VideoInput_Shutdown(NEXUS_VideoDecoder_GetConnector(videoDecoder));
    NEXUS_VideoDecoder_Close(videoDecoder);
    NEXUS_StcChannel_Close(stcChannel);

    NEXUS_Display_Close(displayTranscode);
    NEXUS_VideoEncoder_Close(videoEncoder);
    NEXUS_StcChannel_Close(stcChannelEncoder);
    
    NEXUS_Platform_Uninit();

#endif
    return 0;
}


