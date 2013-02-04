/******************************************************************************
 *    (c)2008-2009 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its licensors,
 * and may only be used, duplicated, modified or distributed pursuant to the terms and
 * conditions of a separate, written license agreement executed between you and Broadcom
 * (an "Authorized License").  Except as set forth in an Authorized License, Broadcom grants
 * no license (express or implied), right to use, or waiver of any kind with respect to the
 * Software, and Broadcom expressly reserves all rights in and to the Software and all
 * intellectual property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
 * HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
 * NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1.     This program, including its structure, sequence and organization, constitutes the valuable trade
 * secrets of Broadcom, and you shall use all reasonable efforts to protect the confidentiality thereof,
 * and to use this information only in connection with your use of Broadcom integrated circuit products.
 *
 * 2.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES
 * OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE,
 * LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION
 * OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF
 * USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3.     TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 * LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 * EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR
 * USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT
 * ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE
 * LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF
 * ANY LIMITED REMEDY.
 *
 * $brcm_Workfile: tune_analog.c $
 * $brcm_Revision: 18 $
 * $brcm_Date: 10/26/09 5:12p $
 *
 * Module Description:
 *
 * Revision History:
 *
 * $brcm_Log: /nexus/examples/frontend/tune_analog.c $
 * 
 * 18   10/26/09 5:12p jgarrett
 * SW3556-823: Adding AASD standard to RfAudioDecoderStatus
 * 
 * 17   10/26/09 2:09p jgarrett
 * SW3556-820: Expanding test application to include second display and
 *  SECAM_L_PRIME
 * 
 * 16   4/9/09 7:05p jgarrett
 * PR 52275: Improving debug messages for AASD status
 * 
 * 15   3/11/09 9:55a erickson
 * PR52991: add IFD->composite output bypass
 *
 * 14   1/14/09 6:32p ahulse
 * PR51128: Reset Analog Video Decoder after channel change
 *
 * 13   11/21/08 2:49p jgarrett
 * PR 47439: Adding generic status callback
 *
 * 12   10/22/08 2:39p erickson
 * PR48023: pq api change
 *
 * 11   10/17/08 11:05a jgarrett
 * PR 45171: Adding option to restart RF audio decoder
 *
 * 10   8/28/08 3:12p jgarrett
 * PR 45171: Adding AV Ratio
 *
 * 9   8/8/08 7:16p jgarrett
 * PR 45171: Adding AASD options
 *
 * 8   7/18/08 11:31a jgarrett
 * PR 44953: Removing -Wstrict-prototypes warnings
 *
 * 7   6/19/08 5:01p jgarrett
 * PR 42360: Adding support for NICAM and A2
 *
 * 6   6/19/08 2:32p vsilyaev
 * PR 40921: There is no component output on the 93556 platform
 *
 * 5   5/14/08 1:50p erickson
 * PR39453: 3548 modes
 *
 * 4   3/26/08 5:24p jgarrett
 * PR 40113: Adding option to check audio status
 *
 * 3   2/22/08 4:51p erickson
 * PR39888: replace scanf
 *
 * 2   1/23/08 12:39p erickson
 * PR38919: rename NEXUS_Display_GetWindow to NEXUS_VideoWindow_Open
 *
******************************************************************************/
/* Example to tune an analog channel using nexus */

#include "nexus_platform.h"
#include "nexus_display.h"
#include "nexus_video_input.h"
#include "nexus_composite_input.h"
#include "nexus_svideo_input.h"
#include "nexus_component_input.h"
#include "nexus_analog_video_decoder.h"
#include "nexus_component_output.h"
#include "nexus_video_window.h"
#include "nexus_video_adj.h"
#include "nexus_picture_ctrl.h"
#include "nexus_rf_audio_decoder.h"
#include "nexus_audio_dac.h"
#include "nexus_audio_output.h"
#if NEXUS_DTV_PLATFORM
#include "nexus_platform_boardcfg.h"
#endif

