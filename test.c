#include <stdio.h>
#include "bstd.h"
#include "nexus_types.h"
#include "nexus_platform.h"
#include "nexus_security_init.h"

int main(int argc, char* argv[]) {
	NEXUS_Error ret;
	NEXUS_PlatformSettings platformStatus;
	char versionString[256];

	printf("Ready to initialize\n");
	printf("struct size:%d\n", NEXUS_P_GET_STRUCT_SIZES());
	printf("struct size:%d\n", sizeof(NEXUS_PlatformSettings));
	printf("struct size:%d\n", sizeof(NEXUS_PlatformConfiguration));
//	printf("size of NEXUS_TranscoderModulePlatformSettings:%d\n",
//			sizeof(NEXUS_TranscoderModulePlatformSettings));
	printf("size of NEXUS_SecurityModuleSettings:%d\n",
			sizeof(NEXUS_SecurityModuleSettings));

	ret = NEXUS_Platform_Init(NULL);
	if ( ret ) {
		printf("Unable to initialize nexus:%d\n", ret);
		return -1;
	}

	ret = NEXUS_Platform_GetStatus(&platformStatus);
	NEXUS_Platform_GetReleaseVersion(versionString, 256);

	NEXUS_Platform_Uninit();

	printf("Uninit complete. \n");
	return 0;
}
