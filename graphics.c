#include "nexus_platform.h"
#include "nexus_surface.h"
#include "nexus_graphics2d.h"
#include "nexus_display.h"
#include "nexus_core_utils.h"

#include "bstd.h"
#include "bkni.h"
#include <stdlib.h>
#include <stdio.h>

void complete(void *data, int unused)
{
}


int main(void) {
	NEXUS_SurfaceHandle surface;
	NEXUS_SurfaceCreateSettings createSettings;
	NEXUS_DisplayHandle display;
	NEXUS_DisplaySettings displaySettings;
	NEXUS_GraphicsSettings graphicsSettings;
	NEXUS_Graphics2DHandle gfx;
	NEXUS_Graphics2DFillSettings fillSettings;
	NEXUS_Graphics2DSettings gfxSettings;
	NEXUS_PlatformSettings platformSettings;
	NEXUS_PlatformConfiguration platformConfig;
	NEXUS_VideoFormatInfo info;
	BKNI_EventHandle checkpointEvent, spaceAvailableEvent;

	NEXUS_Platform_GetDefaultSettings(&platformSettings);
	platformSettings.openFrontend = false;
	NEXUS_Platform_Init(&platformSettings);
	NEXUS_Platform_GetConfiguration(&platformConfig);

	NEXUS_Display_GetDefaultSettings(&displaySettings);
	displaySettings.displayType = NEXUS_DisplayType_eAuto;
	displaySettings.format = NEXUS_VideoFormat_eNtsc;
	display = NEXUS_Display_Open(0, &displaySettings);

	NEXUS_Display_AddOutput(display, NEXUS_CompositeOutput_GetConnector(platformConfig.outputs.composite[0]));

	NEXUS_Display_AddOutput(display, NEXUS_HdmiOutput_GetVideoConnector(platformConfig.outputs.hdmi[0]));
	NEXUS_VideoFormat_GetInfo(displaySettings.format, &info);

	NEXUS_Surface_GetDefaultCreateSettings(&createSettings);
	createSettings.pixelFormat = NEXUS_PixelFormat_eA8_R8_G8_B8;
	createSettings.width = info.width;
	createSettings.height = info.height;
	createSettings.heap = NEXUS_Platform_GetFramebufferHeap(0);
	surface = NEXUS_Surface_Create(&createSettings);

	gfx = NEXUS_Graphics2D_Open(0, NULL);

	BKNI_CreateEvent(&checkpointEvent);
	BKNI_CreateEvent(&spaceAvailableEvent);

	/*
	NEXUS_Graphics2D_GetSettings(gfx, &gfxSettings);
	gfxSettings.checkpointCallback.callback = complete;
	gfxSettings.checkpointCallback.context = checkpointEvent;
	gfxSettings.packetSpaceAvailable.callback = complete;
	gfxSettings.packetSpaceAvailable.context = spaceAvailableEvent;
	NEXUS_Graphics2D_SetSettings(gfx, &gfxSettings);
	*/

	NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);
	fillSettings.surface = surface;
	fillSettings.rect.x = 10;
	fillSettings.rect.y = 10;
	fillSettings.rect.width = 300;//createSettings.width;
	fillSettings.rect.height = 200;// createSettings.height;
	//fillSettings.colorOp = NEXUS_FillOp_eCopy;
	//fillSettings.alphaOp = NEXUS_FillOp_eCopy;
	//fillSettings.color = (rand()%0xFFFFFF) | (0xFF<<24);
	fillSettings.color = 0xFFFFFFFF;
	NEXUS_Graphics2D_Fill(gfx, &fillSettings);

	NEXUS_Graphics2D_Checkpoint(gfx, NULL);

	NEXUS_Display_GetGraphicsSettings(display, &graphicsSettings);
	graphicsSettings.enabled = true;
	NEXUS_Display_SetGraphicsSettings(display, &graphicsSettings);
	NEXUS_Display_SetGraphicsFramebuffer(display, surface);

	printf("starting\n");
	//NEXUS_Graphics2D_GetDefaultFillSettings(&fillSettings);

	getchar();
	return 0;

}