#include "bstd.h"
#include "bkni.h"
#include <stdio.h>
#include <stdlib.h>

#define TERRESTRIAL 0
#define CABLE_STD 1
#define CABLE_IRC 2
#define CABLE_HRC 3

/* Uncomment one of these to test a mode other than US-BTSC */
#if 0
#define KOREA_A2 1
#define SECAM_L 1
#define SECAM_L_PRIME 1
#define NICAM 1
#define PAL_A2 1
#define NICAM 1
#define AASD 1
#endif

#if 0
#define SIF 1 /* Uncomment this to use CVBS+SIF input instead of IF */
#define CVBS_IF 1  /* Uncomment thsi to use CVBS input instead of RF.  Audio is still RF */
#endif

static const char *get_mode_string(int mode)
{
    static const char *modes[] =
    {
        "terrestrial",
        "cable-std",
        "cable-irc",
        "cable-hrc"
    };

    if ( mode < 4 )
    {
        return modes[mode];
    }

    return NULL;
}

static unsigned get_frequency(int channel, int cable)
{
    int hz=0;

    if ( cable )
    {
        if ( channel < 1 )
        {
            hz = 0;
        }
        else if ( channel == 1 )
        {
            if ( cable == CABLE_STD )
                hz = 0;    /* Standard cable has no ch. 1 */
            else
                hz = (cable == CABLE_IRC)?75000000:73750000; /* LO band */
        }
        else if ( channel < 5 )
        {
            hz = (cable == CABLE_HRC)?55750000:57000000; /* LO band - std=irc */
            hz += 6000000*(channel-2);
        }
        else if ( channel < 7 )
        {
            hz = (cable != CABLE_IRC)?79750000:81000000; /* LO band - std=irc */
            hz += 6000000*(channel-5);
        }
        else if ( channel < 14 )
        {
            hz = (cable == CABLE_HRC)?175750000:177000000; /* HI band - std=irc */
            hz += 6000000*(channel-7);
        }
        else if ( channel < 23 )
        {
            hz = (cable == CABLE_HRC)?121750000:123000000; /* MID band - std=irc */
            hz += 6000000*(channel-14);
        }
        else if ( channel < 37 )
        {
            hz = (cable == CABLE_HRC)?217750000:219000000; /* SUPER band - std=irc */
            hz += 6000000*(channel-23);
        }
        else if ( channel < 79 )
        {
            hz = (cable == CABLE_HRC)?301750000:303000000; /* HYPER band - std=irc */
            hz += 6000000*(channel - 37);
        }
        else if ( channel >= 95 && channel <= 99 )
        {
            hz = (cable == CABLE_IRC)?91750000:93000000;   /* MID band - std=hrc */
            hz += 6000000*(channel-95);
        }
    }
    else
    {
        if ( channel < 2 )
        {
            hz = 0;
        }
        else if ( channel < 7 )
        {
            hz = 57000000 + (6000000*(channel-2));
        }
        else if ( channel < 14 )
        {
            hz = 177000000 + (6000000*(channel-7));
        }
        else if ( channel < 70 )
        {
            hz = 473000000 + (6000000*(channel-14));
        }
        else if ( channel < 83 )
        {
            hz = 809000000 + (6000000*(channel-70));
        }
        else
        {
            hz = 0;
        }
    }

    return hz;
}

static void lock_callback(void *context, int param)
{
    NEXUS_FrontendHandle frontend = (NEXUS_FrontendHandle)context;
    NEXUS_FrontendAnalogStatus status;

    param=param;
    fprintf(stderr, "Lock callback, frontend 0x%08x\n", (unsigned)frontend);

    NEXUS_Frontend_GetAnalogStatus(frontend, &status);
    fprintf(stderr, "Lock callback, frontend 0x%08x - lock status %d [carrier offset %d Hz - %s %s]\n",
            (unsigned)frontend, status.locked, status.carrierOffset, status.stereoDetected?"Stereo":"Mono", status.sapDetected?"SAP":"No SAP");
}

