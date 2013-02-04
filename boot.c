#include <stdio.h>
#include "nexus_platform.h"
#include "nexus_display.h"
#include "nexus_surface.h"

//extern void SetSignalMask();

static int _initDisplay();
int main(int argc, char* argv[]) {
	NEXUS_Error ret;
	NEXUS_PlatformSettings platformSettings;


	printf("before GetDefaultSettings\n");
	//NEXUS_Platform_GetDefaultSettings(&platformSettings);

	printf("after GetDefaultSettings\n");
	ret = NEXUS_Platform_Init(NULL);
	printf("ret = %d\n", ret);


	_initDisplay();

	printf("hello world111\n");
	return 0;
}

static int _initDisplay() {
	NEXUS_DisplaySettings displaySettings0;

	NEXUS_Display_GetDefaultSettings(&displaySettings0);
}


