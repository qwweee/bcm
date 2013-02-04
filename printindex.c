#include "bstd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bcmindexer.h"
#include "bcmindexerpriv.h"
#include "tsindexer.h"
#include "tsplayer.h"
#include "mpeg2types.h"
#include "avstypes.h"
#include "bcmindexer_svc.h"

void printHeader() {
    printf(
    "printindex, Rev. 1.3\n"
    "Broadcom Corp. Confidential\n"
    "Copyright 2002-2005 Broadcom Corp. All Rights Reserved.\n"
    "\n"
    );
}

#define DEFAULT_OPTIONSTRING "index,lo,size,rfo,type,pts"

void printUsage() {
    printHeader();
    printf(
    "Usage: printindex [options] [filename]\n\n"
    "  If compiled with 64-bit support, this app will read indexes for mpeg >2GB.\n"
    "  If filename is blank or \"-\", stdin is used.\n\n"
    );
    printf(
    "  Options:\n"
    "  -d              Print offset and size in decimal (default is hex)\n"
    "  -i <index>      Index number to start at. Default is 0.\n"
    "  -n <number>     Number of indexes to print. Default is -1, which means to EOF.\n"
    "  -sct            The index is an sct index, not a bcm index. This is defaulted\n"
    "                  if the filename doesn't have a .nav or .bcm suffix.\n"
    );
    printf(
    "  -sct6           The index is a six word sct index.\n"
    "                  if the filename has a .nav or .bcm suffix.\n"
    "  -bcm            The index is a broadcom index\n"
    );
    printf(
    "  -avs            The scts are for AVS streams\n"
    );
    printf(
    "  -skippts        Don't print pts entries in an sct index\n"
    "  -s              Silence various comments about defaults.\n"
    "  -o OPTIONS      Specify bcmindex fields. Fields include:\n"
    "                    index     index number of each index entry\n"
    "                    seqhdr    seqhdr offset and size\n"
    "                    hi        hi 32 bits of frame offset\n"
    "                    lo        lo 32 bits of frame offset\n"
    "                    pts       presentation time stamp\n"
    );
    printf(
    "                    size      size of the frame\n"
    "                    vchip     16 bits of vchip content rating\n"
    "                    ts        continuous time stamp\n"
    "                    rfo       reference frame offset\n"
    "                    type      frame type (I,B,P,reference P)\n"
    "                  Default is " DEFAULT_OPTIONSTRING "\n"
    );
}

/* lifted from bcmindexer.c */
static unsigned long bcmindex_returnPts(const sIndexEntry *p_sct)
{
    unsigned long pts;
    /* Calculate PTS[32:1] */
    pts = p_sct->startCodeBytes & 0x1;
    pts = (pts << 31) + (p_sct->recordByteCount >> 1);
    return pts;
}

static unsigned long bcmindex_returnPts2(const sSixWordIndexEntry *p_sct)
{
#if 0
    unsigned long pts;
    /* Calculate PTS[32:1] */
    pts = p_sct->startCodeBytes & 0x1;
    pts = (pts << 31) + (p_sct->recordByteCount >> 1);
    return pts;
#else /* for RAVE based platforms */
    return p_sct->recordByteCountHi;
#endif
}

typedef enum {
    b_indextype_sct,
    b_indextype_sct6,
    b_indextype_bcm,
    b_indextype_avcbcm,
    b_indextype_vc1bcm,
    b_indextype_avsbcm,
    b_indextype_timestamp_only,
    b_indextype_avc_extended,
    b_indextype_unknown
} b_indextype;

/* options */
static char *optionstring = DEFAULT_OPTIONSTRING;
static int decimal = 0;
static int skippts = 0;
static b_indextype indextype = b_indextype_unknown;

