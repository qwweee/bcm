#include <stdio.h>
#include <string.h>
typedef char AC_CHAR8;

#define AC_TRUE 1
#define AC_FALSE 0


typedef struct _Location_Info {
	char path[255];
	char index_path[255];
	char ts_path[255];
} Location_Info;

static void convertUrl(AC_CHAR8* url, Location_Info* conUrl);
static void convertPath(AC_CHAR8* url, Location_Info* loc);
static void convertPath1(AC_CHAR8* url, Location_Info* loc);
static int isFileContent(AC_CHAR8* url); 
AC_CHAR8 *aci_st_getRealPath(AC_CHAR8 *path); 


static void test1() {
	char str[] = "ocap:/recorded_service/1801.64./media0_0/1220506712793";
	Location_Info loc;
	convertUrl(str, &loc);
	printf("---> path:%s\n", loc.path);
	printf("---> index name:%s\n", loc.index_path);
	printf("---> ts name:%s\n", loc.ts_path);
}

static void test2() {
	char str[] = "http://10.1.1.9:38520/ExportContentDiscovery10.ts?id=9";
	Location_Info loc;
	convertPath(str, &loc);
}

static void test3() {
	char str[] = "http://10.1.1.9:38520/ExportContentDiscovery10.ts?id=9";
	printf("isFileContent:%d\n" ,isFileContent(str));
}

static void test4() {
	char str[] = "ExportContent?id=7&url=/gem_storage/1/yn_2.jpg";
	Location_Info loc;
	convertPath1(str, &loc);
}

static void test5() {
	char str[] = "test.jpg";
	if (strstr(str, ".jpg")) {
		printf("found jpg");
	} else {
		printf("not found jpg");
	}
}

void test6() {
	char str[]="ocap:/recorded_service/c01.1./media0_0/8135531";
	if (strstr(str, "ocap:")) {
		printf("found ocap");
	} else {
		printf("not found ocap");
	}

}

void test7() {
	char str[]="mms://10.1.1.4:80/Cats.avi";
	char realUrl[255];
	sprintf(realUrl, "http%s", str + 3);
	printf("realUrl:%s\n", realUrl);
}


/** remove additional parameters */
static void test8() {
	char str[] = "/ExportContent?id=1&url=ocap:/recorded_service/c01.1./media0_0/2139238&WMContentFeatures=DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO;DLNA.ORG_OP=11;DLNA.ORG_FLAGS=01700000000000000000000000000000;DLNA.ORG_PS=1/2,2,-2,4,-4,8,-8,16,-16,32,-32&WMHME=1";
	Location_Info loc;
	convertUrl(str, &loc);
	printf("---> path:%s\n", loc.path);
	printf("---> index name:%s\n", loc.index_path);
	printf("---> ts name:%s\n", loc.ts_path);
}
int main() {
	test1();
	test8();
	return 0;
}

static int isFileContent(char* url) {
	char* start = NULL;
	start = strstr(url, "ocap:");
	if (start != NULL) {
		printf(" found\n");
		return 1;
	}
	printf("not found \n");
	return 0;
}


static void convertUrl(AC_CHAR8* realUrl, Location_Info* loc) {
	char *pch;
	char index_name[255];
	char ts_name[255];
	char url[255];
	memset(url, 0, 255);
	memset(index_name, 0, 255);
	memset(ts_name, 0, 255);
	if (strstr(realUrl, "&WMContent")) {
		char *start = realUrl;
		char *end = strstr(realUrl, "&WMContent");
		memcpy(url, realUrl, (end - start));
	} else {
		memcpy(url,realUrl,strlen(realUrl));
	}
	printf("url:%s\n", url);

	pch = strstr(url, "/media0_0");
	sprintf(loc->path, "/data%s", pch);
	sprintf(index_name, "%s.nav", pch + 10);
	sprintf(ts_name, "%s.ts", pch + 10);
	sprintf(loc->index_path, "%s/%s", loc->path, index_name);
	sprintf(loc->ts_path, "%s/%s", loc->path, ts_name);
}

/*
http://10.1.1.9:38520/ExportContentDiscovery10.ts?id=9
   */
static void convertPath(AC_CHAR8* url, Location_Info* loc) {
	char keyStr[] = "ExportContent";
	char* start;
	char* end;
	char temp[255];

	start = strstr(url, keyStr) + strlen(keyStr);
	end = strchr(start, '?');
	memcpy(temp, start, (end - start));
	printf("filename:%s\n", temp); 


}

static void convertPath1(AC_CHAR8* url, Location_Info* loc) {
	char keyStr[] = "url=";
	char* start;
	char* end;
	char temp[255];
	int len;

	start = strstr(url, keyStr) + strlen(keyStr);
	printf("->len:%d\n", strlen(start));
	memcpy(temp, start, strlen(start));
	printf("filename:%s\n", temp); 


}

