#include "nexus_frontend.h"
#include "nexus_platform.h"
#include "nexus_parser_band.h"
#include "nexus_video_decoder.h"
#include "nexus_stc_channel.h"
#include "nexus_display.h"
#include "nexus_video_window.h"
#include "nexus_video_input.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_decoder.h"
#include "nexus_audio_output.h"
#include "nexus_audio_input.h"
#include "nexus_still_decoder.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void lock_callback(void *context, int param)
{
    NEXUS_FrontendHandle frontend = (NEXUS_FrontendHandle)context;
    NEXUS_FrontendQamStatus qamStatus;


    fprintf(stderr, "Lock callback, frontend 0x%08x\n", (unsigned)frontend);

    NEXUS_Frontend_GetQamStatus(frontend, &qamStatus);
    fprintf(stderr, "QAM Lock callback, frontend 0x%08x - lock status %d, %d\n", (unsigned)frontend,
            qamStatus.fecLock, qamStatus.receiverLock);
}

int main(int argc, char **argv)
{
    NEXUS_FrontendUserParameters userParams;
    NEXUS_FrontendHandle frontend=NULL;
    NEXUS_FrontendQamSettings qamSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_ParserBand parserBand;
    NEXUS_ParserBandSettings parserBandSettings;
    NEXUS_StcChannelHandle stcChannel;
    NEXUS_StcChannelSettings stcSettings;
    NEXUS_DisplayHandle display;
    NEXUS_DisplayHandle display1;
    NEXUS_DisplaySettings displaySettings;
    NEXUS_VideoWindowHandle window;
    NEXUS_VideoWindowHandle window1;
	NEXUS_VideoWindowSettings windowSettings;
    NEXUS_VideoDecoderHandle videoDecoder;
	NEXUS_StillDecoderHandle stillDecoder;
    NEXUS_AudioDecoderHandle pcmDecoder;
    NEXUS_AudioDecoderHandle compressedDecoder;
	//NEXUS_AudioMixerHandle audioMixer;

    unsigned i;
    /* default freq & qam mode */
    unsigned freq = 765;
    NEXUS_FrontendQamMode qamMode = NEXUS_FrontendQamMode_e64;

    /* allow cmdline freq & qam mode for simple test */
    if (argc > 1) {
        freq = atoi(argv[1]);
    }
    if (argc > 2) {
        unsigned mode = atoi(argv[2]);
        switch (mode) {
        case 64: qamMode = NEXUS_FrontendQamMode_e64; break;
        case 256: qamMode = NEXUS_FrontendQamMode_e256; break;
        case 1024: qamMode = NEXUS_FrontendQamMode_e1024; break;
        default: printf("unknown qam mode %d\n", mode); return -1;
        }
    }

    NEXUS_Platform_Init(NULL);
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
    qamSettings.frequency = freq * 1000000;
    qamSettings.mode = qamMode;
    switch (qamMode) {
    default:
    case NEXUS_FrontendQamMode_e64: qamSettings.symbolRate = 5056900; break;
    case NEXUS_FrontendQamMode_e256: qamSettings.symbolRate = 5360537; break;
    case NEXUS_FrontendQamMode_e1024: qamSettings.symbolRate = 0; /* TODO */ break; 
    }
    qamSettings.annex = NEXUS_FrontendQamAnnex_eB;
    qamSettings.bandwidth = NEXUS_FrontendQamBandwidth_e6Mhz;
    qamSettings.lockCallback.callback = lock_callback;
    qamSettings.lockCallback.context = frontend;

    NEXUS_Frontend_GetUserParameters(frontend, &userParams);

    /* Map a parser band to the demod's input band. */
    parserBand = NEXUS_ParserBand_e0;
#if 0
    NEXUS_ParserBand_GetSettings(parserBand, &parserBandSettings);
    parserBandSettings.sourceType = NEXUS_ParserBandSourceType_eInputBand;
    parserBandSettings.sourceTypeSettings.inputBand = userParams.param1;  /* Platform initializes this to input band */
    parserBandSettings.transportType = NEXUS_TransportType_eTs;
    NEXUS_ParserBand_SetSettings(parserBand, &parserBandSettings);
#endif

    NEXUS_StcChannel_GetDefaultSettings(0, &stcSettings);
    stcSettings.timebase = NEXUS_Timebase_e0;
    stcChannel = NEXUS_StcChannel_Open(0, &stcSettings);

	//audioMixer = NEXUS_AudioMixer_Open(NULL);
    pcmDecoder = NEXUS_AudioDecoder_Open(0, NULL);
    compressedDecoder = NEXUS_AudioDecoder_Open(1, NULL);
    NEXUS_AudioOutput_AddInput(
        NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]),
        NEXUS_AudioDecoder_GetConnector(pcmDecoder, NEXUS_AudioDecoderConnectorType_eStereo));
    NEXUS_AudioOutput_AddInput(
        NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]),
        NEXUS_AudioDecoder_GetConnector(compressedDecoder, NEXUS_AudioDecoderConnectorType_eCompressed));

	/*
	NEXUS_AudioMixer_AddInput(audioMixer, NEXUS_AudioDecoder_GetConnector(pcmDecoder,
				NEXUS_AudioDecoderConnectorType_eStereo));
	NEXUS_AudioMixer_AddInput(audioMixer, NEXUS_AudioDecoder_GetConnector(compressedDecoder,
				NEXUS_AudioDecoderConnectorType_eCompressed);
				*/
    NEXUS_Display_GetDefaultSettings(&displaySettings);
    //displaySettings.format = NEXUS_VideoFormat_eNtsc;
    displaySettings.format = NEXUS_VideoFormat_e1080i;
    display = NEXUS_Display_Open(0, &displaySettings);
    NEXUS_Display_AddOutput(display, NEXUS_HdmiOutput_GetVideoConnector(platformConfig.outputs.hdmi[0]));
    window = NEXUS_VideoWindow_Open(display, 0);
    videoDecoder = NEXUS_VideoDecoder_Open(0, NULL);
    NEXUS_VideoWindow_AddInput(window, NEXUS_VideoDecoder_GetConnector(videoDecoder));

	NEXUS_Display_GetDefaultSettings(&displaySettings);
	displaySettings.format = NEXUS_VideoFormat_eNtsc;
	display1 = NEXUS_Display_Open(1, &displaySettings);
	window1 = NEXUS_VideoWindow_Open(display1, 0);

	NEXUS_VideoWindow_GetSettings(window1, &windowSettings);
	windowSettings.contentMode = NEXUS_VideoWindowContentMode_eFull;
	NEXUS_VideoWindow_SetSettings(window1, &windowSettings);

	NEXUS_VideoWindow_AddInput(window1, NEXUS_VideoDecoder_GetConnector(videoDecoder));
	NEXUS_Display_AddOutput(display1, NEXUS_CompositeOutput_GetConnector(platformConfig.outputs.composite[0]));

        NEXUS_PidChannelHandle videoPidChannel, audioPidChannel;
        NEXUS_VideoDecoderStartSettings videoProgram;
        NEXUS_AudioDecoderStartSettings audioProgram;

        //videoPidChannel = NEXUS_PidChannel_Open(parserBand, 289, NULL);
        //audioPidChannel = NEXUS_PidChannel_Open(parserBand, 290, NULL);
        //videoPidChannel = NEXUS_PidChannel_Open(parserBand, 3000, NULL);
        //audioPidChannel = NEXUS_PidChannel_Open(parserBand, 3001, NULL);
		printf("jcjung:...\n");
        videoPidChannel = NEXUS_PidChannel_Open(parserBand, 515, NULL);
        audioPidChannel = NEXUS_PidChannel_Open(parserBand, 516, NULL);

        NEXUS_VideoDecoder_GetDefaultStartSettings(&videoProgram);
        //videoProgram.codec = NEXUS_VideoCodec_eH264;
        videoProgram.codec = NEXUS_VideoCodec_eMpeg2;
        videoProgram.pidChannel = videoPidChannel;
        //videoProgram.stcChannel = stcChannel;
        NEXUS_AudioDecoder_GetDefaultStartSettings(&audioProgram);
        audioProgram.codec = NEXUS_AudioCodec_eMpeg;
        audioProgram.pidChannel = audioPidChannel;
        //audioProgram.stcChannel = stcChannel;

        //NEXUS_StcChannel_GetSettings(stcChannel, &stcSettings);
        //stcSettings.mode = NEXUS_StcChannelMode_ePcr; /* live */
        //stcSettings.modeSettings.pcr.pidChannel = videoPidChannel; /* PCR happens to be on video pid */
    //    NEXUS_StcChannel_SetSettings(stcChannel, &stcSettings);

        NEXUS_Frontend_TuneQam(frontend, &qamSettings);

		printf("frontend : %d\n", frontend);

        NEXUS_VideoDecoder_Start(videoDecoder, &videoProgram);
        NEXUS_AudioDecoder_Start(compressedDecoder, &audioProgram);

        printf("Press enter to tune again\n");
        getchar();

        NEXUS_AudioDecoder_Stop(compressedDecoder);
        NEXUS_VideoDecoder_Stop(videoDecoder);
        NEXUS_PidChannel_Close(videoPidChannel);
        NEXUS_PidChannel_Close(audioPidChannel);
    return 0;
}
