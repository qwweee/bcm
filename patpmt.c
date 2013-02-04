#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE (1024*1024)

static void makePat(char* section, int pmtPid);
static void makePmt(char* section, int pmtPid, int vPid, int aPid); 

FILE *outputfile;
int count;
int main(void) {
	char pat[188];
	char pmt[188];
	int i;
	outputfile = (FILE*) fopen("output.ts", "w");
	if (outputfile == NULL) {
		printf("fail to open file\n");
		return -1;
	}
	makePat(pat, 111); 
	makePmt(pmt, 111, 289, 290);
	for (i = 0 ; i < 200; i++) {
		fwrite(pat, 188 ,1, outputfile);
		fwrite(pmt, 188 ,1, outputfile);
	}
	fclose(outputfile);

}

int makeCrc32(int crc, char* data, int length) {
	int j;
	crc = ~crc;
	while (length--) {
		for (j=0; j < 8; j++) 
			crc = (crc<<1) ^ ((((*data >> (7-j)) ^ (crc >> 31)) & 1) ?
					0x04c11db7 : 0);
		data++;
	}
	return crc;
}

static void makePat(char* section, int pid) {
	char *pat, *length, *start, *end = section;
	int crc = 0;
	memset(section , 0xff, 188);	
	*end++ = 0x47;
	*end++ = 0x40;
	*end++ = 0x00;
	*end++ = 0x10;
	*end++ = 0x00;
	pat = end;
	*end++ = 0x00; /*table_id=0, PAT*/
	*end++ = 0xB0;
	length = end;
	end++;
	start = end;
	*end++ = 0x00;
	*end++ = 0x01;
	*end++ = 0xc1; /* version number */
	*end++ = 0x00;
	*end++ = 0x00;

	*end++ = 0x00; /* program number */
	*end++ = 0x01;

	*end++ = 0xe0 | ((pid >> 8) & 0x1f);
	*end++ = (pid & 0xff);

	*length = 4 + (int)end - (int)start;
	crc = (makeCrc32(0, pat, (int)end - (int)pat));
	*end++ = crc >> 24;
	*end++ = crc >> 16;
	*end++ = crc >> 8;
	*end++ = crc >> 0;
}

static void makePmt(char* section, int pmtPid, int vPid, int aPid) {
	char *pmt, *length, *start, *end = section;
	int crc = 0;
	int i;

	memset(section , 0xff, 188);
	*end++ = 0x47;
	*end++ = 0x40 | ((pmtPid >> 8) & 0x1f);
	*end++ = (pmtPid & 0xff);
	*end++ = 0x10;
	*end++ = 0x00;
	
	pmt = end;
	*end++ = 0x02;   /* tabld_id=2, PMT */
	*end++ = 0xB0;
	
	length = end;
	end++;
	start = end;

	*end++ = 0x00; /* program number */
	*end++ = 0x01;

	*end++ = 0xc1; /* version number */

	*end++ = 0x00; /* section number start */
	*end++ = 0x00; /* last section number */

	*end++ = (vPid >> 8) & 0x01; /* PCR PID */
	*end++ = vPid & 0xff;

	*end++ = 0xf0;
	*end++ = 0x00; /* program info length */

	/* video info */
	*end++ = 0x1b;  /* H.264 */
	*end++ = 0xe0 | ((vPid >> 8) & 0x1f);
	*end++ = vPid & 0xff;
	*end++ = 0xf0;
	*end++ = 0x00;

	/* audio info */
	*end++ = 0x81; /* ATSC AC3 audio */
	*end++ = 0xe0 | ((aPid >> 8) & 0x1f);
	*end++ = aPid & 0xff;
	*end++ = 0xf0;
	*end++ = 0x00;

	*length = 4 + (int)end - (int)start;
	crc = makeCrc32(0, pmt, (int)end - (int)pmt);

	*end++ = crc >> 24;
	*end++ = crc >> 16;
	*end++ = crc >>  8;
	*end++ = crc >>  0;
}