#if AASD
static const char *standard_name(NEXUS_RfAudioDecoderStandard standard)
{
    switch ( standard )
    {
    case NEXUS_RfAudioDecoderStandard_eBGH:
        return "B/G/H";
    case NEXUS_RfAudioDecoderStandard_eI:
        return "I";
    case NEXUS_RfAudioDecoderStandard_eDK:
        return "DK";
    case NEXUS_RfAudioDecoderStandard_eDK1:
        return "DK1";
    case NEXUS_RfAudioDecoderStandard_eDK2:
        return "DK2";
    case NEXUS_RfAudioDecoderStandard_eDK3:
        return "DK3";
    case NEXUS_RfAudioDecoderStandard_eL:
        return "L";
    default:
        return "unknown";
    }
}
#endif

static void status_callback(void *context, int param)
{
    NEXUS_RfAudioDecoderStatus audioStatus;
    NEXUS_RfAudioDecoderHandle rfAudio = context;
    NEXUS_FrontendAnalogStatus ifdStatus;
    NEXUS_FrontendHandle frontend = (NEXUS_FrontendHandle)param;

    NEXUS_Frontend_GetAnalogStatus(frontend, &ifdStatus);
    NEXUS_RfAudioDecoder_GetStatus(rfAudio, &audioStatus);

#if AASD
    /* This function is only called while running AASD */
    if ( !ifdStatus.locked )
    {
        fprintf(stderr, "RF Decoder Status: No Signal\n");
    }
    else if ( audioStatus.noisePresent )
    {
        fprintf(stderr, "RF Decoder Status: Searching\n");
    }
    else if ( audioStatus.currentMode == NEXUS_RfAudioDecoderMode_eAutoNicamPalA2 )
    {
        fprintf(stderr, "RF Decoder Status: Found FM Carrier (%s), searching for NICAM/A2\n", standard_name(audioStatus.currentStandard));
    }
    else if ( audioStatus.currentMode == NEXUS_RfAudioDecoderMode_eNicam )
    {
        fprintf(stderr, "RF Decoder Status: Found NICAM (%s) - stereo %d, dual %d\n", standard_name(audioStatus.currentStandard), 
                audioStatus.stereoPresent, audioStatus.dualMonoPresent);
    }
    else if ( audioStatus.currentMode == NEXUS_RfAudioDecoderMode_ePalA2 )
    {
        fprintf(stderr, "RF Decoder Status: Found A2 (%s)\n", standard_name(audioStatus.currentStandard));
    }
    else
    {
        /* Shouldn't be here... */
        fprintf(stderr, "RF Decoder Status: Unknown\n");
    }
#else
    fprintf(stderr, "RF Audio decoder status update: noise=%d stereo=%d sap=%d dual=%d nicam=%d\n",
            audioStatus.noisePresent, audioStatus.stereoPresent,
            audioStatus.sapPresent, audioStatus.dualMonoPresent,
            audioStatus.nicamPresent);
#endif
}

int get_number(void)
{
    char buf[256];
    fgets(buf, 256, stdin);
    return atoi(buf);
}