void print_naventry(int i, const BNAV_Entry *entry)
{
#define FIND(STR) strstr(optionstring, STR)
    if (FIND("index"))
        printf("%d: ", i);

    if (FIND("Seqhdr")) { /* use capital S and get absolute offset, 32 bit addresses only */
        printf("seqhdr %#08lx(%#x) ",
            BNAV_get_frameOffsetLo(entry) - BNAV_get_seqHdrStartOffset(entry), BNAV_get_seqHdrSize(entry));
        if (indextype == b_indextype_avcbcm) {
            printf("sps %#08lx(%#lx) ",
                BNAV_get_frameOffsetLo(entry) - BNAV_get_SPS_Offset((const BNAV_AVC_Entry*)entry), BNAV_get_SPS_Size((const BNAV_AVC_Entry*)entry));
        }
        else if (indextype == b_indextype_vc1bcm) {
            printf("EP %#08lx(%#lx) ",
                BNAV_get_frameOffsetLo(entry) - BNAV_get_SPS_Offset((const BNAV_AVC_Entry*)entry), BNAV_get_SPS_Size((const BNAV_AVC_Entry*)entry));
        }
    }

    if (FIND("seqhdr")) {
        printf("seqhdr %#08lx(%#x) ",
            BNAV_get_seqHdrStartOffset(entry), BNAV_get_seqHdrSize(entry));
        if (indextype == b_indextype_avcbcm) {
            printf("sps %#08lx(%#lx) ",
                BNAV_get_SPS_Offset((const BNAV_AVC_Entry*)entry), BNAV_get_SPS_Size((const BNAV_AVC_Entry*)entry));
        }
        else if (indextype == b_indextype_vc1bcm) {
            printf("EP %#08lx(%#lx) ",
                BNAV_get_SPS_Offset((const BNAV_AVC_Entry*)entry), BNAV_get_SPS_Size((const BNAV_AVC_Entry*)entry));
        }
    }
    if (FIND("hi")) {
        if (decimal)
            printf("hi=%-8ld ", (unsigned long)entry->words[2]);
        else
            printf("hi=0x%08x ", entry->words[2]);
    }
    if (FIND("lo")) {
        if (decimal)
            printf("lo=%-8ld ", (unsigned long)entry->words[3]);
        else
            printf("lo=0x%08x ", entry->words[3]);
    }
    if (FIND("pts"))
        printf("pts=0x%08x ", entry->words[4]);
    if (FIND("size")) {
        if (decimal)
            printf("size=%-8ld ", BNAV_get_frameSize(entry));
        else
            printf("size=0x%08lx ", BNAV_get_frameSize(entry));
    }
    if (FIND("vchip"))
        printf("vchip=0x%04x ", BNAV_unpack_vchip(BNAV_get_packed_vchip(entry)));
    if (FIND("ts")) {
        if (decimal)
            printf("ts=%d ", entry->words[6]);
        else
            printf("ts=0x%08x ", entry->words[6]);
    }
    if (FIND("rfo"))
        printf("rfo=%2d ", BNAV_get_refFrameOffset(entry));
    if (FIND("type"))
        printf("(%s) ", BNAV_frameTypeStr[BNAV_get_frameType(entry)]);

    if (FIND("rai"))
        printf("rai=%s ", BNAV_get_RandomAccessIndicator(entry)?"true":"false");

    printf("\n");
}

static void print_svcentry(int i, const BNAV_SVC_Entry *entry)
{
    if (FIND("index"))
        printf("%d: ", i);

    if (decimal)
        printf("pid=%u", entry->header.pid);
    else
        printf("pid=%#x ", entry->header.pid);
    
    if (FIND("hi") || FIND("lo")) {
       if (decimal)
            printf("offset=%-8u) ", (unsigned)(entry->common.offset));
        else
            printf("offset=0x%08x ", (unsigned)(entry->common.offset));
    }

    if (FIND("size")) {
       if (decimal)
            printf("size=%-8d ", (int)entry->common.payload_size);
        else
            printf("size=0x%08x ", (unsigned)entry->common.payload_size);
    }

    
    switch(entry->header.type) {
    case BNAV_SVC_Entry_Type_eExtensionFrame:
    case BNAV_SVC_Entry_Type_eFrame:
        printf("%sframe ",entry->header.type==BNAV_SVC_Entry_Type_eExtensionFrame?"extension":"");
        if (FIND("pts")) {
            printf("pts=0x%08x ", entry->data.frame.pts);
        }
        if (FIND("ts")) {
            if (decimal)
                printf("ts=%d ", entry->data.frame.timestamp);
            else
                printf("ts=0x%08x ", entry->data.frame.timestamp);
        }
    default:
        printf("type=%u", entry->data.frame.frame_type);
        BDBG_ASSERT(0);
    }
    printf("\n");
    return;
}

