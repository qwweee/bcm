#include <bstd.h>
#include <stdio.h>
#include "nexus_platform.h"
#include "nexus_base_types.h"
#include "nexus_ir_input.h"

static NEXUS_Callback user_callback(void *context, int iParam) {
	size_t numEvents = 1;
	NEXUS_Error rc = 0;
	bool overflow;
	NEXUS_IrInputHandle irHandle = *(NEXUS_IrInputHandle *)context;
	printf("user_callback\n");
	while (numEvents && !rc) {
		NEXUS_IrInputEvent irEvent;
		rc = NEXUS_IrInput_GetEvents(irHandle, &irEvent,11,&numEvents,&overflow);
		if (numEvents) {
			printf("irCallback:rc: %d, code:%08x, repeat:%s\n", rc, irEvent.code, irEvent.repeat ? "true" : "false");
		}

	}
	//((MIoDevice *)context)->trigger();
}

int main(int argc, char* argv[]) {

	NEXUS_IrInputSettings settings;
	NEXUS_IrInputHandle handle = NULL;
	NEXUS_Error ret;
	NEXUS_PlatformSettings platformSettings;

	ret = NEXUS_Platform_Init(NULL);
	if (ret) {
		printf("error failed to NEXUS platform initialize\n");
		return 0;
	}
	NEXUS_IrInput_GetDefaultSettings(&settings);
	settings.mode = NEXUS_IrInputMode_eCirNec;
	settings.channel_number = 0;
	settings.dataReady.callback = (NEXUS_Callback)user_callback;
	settings.dataReady.context = &handle;
	handle = NEXUS_IrInput_Open(0, &settings);

	if (handle == NULL) {
		printf("error in create Remote Input\n");
		return 0;
	}
	printf("start waiting IR event\n");
	while (1) {
//		printf(".\n");
		sleep(1);
	}


	printf("hello world111\n");
	return 0;
}