int main(void)
{
    NEXUS_FrontendHandle frontend;
    NEXUS_DisplayHandle display, display2;
    NEXUS_DisplaySettings displayCfg;
    NEXUS_VideoWindowHandle window, window2;
    NEXUS_VideoWindowMadSettings madSettings;
    NEXUS_VideoWindowAnrSettings anrSettings;
    NEXUS_AnalogVideoDecoderHandle analogVideoDecoder;
    NEXUS_AnalogVideoDecoderSettings analogVideoDecoderSettings;
    NEXUS_PlatformConfiguration platformConfig;
    NEXUS_FrontendAnalogSettings frontendAnalogSettings;
    NEXUS_RfAudioDecoderHandle audioDecoder;
    NEXUS_RfAudioDecoderSettings audioSettings;
    int done = 0, i;
    int cmd;
    int mode = CABLE_STD;

    /* Bring up all modules for a platform in a default configuration for this platform */
    NEXUS_Platform_Init(NULL);
    NEXUS_Platform_GetConfiguration(&platformConfig);

    for ( i = 0; i < NEXUS_MAX_FRONTENDS; i++ )
    {
        NEXUS_FrontendCapabilities capabilities;
        frontend = platformConfig.frontend[i];
        if (frontend) {
            NEXUS_Frontend_GetCapabilities(frontend, &capabilities);
            /* Does this frontend support analog? */
            if ( capabilities.analog && capabilities.ifd )
            {
                break;
            }
        }
    }

    if (NULL == frontend )
    {
        fprintf(stderr, "Unable to find analog-capable frontend\n");
        return 0;
    }

    /* Bring up display for 720p component output only */
    NEXUS_Display_GetDefaultSettings(&displayCfg);
#if NICAM || PAL_A2 || SECAM_L || SECAM_L_PRIME || AASD
    displayCfg.format = NEXUS_VideoFormat_eCustom1;
#endif
    display = NEXUS_Display_Open(0, &displayCfg);
#if NEXUS_DTV_PLATFORM
    NEXUS_Display_AddOutput(display, NEXUS_PanelOutput_GetConnector(platformConfig.outputs.panel[0]));
    NEXUS_BoardCfg_ConfigurePanel(true, true, true);

    if ( displayCfg.format == NEXUS_VideoFormat_eCustom1 )
    {
        /* 50Hz panel */
        displayCfg.format = NEXUS_VideoFormat_ePal;
    }
    else
    {
        /* Assume 60Hz */
        displayCfg.format = NEXUS_VideoFormat_eNtsc;
    }
    displayCfg.displayType = NEXUS_DisplayType_eAuto;
    display2 = NEXUS_Display_Open(1, &displayCfg);
#if NEXUS_NUM_SCART_INPUTS
    NEXUS_Display_AddOutput(display2, NEXUS_ScartInput_GetVideoOutputConnector(platformConfig.inputs.scart[0]));
#else
    NEXUS_Display_AddOutput(display2, NEXUS_CompositeOutput_GetConnector(platformConfig.outputs.composite[0]));
#endif

#else
#error DTV only
#endif

    window = NEXUS_VideoWindow_Open(display, 0);
    window2 = NEXUS_VideoWindow_Open(display2, 0);

    /* Tune Frontend */
    NEXUS_Frontend_GetDefaultAnalogSettings(&frontendAnalogSettings);
    #if KOREA_A2
    frontendAnalogSettings.frequency = get_frequency(8, mode);
    frontendAnalogSettings.ifdSettings.audioMode = NEXUS_IfdAudioMode_eKorea;
    #elif NICAM
    frontendAnalogSettings.frequency = 474000000;
    frontendAnalogSettings.ifdSettings.carrierFrequency = 38900000;
    frontendAnalogSettings.ifdSettings.videoFormat = NEXUS_VideoFormat_ePalI;
    frontendAnalogSettings.ifdSettings.audioMode = NEXUS_IfdAudioMode_eNicam;
    #elif SECAM_L || SECAM_L_PRIME
    frontendAnalogSettings.frequency = 474000000;
    frontendAnalogSettings.ifdSettings.carrierFrequency = 38900000;
    frontendAnalogSettings.ifdSettings.videoFormat = NEXUS_VideoFormat_eSecamL;
    frontendAnalogSettings.ifdSettings.audioMode = NEXUS_IfdAudioMode_eNicam;
    #if SECAM_L_PRIME
    frontendAnalogSettings.ifdSettings.spectrumInverted = true;
    #endif
    #elif PAL_A2
    frontendAnalogSettings.frequency = 474000000;
    frontendAnalogSettings.ifdSettings.carrierFrequency = 38900000;
    frontendAnalogSettings.ifdSettings.videoFormat = NEXUS_VideoFormat_ePalB;
    frontendAnalogSettings.ifdSettings.audioMode = NEXUS_IfdAudioMode_ePalA2;
    #elif AASD
    frontendAnalogSettings.frequency = 474000000;
    frontendAnalogSettings.ifdSettings.carrierFrequency = 38900000;
    frontendAnalogSettings.ifdSettings.videoFormat = NEXUS_VideoFormat_ePal;
    frontendAnalogSettings.ifdSettings.audioMode = NEXUS_IfdAudioMode_eAutoNicamPalA2;
    #else
    frontendAnalogSettings.frequency = get_frequency(8, mode);
    #endif
    frontendAnalogSettings.ifdSettings.lockCallback.callback = lock_callback;
    frontendAnalogSettings.ifdSettings.lockCallback.context = frontend;

#if SIF
    frontendAnalogSettings.ifdSettings.videoInputType = NEXUS_IfdInputType_eNone;
    frontendAnalogSettings.ifdSettings.audioInputType = NEXUS_IfdInputType_eSif;
#endif

    NEXUS_Frontend_TuneAnalog(frontend, &frontendAnalogSettings);

    /* bring up VDEC */
    analogVideoDecoder = NEXUS_AnalogVideoDecoder_Open(0, NULL);
    NEXUS_VideoWindow_AddInput(window, NEXUS_AnalogVideoDecoder_GetConnector(analogVideoDecoder));
    NEXUS_VideoWindow_AddInput(window2, NEXUS_AnalogVideoDecoder_GetConnector(analogVideoDecoder));
    #if SIF
    {
        NEXUS_CompositeInputSettings cvbsInputSettings;
        /* Set CVBS 0 as the external IFD input */
        NEXUS_CompositeInput_GetSettings(platformConfig.inputs.composite[0], &cvbsInputSettings);
        cvbsInputSettings.externalIfd = true;
        NEXUS_CompositeInput_SetSettings(platformConfig.inputs.composite[0], &cvbsInputSettings);
        /* Connect CVBS 0 to VDEC */
        NEXUS_AnalogVideoDecoder_GetDefaultSettingsForVideoInput(
            NEXUS_CompositeInput_GetConnector(platformConfig.inputs.composite[0]), 
            &analogVideoDecoderSettings);
    }
    #elif CVBS_IF
    /* Connect CVBS 0 to VDEC */
    NEXUS_AnalogVideoDecoder_GetDefaultSettingsForVideoInput(
        NEXUS_CompositeInput_GetConnector(platformConfig.inputs.composite[0]), 
        &analogVideoDecoderSettings);
    #else
    NEXUS_AnalogVideoDecoder_GetDefaultSettingsForVideoInput(NEXUS_Frontend_GetAnalogVideoConnector(frontend), &analogVideoDecoderSettings);
    #endif
    {
        #if defined SECAM_L || defined SECAM || defined SECAM_L_PRIME
        analogVideoDecoderSettings.autoDetectionMode.autoMode = false;
        analogVideoDecoderSettings.autoDetectionMode.manualFormat = NEXUS_VideoFormat_eSecam;
        #else
        int i=0;
        analogVideoDecoderSettings.autoDetectionMode.autoMode = true;
        analogVideoDecoderSettings.autoDetectionMode.autoFormatTable[i++] = NEXUS_VideoFormat_eNtsc;
        analogVideoDecoderSettings.autoDetectionMode.autoFormatTable[i++] = NEXUS_VideoFormat_ePal;
        analogVideoDecoderSettings.autoDetectionMode.autoFormatTable[i++] = NEXUS_VideoFormat_eSecam;
        analogVideoDecoderSettings.autoDetectionMode.autoFormatTable[i++] = NEXUS_VideoFormat_eUnknown;
        #endif
    }

    NEXUS_AnalogVideoDecoder_SetSettings(analogVideoDecoder, &analogVideoDecoderSettings);

    /* Bring up audio */
    audioDecoder = NEXUS_RfAudioDecoder_Open(0, NULL);
    NEXUS_RfAudioDecoder_GetSettings(audioDecoder, &audioSettings);
    audioSettings.input = NEXUS_Frontend_GetAnalogAudioConnector(frontend);
    #if KOREA_A2
    audioSettings.mode = NEXUS_RfAudioDecoderMode_eKorea;
    #elif NICAM
    audioSettings.mode = NEXUS_RfAudioDecoderMode_eNicam;
    #elif SECAM_L || defined SECAM_L_PRIME
    audioSettings.mode = NEXUS_RfAudioDecoderMode_eNicamAm;
    #elif PAL_A2
    audioSettings.mode = NEXUS_RfAudioDecoderMode_ePalA2;
    #elif AASD
    audioSettings.mode = NEXUS_RfAudioDecoderMode_eAutoNicamPalA2;
    #else
    audioSettings.mode = NEXUS_RfAudioDecoderMode_eUs;
    #endif
    audioSettings.statusChangeCallback.callback = status_callback;
    audioSettings.statusChangeCallback.context = audioDecoder;
    audioSettings.statusChangeCallback.param = (int)frontend;
    NEXUS_RfAudioDecoder_SetSettings(audioDecoder, &audioSettings);
    NEXUS_AudioOutput_AddInput(NEXUS_AudioDac_GetConnector(platformConfig.outputs.audioDacs[0]),
                               NEXUS_RfAudioDecoder_GetConnector(audioDecoder));
    NEXUS_AudioOutput_AddInput(NEXUS_SpdifOutput_GetConnector(platformConfig.outputs.spdif[0]),
                               NEXUS_RfAudioDecoder_GetConnector(audioDecoder));
    NEXUS_RfAudioDecoder_Start(audioDecoder);

    done = 0;
    /* Control video and picture */
    while (!done) {
        printf("Select Control sets:\n");
        printf("\t1 > Enable deinterlacer\n");
        printf("\t2 > Enable ANR\n");
        printf("\t3 > Adjust brightness\n");
        printf("\t4 > Change Frequency\n");
        printf("\t5 > Change Channel\n");
        printf("\t6 > Change Tuning Mode [currently %s]\n", get_mode_string(mode));
        printf("\t7 > Get audio decoder status\n");
        printf("\t8 > Set audio decoder output mode\n");
        printf("\t9 > Restart RF audio decoder\n");
        printf("\t0 > Exit\n");
        cmd = get_number();

        switch (cmd)
        {
        case 1:
            NEXUS_VideoWindow_GetMadSettings(window, &madSettings);
            madSettings.deinterlace = true;
            NEXUS_VideoWindow_SetMadSettings(window, &madSettings);
            break;
        case 2:
            NEXUS_VideoWindow_GetAnrSettings(window, &anrSettings);
            anrSettings.anr.mode = NEXUS_VideoWindowFilterMode_eEnable;
            NEXUS_VideoWindow_SetAnrSettings(window, &anrSettings);
            break;
        case 3:
            printf("Brightness: ");
            NEXUS_VideoWindow_SetBrightness(window, get_number());
            break;
        case 4:
            printf("Frequency (Hz): ");
            cmd = get_number();
            frontendAnalogSettings.frequency = cmd;
            NEXUS_Frontend_TuneAnalog(frontend, &frontendAnalogSettings);
            NEXUS_AnalogVideoDecoder_ResetInput(analogVideoDecoder);
            break;
        case 5:
            printf("Enter channel number: ");
            cmd = get_number();
            frontendAnalogSettings.frequency = get_frequency(cmd, mode);
            if ( frontendAnalogSettings.frequency == 0 )
            {
                fprintf(stderr, "Invalid channel number %d for %s\n", cmd, get_mode_string(mode));
            }
            else
            {
                NEXUS_Frontend_TuneAnalog(frontend, &frontendAnalogSettings);
                NEXUS_AnalogVideoDecoder_ResetInput(analogVideoDecoder);
                printf("Tuned to channel %d (frequency = %d.%02d MHz)\n", cmd,
                       frontendAnalogSettings.frequency/1000000,
                       (frontendAnalogSettings.frequency%1000000)/10000);
            }
            break;
        case 6:
            printf("Enter new channel mode [0=%s, 1=%s, 2=%s, 3=%s]: ",
                   get_mode_string(0), get_mode_string(1), get_mode_string(2), get_mode_string(3));
            cmd = get_number();
            if ( cmd < 0 || cmd > 3 )
            {
                fprintf(stderr, "Invalid mode\n");
            }
            else
            {
                mode = cmd;
                printf("Mode set to %s\n", get_mode_string(mode));
                frontendAnalogSettings.terrestrial = (mode == TERRESTRIAL)?true:false;
            }
            break;
        case 7:
            {
                NEXUS_RfAudioDecoderStatus audioStatus;
                NEXUS_FrontendAnalogStatus analogStatus;
                NEXUS_RfAudioDecoder_GetStatus(audioDecoder, &audioStatus);
                printf("Audio Status - started %d stereo %d sap %d dual %d noise %d\n", audioStatus.started, audioStatus.stereoPresent, audioStatus.sapPresent, audioStatus.dualMonoPresent, audioStatus.noisePresent);
                NEXUS_Frontend_GetAnalogStatus(frontend, &analogStatus);
                printf("AV Ratio 1 0x%08x 0x%08x\n",
                       (unsigned int)(analogStatus.avRatio1>>32), (unsigned int)analogStatus.avRatio1);
                printf("AV Ratio 2 0x%08x 0x%08x\n",
                       (unsigned int)(analogStatus.avRatio2>>32), (unsigned int)analogStatus.avRatio2);
                #if AASD
                printf("Mode %d\n", audioStatus.currentMode);
                #endif
            }
            break;
        case 8:
            {
                NEXUS_RfAudioDecoder_GetSettings(audioDecoder, &audioSettings);
                #if KOREA_A2 || PAL_A2
                printf("A2 Audio mode - 0=Mono, 1=Stereo, 2=Sub, 3=Dual-Mono: ");
                cmd = get_number();
                audioSettings.modeSettings.korea.mode = cmd;
                audioSettings.modeSettings.palA2.mode = cmd;
                #elif NICAM || SECAM_L || defined SECAM_L_PRIME
                printf("NICAM Audio mode - 0=FmAm, 1=Nicam Mono 2=Stereo, 3=Dual1, 4=Dual2, 5=Dual12: ");
                cmd = get_number();
                audioSettings.modeSettings.nicam.mode = cmd;
                #elif AASD
                printf("A2 Audio mode - 0=Mono, 1=Stereo, 2=Sub, 3=Dual-Mono: ");
                cmd = get_number();
                audioSettings.modeSettings.autoNicamPalA2.palA2Mode = cmd;
                printf("NICAM Audio mode - 0=FmAm, 1=Nicam Mono 2=Stereo, 3=Dual1, 4=Dual2, 5=Dual12: ");
                cmd = get_number();
                audioSettings.modeSettings.autoNicamPalA2.nicamMode = cmd;
                #else
                printf("BTSC Audio mode - 0=Mono, 1=Stereo, 2=Sap, 3=Sap-Mono: ");
                cmd = get_number();
                audioSettings.modeSettings.us.mode = cmd;
                #endif
                NEXUS_RfAudioDecoder_SetSettings(audioDecoder, &audioSettings);
            }
            break;
        case 9:
            NEXUS_RfAudioDecoder_Stop(audioDecoder);
            NEXUS_RfAudioDecoder_Start(audioDecoder);
            break;
        default:
            continue;
        }
    }

    analogVideoDecoderSettings.input = NULL;
    NEXUS_AnalogVideoDecoder_SetSettings(analogVideoDecoder, &analogVideoDecoderSettings);
    NEXUS_RfAudioDecoder_Stop(audioDecoder);

    return 0;
}