void print_sct(int i, const BSCT_Entry *entry)
{
    int pic, sc;
    sc = entry->startCodeBytes >> 24;
    if (sc == SC_PTS) {
        if (!skippts)
        printf("%03d: PTS=%#lx\n", i, bcmindex_returnPts(entry));
    }
    else {
        pic = sc == SC_PICTURE ? returnPictureCode(entry->startCodeBytes) : 0;
        printf("%03d: SC=%02lx payload=%04lx offset=%02lx offset=0x%02lx%08lx (%s)\n",
            i,
            (unsigned long)entry->startCodeBytes>>24,
            (unsigned long)(entry->startCodeBytes>>8) & 0xFFFF,
            (unsigned long)entry->startCodeBytes&0xFF,
            (unsigned long)entry->recordByteCountHi >> 24,
            (unsigned long)entry->recordByteCount,
            pic==PC_I_FRAME?"I":pic==PC_P_FRAME?"P":pic==PC_B_FRAME?"B":"-");
    }
}


 
/* 
Summary:
Returns bits 'e'..'b' from word 'w', 

Example:
    B_GET_BITS(0xDE,7,4)==0x0D
    B_GET_BITS(0xDE,3,0)=0x0E 
*/
#define B_GET_BITS(w,e,b)  (((w)>>(b))&(((unsigned)(-1))>>((sizeof(unsigned))*8-(e+1-b))))

static void print_sct6(int i, const BSCT_SixWord_Entry *entry)
{
    int pic, sc,entry_type;
    entry_type = entry->word0 >> 24;

    if(entry_type == 0x80) {
        printf("%03d: TS payload=%06lx%08lx%08lx%08lx%08lx%08lx\n", i, (unsigned long)(entry->word0 & 0x00FFFFFF), (unsigned long)entry->word1, (unsigned long)entry->startCodeBytes, (unsigned long)entry->recordByteCount, (unsigned long)entry->recordByteCountHi, (unsigned long)entry->flags);
        return;
    }
    printf("%03d: pid:0x%04x ",  i, (unsigned)B_GET_BITS(entry->word0, 20, 8));

    if (entry_type == 0x01)
    {
        printf("TPIT entry: %08x\n", entry->startCodeBytes);
    }
    else if(entry_type == 0x07) {
         printf("extra payload=                %08x\n", entry->startCodeBytes);
    }
    else
    {
        sc = entry->startCodeBytes >> 24;
        if (sc == SC_PTS)
        {
            if (!skippts)
                printf("PTS=%#lx DTS=%#x\n", bcmindex_returnPts2(entry), entry->recordByteCountHi);
        }
        else
        {
            pic = sc == SC_PICTURE ? returnPictureCode(entry->startCodeBytes) : 0;
            printf("SC=%02lx payload=%04lx%06lx%06lx offset=%02lx offset=0x%02lx%08lx (%s)\n",
                   (unsigned long)entry->startCodeBytes>>24,
                   (unsigned long)(entry->startCodeBytes>>8) & 0xFFFF,
                   (unsigned long)entry->recordByteCountHi & 0xFFFFFF,
                   (unsigned long)entry->flags & 0xFFFFFF,
                   (unsigned long)entry->startCodeBytes&0xFF,
                   (unsigned long)entry->recordByteCountHi >> 24,
                   (unsigned long)entry->recordByteCount,
                   pic==PC_I_FRAME?"I":pic==PC_P_FRAME?"P":pic==PC_B_FRAME?"B":"-");
        }
    }

}

