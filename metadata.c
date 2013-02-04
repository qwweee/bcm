#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char AC_CHAR8;
typedef unsigned int AC_UINT32;


typedef struct Ac_HnMetadataAttr{
	AC_CHAR8 *key;
	AC_CHAR8 *value;
} Ac_HnMetadataAttr;

typedef struct Ac_HnMetadata {
	AC_CHAR8 *key;                   /**< key of Metadata. Should include prefix even in the case of DIDL-Lite */
	AC_CHAR8 *value;                 /**< value of Metadata */
	struct Ac_HnMetadata *parent;    /**< pointer to parent node. if this Node is root, value will be NULL */
	AC_UINT32  child_cnt;            /**< cnt of children nodes */
	struct Ac_HnMetadata *childs;    /**< pointers to children nodes */
	AC_UINT32  attr_cnt;             /**< cnt of attributes */
	Ac_HnMetadataAttr *attrs;        /**< pointers to attributes -- key is not @ with start.   @id(x) id(0) */
	AC_UINT32  ns_cnt;               /**< cnt of namspaces for children. */
	Ac_HnMetadataAttr *ns;           /**< pointers to namspaces for children -- key is without \"xmlns:\" ex) altiApp not xmlns:altiApp */
	AC_CHAR8 *UUID;                  /**< uuid of this Metadata */
} Ac_HnMetadata;

void aci_freeMetadata(Ac_HnMetadata *pMeta, AC_UINT32 nMeta); 
#define SET_VALUE(x,y) x = y	
#define aci_free free

#if 0
static void setValue(AC_CHAR8** addr, AC_CHAR8* value) {
	int len = strlen(value) + 1;
	printf("len:%d\n", len);
	*addr = (AC_CHAR8*) malloc(len);
	memcpy(*addr, value, len);
	printf("%s\n", *addr);
}
#endif

int main() {
	int attr_count = 8;
	int child_count = 0;
	Ac_HnMetadata* meta;
	int i, j;

	meta = malloc(sizeof(Ac_HnMetadata));
	meta->key = (AC_CHAR8*) malloc(255);
	meta->value= (AC_CHAR8*) malloc(255);
	meta->UUID = (AC_CHAR8*) malloc(255);
	sprintf(meta->key, "res");
	sprintf(meta->value, "");
	sprintf(meta->UUID, "12"); 
	meta->child_cnt = child_count;
	meta->attr_cnt = attr_count;

	meta->attrs = (Ac_HnMetadataAttr*) malloc(sizeof(Ac_HnMetadataAttr) * attr_count);
	j = 0;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "protocolInfo");
	sprintf((meta->attrs+j)->value, "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO;DLNA.ORG_OP=11;DLNA.ORG_FLAGS=01700000000000000000000000000000;DLNA.ORG_PS=1/2,2,-2,4,-4,8,-8,16,-16,32,-32");
	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "profileID");
	sprintf((meta->attrs+j)->value, "AVC_TS_MP_HD_AC3_ISO");

	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "size");
	sprintf((meta->attrs+j)->value, "59682816");
	
	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "duration");
	sprintf((meta->attrs+j)->value, "0:01:00");

	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "bitrate");
	sprintf((meta->attrs+j)->value, "384000");

	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "resolution");
	sprintf((meta->attrs+j)->value, "720x480");

	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "sampleFrequency");
	sprintf((meta->attrs+j)->value, "48000");

	j++;
	(meta->attrs+j)->key = (AC_CHAR8*) malloc(255);
	(meta->attrs+j)->value = (AC_CHAR8*) malloc(255);
	sprintf((meta->attrs+j)->key, "nrAudioChannels");
	sprintf((meta->attrs+j)->value, "2");

	printf("key:%s, value:%s, child_cnt:%d, attr_cnt:%d\n",
			meta->key, meta->value, meta->child_cnt, meta->attr_cnt);
	for (i = 0; i < meta->attr_cnt ;i++) {
		printf("___attr key:%s, value:%s\n", 
				(meta->attrs + i)->key, (meta->attrs + i)->value);
	}

	aci_freeMetadata(meta, 1);
	return 0;
}

void aci_freeMetadata(Ac_HnMetadata *pMeta, AC_UINT32 nMeta) {
	int i = 0, n = 0;
    for (n = 0; n < nMeta; n++) {
		aci_free((pMeta + n)->key);
		aci_free((pMeta + n)->value);
		aci_free((pMeta + n)->UUID);
		printf("attr_cnt:%d\n", (pMeta + n)->attr_cnt);
        for (i = 0; i < (pMeta + n)->attr_cnt; i++) {
			printf("index:%d\n", i);
            //aci_free((((pMeta + n)->attrs)+i)->key);
            //aci_free((((pMeta + n)->attrs)+i)->value);
            aci_free(((pMeta + n)->attrs+i)->key);
            aci_free(((pMeta + n)->attrs+i)->value);
        }
        aci_free((pMeta + n)->attrs);
        aci_free(pMeta + n);
    }

}

#if 0
void aci_freeMetadata(Ac_HnMetadata *pMeta, AC_UINT32 nMeta) {
	int i = 0, n = 0;
    Ac_HnMetadata *meta;
    for (n = 0; n < nMeta; n++) {
        meta = pMeta + n;
		printf("key:%s, value:%s, child_cnt:%d, attr_cnt:%d\n",
				meta->key, meta->value, meta->child_cnt, meta->attr_cnt);
		for (i = 0; i < meta->attr_cnt ;i++) {
			printf("___attr key:%s, value:%s\n", 
					(meta->attrs + i)->key, (meta->attrs + i)->value);
		}
		printf("attr_cnt:%d\n", meta->attr_cnt);
        for (i = 0; i < meta->attr_cnt; i++) {
			printf("index:%d\n", i);
            aci_free((meta->attrs+i)->key);
            aci_free((meta->attrs+i)->value);
        }
        aci_free(meta->attrs);
        aci_free(meta);
    }

}
#endif
