#include <stdio.h>
#include "bcmindexer.h"
#define BNAV_get_version( pEntry ) \
	        ((unsigned char)((pEntry)->words[1] & 0xf0)>>4)

int main(int argc, char **argv) {
	FILE *file;
	BNAV_Entry entry;
	BNAV_Version ver;
	file = fopen("/data/media0_0/110197/110197.nav", "rb");
	if (!file) {
		fprintf(stderr, "Cannot open %s\n");
		exit(1);
	}
	fseek(file, 0, SEEK_SET);
	fread(&entry, sizeof(entry), 1, file);
	ver = BNAV_get_version(&entry);
	printf("navVersion:%d\n", ver); 
	return 0;
}