void print_avs_sct6(int i, const BSCT_SixWord_Entry *entry)
{
    int pic, sc,entry_type;
    entry_type = entry->word0 >> 24;
    if (entry_type == 0x01)
    {
        printf("%03d: TPIT entry\n",i);
    }
    else
    {
        sc = entry->startCodeBytes >> 24;
        if (sc == SC_AVS_PTS)
        {
            if (!skippts)
                printf("%03d: PTS=%#lx\n", i, (long unsigned int)entry->recordByteCountHi);
        }
        else if(sc == SC_AVS_PICTURE_I)
        {
            printf("%03d: SC=%02lx payload=%04lx%06lx%06lx offset=%02lx offset=0x%02lx%08lx (%s)\n",
                   i,
                   (unsigned long)entry->startCodeBytes>>24,
                   (unsigned long)(entry->startCodeBytes>>8) & 0xFFFF,
                   (unsigned long)entry->recordByteCountHi & 0xFFFFFF,
                   (unsigned long)entry->flags & 0xFFFFFF,
                   (unsigned long)entry->startCodeBytes&0xFF,
                   (unsigned long)entry->recordByteCountHi >> 24,
                   (unsigned long)entry->recordByteCount,
                    "I");
        }
        else
        {
            pic = sc == SC_AVS_PICTURE_PB ? returnAvsPictureCode(entry->recordByteCountHi) : 0;
            printf("%03d: SC=%02lx payload=%04lx%06lx%06lx offset=%02lx offset=0x%02lx%08lx (%s)\n",
                   i,
                   (unsigned long)entry->startCodeBytes>>24,
                   (unsigned long)(entry->startCodeBytes>>8) & 0xFFFF,
                   (unsigned long)entry->recordByteCountHi & 0xFFFFFF,
                   (unsigned long)entry->flags & 0xFFFFFF,
                   (unsigned long)entry->startCodeBytes&0xFF,
                   (unsigned long)entry->recordByteCountHi >> 24,
                   (unsigned long)entry->recordByteCount,
                   pic==PC_AVS_P_FRAME?"P":pic==PC_AVS_B_FRAME?"B":"-");
        }
    }

}

