#include <stdio.h>

FILE* fp = NULL;
int isMp3(FILE* file);
#define BUF_SIZE 1024
void loadFrameInfo();

int isMp3(FILE* file) {
	char buf[3];
	fread(buf, 1, 3, file);
	if (strcmp(buf,"ID3") == 0) {
		return 1;
	}
	return -1;
}


typedef struct _FrameInfo {
	char* id;
	char* desc;
	unsigned int size;
	unsigned int flags;
} MP3_FrameInfo;

FrameInfo frames[5];

void printFrame(FrameInfo* frame) {
	printf("id:%s\n", frame->id);
	printf("size:%d\n", frame->size);
	printf("flag:%02x\n", frame->flags);
}

unsigned int btoi(char* src, int size) {
	unsigned long value = 0;
	int i;
	for (i = 0;i < size;i++) {
		value |= src[i] & 0xff;
		value = value << 8;
	}
	value = value >> 8;
	return (unsigned int) value;
}

unsigned int getTagLength(char* src) {
	unsigned long value = 0;
	int i;
	for (i = 0;i < 4;i++) {
		value |= src[i] & 0x6f;
		value = value << 7;
	}
	value = value >> 7;
	printf("tag length:%d\n", (unsigned int)value);
	return (unsigned int) value;
}

void parseInfo(FILE* file) {
	int HEADER_SIZE = 10;
	char header[10];
	char tag_size[4];
	int tag_length;
	int i;
	/* jump to frame start position */
	fseek(file, 6, SEEK_SET);
	fread(tag_size, 1, 4, file);
	tag_length = getTagLength(tag_size) + HEADER_SIZE;
	while (ftell(file) < tag_length) {
		FrameInfo frame;
		frame.size = 0;
		frame.flags = 0;
		char size[4];
		char flags[2];
		fread(header, 1, HEADER_SIZE, file);
		printf("%02x %02x %02x %02x\n", header[0],
				header[1], header[2], header[3],
				header[4], header[5], header[6],
				header[7], header[8], header[9]);
		frame.id = malloc(4);
		memcpy(frame.id, header, 4);
		memcpy(size, header+4, 4);
		memcpy(flags, header+8, 2);
		printf("size:%02x %02x %02x %02x\n", size[0], size[1], size[2], (size[3] & 0xff));
		frame.size = btoi(size, 4);
		frame.flags = btoi(flags, 2);
		printFrame(&frame);
		{
			char content[frame.size];
			char outputname[255];
			fread(content, 1, frame.size, file);
			printf("%s\n", content);
			/*
			<Header for 'Attached picture', ID: "APIC">
			Text encoding  $xx
			MIME type      <text string> $00
			Picture type    $xx
			Description    <text string according to encoding> $00 (00)
			Picture data    <binary data>
			   */
			if (strcmp(frame.id, "APIC") == 0) {
				printf("save image file: %02x %02x %02x\n", content[0],
						content[1], content[2]);
				char* pch = strchr(content + 1, 0x00);
				printf("-->%d\n", (pch-content));
				char* ext = strstr(content + 1, "image");
				printf("-->%d\n", (pch - ext));
				memcpy(outputname, ext, (pch - ext) + 1);
				char* temp = strchr(outputname, '/');
				*temp = '.';
				//sprintf(outputname, "image.%s", outputname);
				printf("-->outputname:%s\n", outputname);
				pch = strchr(pch+1, 0x00) + 1;
				printf("-->%d\n", (pch-content));
				FILE* output = fopen(outputname, "wb");
				if (output == NULL) {
					printf("failed to open output file\n");
				}
				fwrite(pch, 1, frame.size - (pch - content), output);
				fclose(output);
			} else {
			}
		}
		free(frame.id);
	}

}
/**
  * TIT2 : title
  * TPE1 : Lead performer
  * TALB : Album/Movide/Show title
  * TYER : Year
  */
void getSimpleFrame(char* id, int size, int flags) {
}

/**
  * APIC : Attached picture
  */
void getComplexFrame(char* id) {
}

int main(int argc, char* argv[]) {
	char* filename;
	if (argc < 2) {
		printf("Usage: filetype filename\n");
		return -1;
	}
	filename = argv[1];
	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		printf("failed to open %s\n", argv[1]);
		return -2;
	}

	if (isMp3(fp) > 0) {
		printf("%s is mp3 file \n", filename);
		parseInfo(fp);
	} else {
		printf("%s is not mp3 file \n", filename);
	}
	fclose(fp);
	return 0;
}

void loadFrameInfo() {
	int i = 0;
	frames[i].id = "IPLS";
	frames[i].desc = "Involved people list";
	i++;
	frames[i].id = "MCDI";
	frames[i].desc = "Music CD identifier";
	i++;
	frames[i].id = "PCNT";
	frames[i].desc = "Player counter";
	i++;
	frames[i].id = "TALB";
	frames[i].desc = "Album/Movie/Show title";
	i++;
	frames[i].id = "TBPM";
	frames[i].desc = "BPM(beats per minute)";
	i++;
	frames[i].id = "TRCK";
	frames[i].desc = "BPM(beats per minute)";
	i++;
}