int main(int argc, char **argv) {
    int i;
    int offset = 0;
    int total = -1;
    int silence = 0;
    const char *filename = NULL;
    FILE *file;
    unsigned entrysize = 0;
    int sctIsAvs =0;

    for (i=1;i<argc;i++) {
        if (!strcmp(argv[i], "--help")) {
            printUsage();
            exit(0);
        }
        else if (!strcmp(argv[i], "-i")) {
            offset = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-n")) {
            total = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-d")) {
            decimal = 1;
        }
        else if (!strcmp(argv[i], "-o")) {
            optionstring = argv[++i];
        }
        else if (!strcmp(argv[i], "-sct")) {
            indextype = b_indextype_sct;
        }
        else if (!strcmp(argv[i], "-sct6")) {
            indextype = b_indextype_sct6;
        }
        else if (!strcmp(argv[i], "-avs")) {
            sctIsAvs=1;
        }
        else if (!strcmp(argv[i], "-bcm")) {
            indextype = b_indextype_bcm;
        }
        else if (!strcmp(argv[i], "-skippts")) {
            skippts = 1;
        }
        else if (!strcmp(argv[i], "-s")) {
            silence = 1;
        }
        else if (!filename) {
            filename = argv[i];
        }
        else {
            printf("Invalid parameters.\n");
            exit(1);
        }
    }

    if (!silence) {
        printHeader();
        printf("'--help' for options.\n");
    }


    if (indextype == b_indextype_unknown) {
        indextype = b_indextype_sct; /* sct */
        if (filename) {
            int len = strlen(filename);
            if (len > 4 &&
                (!strcmp(&filename[len-4], ".nav") ||
                 !strcmp(&filename[len-4], ".bcm")))
                indextype = b_indextype_bcm;
        }
        if (!silence)
            printf("Default index type: %s\n", indextype==1?"SCT":"BCM");
    }

    if (!filename || !strcmp(filename, "-")) {
        if (!filename && !silence)
            printf("Default index: stdin\n");
        filename = NULL;
        file = stdin;
    }
    else {
        file = fopen(filename, "rb");
        if (!file) {
            fprintf(stderr, "Cannot open %s\n", filename);
            exit(1);
        }
    }

    if (indextype == b_indextype_bcm) {
        BNAV_Entry entry;
        fseek(file, 0, SEEK_SET);
        if (fread(&entry, sizeof(entry), 1, file) == 1) {
            BNAV_Version ver = BNAV_get_version(&entry);
            if (!silence)
                printf("Version: %d\n", ver);
            if (ver == BNAV_Version_AVC) {
                indextype = b_indextype_avcbcm;
            }
            else if (ver == BNAV_Version_VC1_PES) {
                indextype = b_indextype_vc1bcm;
            }
            else if (ver == BNAV_Version_AVS) {
                indextype = b_indextype_avsbcm;
            }
            else if (ver == BNAV_Version_TimestampOnly) {
                indextype = b_indextype_timestamp_only;
            }
            else if (ver == BNAV_Version_AVC_Extended) {
                indextype = b_indextype_avc_extended;
            }
            else if (ver >= BNAV_VersionUnknown) {
                printf("Unknown version indicates corrupt index.\n");
                exit(1);
            }
        }
    }

    switch (indextype) {
    case b_indextype_sct:
        entrysize = sizeof(sIndexEntry); break;
    case b_indextype_sct6:
        entrysize = sizeof(sSixWordIndexEntry); break;
    case b_indextype_bcm:
    case b_indextype_avsbcm:
    case b_indextype_timestamp_only:
        entrysize = sizeof(BNAV_Entry); break;
    case b_indextype_avcbcm:
    case b_indextype_vc1bcm:
        entrysize = sizeof(BNAV_AVC_Entry); break;
    case b_indextype_avc_extended:
        entrysize = sizeof(BNAV_Entry); break;
    default: break;
    }

    fseek(file, offset * entrysize, SEEK_SET);

    for (i=0;(total==-1 || i<total) && !feof(file);i++) {
        switch (indextype) {
        case b_indextype_sct:
            {
            sIndexEntry entry;
            if (fread(&entry, sizeof(entry), 1, file) != 1)
                break;
            print_sct(i+offset, &entry);
            }
            break;
        case b_indextype_sct6:
            {
                sSixWordIndexEntry entry;
                if (fread(&entry, sizeof(entry), 1, file) != 1)
                    break;
                if(sctIsAvs)
                    print_avs_sct6(i+offset, &entry);
                else
                    print_sct6(i+offset, &entry);
            }
            break;
        case b_indextype_bcm:
        case b_indextype_avsbcm:
        case b_indextype_timestamp_only:
            {
            /* TODO: the -o option should also determine the order! */
            BNAV_Entry entry;
            if (fread(&entry, sizeof(entry), 1, file) != 1)
                break;
            print_naventry(i+offset, &entry);
            }
            break;
        case b_indextype_avcbcm:
        case b_indextype_vc1bcm:
            {
            BNAV_AVC_Entry entry;
            if (fread(&entry, sizeof(entry), 1, file) != 1)
                break;
            print_naventry(i+offset, (const BNAV_Entry*)(void*)&entry);
            }
            break;
        case b_indextype_avc_extended:
            {
            BNAV_Entry navEntry;
            BNAV_SVC_Entry entry;
            if (fread(&navEntry, sizeof(navEntry), 1, file) != 1)
                break;
            if(BNAV_Indexer_SVC_UnPackEntry(&navEntry, &entry)==0) {
                print_svcentry(i+offset, &entry);
            }
            }
            break;
        default:
            printf("Unknown index format\n");
        }
    }
    if (filename)
        fclose(file);
    return 0;
}


