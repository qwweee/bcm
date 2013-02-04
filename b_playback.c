#include "bstd.h"
#include "bkni.h"
#include "bdbg.h"
#include "b_playback.h"
#include "bcmplayer.h"
#include <stdio.h>

BDBG_MODULE(b_playback);

BDBG_OBJECT_ID(b_playback);

struct b_playback
{
    BDBG_OBJECT(b_playback)
    BNAV_Player_Handle bcmplayer;
    struct b_playback_create_settings create_settings;
    struct b_playback_start_settings start_settings;
    struct b_playback_trick_mode trick_mode;

    FILE *datafile;
    FILE *indexfile;

    uint8_t *buffer;
    unsigned rptr, wptr;
    unsigned last_size;

    BNAV_Player_PlayEntry curEntry;
    bool curEntryPending;
    uint8_t packet[188];
};

void b_playback_get_default_create_settings(struct b_playback_create_settings *p_settings)
{
    BKNI_Memset(p_settings, 0, sizeof(*p_settings));
    p_settings->fifo_size = 1 * 1024 * 1024;
}

b_playback_t b_playback_create(const struct b_playback_create_settings *p_settings)
{
    b_playback_t playback;
    struct b_playback_create_settings default_settings;

    if (!p_settings) {
        b_playback_get_default_create_settings(&default_settings);
        p_settings = &default_settings;
    }

    playback = BKNI_Malloc(sizeof(*playback));
    BKNI_Memset(playback, 0, sizeof(*playback));
    BDBG_OBJECT_SET(playback, b_playback);

    playback->create_settings = *p_settings;
    playback->buffer = BKNI_Malloc(p_settings->fifo_size);
    if (!playback->buffer) {
        goto error;
    }

    return playback;

error:
    b_playback_destroy(playback);
    return NULL;
}

void b_playback_destroy( b_playback_t playback )
{
    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (playback->buffer) {
        BKNI_Free(playback->buffer);
    }
    BDBG_OBJECT_DESTROY(playback, b_playback);
    BKNI_Free(playback);
}

void b_playback_get_default_start_settings( struct b_playback_start_settings *p_settings )
{
    BKNI_Memset(p_settings, 0, sizeof(*p_settings));
    p_settings->videoCodec = NEXUS_VideoCodec_eMpeg2;
}

int b_playback_start( b_playback_t playback, const struct b_playback_start_settings *p_settings )
{
    BNAV_Player_Settings settings;
    int rc;

    BDBG_OBJECT_ASSERT(playback, b_playback);

    if (playback->bcmplayer) {
        BDBG_ERR(("already started"));
        return -1;
    }

    if (p_settings->datafile) {
        playback->datafile = fopen(p_settings->datafile, "r");
        if (!playback->datafile) {
            BDBG_ERR(("unable to open %s", p_settings->datafile));
            rc = BERR_TRACE(-1);
            goto error;
        }
    }
    playback->indexfile = fopen(p_settings->indexfile, "r");
    if (!playback->indexfile) {
        BDBG_ERR(("unable to open %s", p_settings->indexfile));
        rc = BERR_TRACE(-1);
        goto error;
    }

    BNAV_Player_GetDefaultSettings(&settings);
    settings.videoPid = p_settings->video_pid;
    settings.readCb = (BP_READ_CB)fread;
    settings.tellCb = (BP_TELL_CB)ftell;
    settings.seekCb = (BP_SEEK_CB)fseek;
    settings.filePointer = playback->indexfile;
	settings.navVersion = 2;
    settings.useBtpsForHostTrickModes = false; /* TODO: unlikely to create compliant MPEG2 TS streams, even w/o BTP's */
    rc = BNAV_Player_Open(&playback->bcmplayer, &settings);
    if (rc) {
        rc = BERR_TRACE(rc);
        goto error;
    }

    playback->start_settings = *p_settings;
    playback->rptr = playback->wptr = 0;

    return 0;

error:
    b_playback_stop(playback);
    BDBG_ASSERT(rc); /* must be a failure */
    return rc;
}

void b_playback_stop( b_playback_t playback )
{
    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (playback->bcmplayer) {
        BNAV_Player_Close(playback->bcmplayer);
        playback->bcmplayer = NULL;
    }
    if (playback->datafile) {
        fclose(playback->datafile);
        playback->datafile = NULL;
    }
    if (playback->indexfile) {
        fclose(playback->indexfile);
        playback->indexfile = NULL;
    }
}

int b_playback_get_offsets( b_playback_t playback, unsigned timestamp, off_t *p_data_offset, off_t *p_index_offset)
{
    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (playback->bcmplayer) {
        int rc;
        BNAV_Player_Position position;
        BNAV_Player_Settings settings;
        long index;

        index = BNAV_Player_FindIndexFromTimestamp(playback->bcmplayer, timestamp);
        if (index == -1) return BERR_TRACE(-1);

        rc = BNAV_Player_GetPositionInformation(playback->bcmplayer, index, &position);
        if (rc) return BERR_TRACE(rc);

        BNAV_Player_GetSettings(playback->bcmplayer, &settings);

        *p_data_offset = ((off_t)position.offsetHi<<32) + position.offsetLo;
        *p_index_offset = index * BNAV_GetEntrySize(settings.navVersion);

        return 0;
    }
    else {
        return BERR_TRACE(-1);
    }
}

void b_playback_get_default_trick_mode( struct b_playback_trick_mode *p_trick_mode )
{
    BKNI_Memset(p_trick_mode, 0, sizeof(*p_trick_mode));
    p_trick_mode->rate = B_PLAYBACK_NORMAL_RATE;
}

int b_playback_trick_mode( b_playback_t playback, const struct b_playback_trick_mode *p_trick_mode )
{
    BNAV_Player_PlayMode playMode;
    int rc;

    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (!playback->bcmplayer) {
        BDBG_ERR(("not started"));
        return -1;
    }

    BNAV_Player_GetPlayMode(playback->bcmplayer, &playMode);
    if (p_trick_mode->rate == B_PLAYBACK_NORMAL_RATE) {
        playMode.playMode = eBpPlayNormal;
    }
    else if (p_trick_mode->rate > B_PLAYBACK_NORMAL_RATE) {
        /* TODO: compute based on p_trick_mode->rate;
        for now, just hardcode 30x */
        playMode.playMode = eBpPlayIP;
        //playMode.playModeModifier = 1;
    }
    else {
        return BERR_TRACE(NEXUS_INVALID_PARAMETER);
    }
    rc = BNAV_Player_SetPlayMode(playback->bcmplayer, &playMode);
    if (rc) return BERR_TRACE(rc);

    playback->trick_mode = *p_trick_mode;
    return 0;
}

int b_playback_seek( b_playback_t playback, long timestamp, int whence )
{
    long index;
    BNAV_Player_Position pos;
    int rc;

    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (!playback->bcmplayer) {
        BDBG_ERR(("not started"));
        return -1;
    }

    /* clear b_playback fifo */
    playback->rptr = playback->wptr = 0;

    switch (whence) {
    case SEEK_CUR:
        rc = BNAV_Player_GetPosition(playback->bcmplayer, &pos);
        if (rc) {
            timestamp = 0;
        }
        else {
            timestamp += pos.timestamp;
        }
        break;
    case SEEK_SET:
        break;
    case SEEK_END:
        /* TODO: get timestamp of bounds lastIndex */ break;
    default: return BERR_TRACE(NEXUS_INVALID_PARAMETER);
    }

    index = BNAV_Player_FindIndexFromTimestamp(playback->bcmplayer, timestamp);
    if (index == -1) return BERR_TRACE(NEXUS_INVALID_PARAMETER);

    return BNAV_Player_SetCurrentIndex(playback->bcmplayer, index);
}

int b_playback_get_buffer( b_playback_t playback, void **buffer, unsigned *p_size )
{
    int rc;

    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (!playback->bcmplayer) {
        BDBG_ERR(("not started"));
        return -1;
    }
    if (!playback->datafile) {
        BDBG_ERR(("no datafile provided"));
        return BERR_TRACE(NEXUS_INVALID_PARAMETER);
    }

    if (playback->rptr == playback->wptr && !playback->curEntryPending) {
        rc = BNAV_Player_GetNextPlayEntry(playback->bcmplayer, &playback->curEntry, playback->packet);
        if (rc) {
            /* normal at end of file */
            *p_size = 0;
            return 0;
        }

        playback->curEntryPending = true;
    }

    if (playback->rptr == playback->wptr) {
        ssize_t n;

        /* TODO: this code assumes that fifo is greater than largest I-frame. may need to segment that.
        also, may want to allow get_buffer to span >1 picture for I/O efficiency. these are all improvements
        that can be made later if the basic design is right. */
        BDBG_ASSERT(playback->rptr == playback->wptr);
        playback->rptr = playback->wptr = 0;

        if (playback->curEntry.isInsertedPacket) {
            BKNI_Memcpy(&playback->buffer[playback->rptr], playback->packet, 188);
            n = 188;
        }
        else {
            n = fseek(playback->datafile, playback->curEntry.startOffset, SEEK_SET);
            if (n < 0) return BERR_TRACE(-1);

            n = fread(&playback->buffer[playback->rptr], 1, playback->curEntry.byteCount, playback->datafile);
            if (n < 0) return BERR_TRACE(-1);
        }

        playback->wptr += n;
        playback->curEntryPending = false; /* TODO: assumes we can process it all in one pass */
    }

    if (playback->rptr != playback->wptr) {
        *buffer = &playback->buffer[playback->rptr];
        if (playback->wptr > playback->rptr) {
            *p_size = playback->wptr - playback->rptr;
        }
        else {
            *p_size = playback->create_settings.fifo_size - playback->rptr;
        }
    }
    else {
        /* no data */
        *p_size = 0;
    }

    playback->last_size = *p_size;
    return 0;
}

int b_playback_read_complete( b_playback_t playback, unsigned size )
{
    BDBG_OBJECT_ASSERT(playback, b_playback);
    if (!playback->bcmplayer) {
        BDBG_ERR(("not started"));
        return BERR_TRACE(-1);
    }
    if (size > playback->last_size) {
        return BERR_TRACE(-1);
    }
    playback->rptr += size;
    if (playback->rptr == playback->create_settings.fifo_size) {
        playback->rptr = 0;
    }
    playback->last_size = 0;
    return 0;
}

long b_playback_find_index_from_pts_and_starting_index( b_playback_t playback, uint32_t pts, long starting_index, long ending_index, unsigned threshold )
{
    long index;
    BNAV_Player_Position pos;
    
    BDBG_OBJECT_ASSERT(playback, b_playback);
    index = BNAV_Player_FindIndexFromPtsAndStartingIndex(playback->bcmplayer, pts, starting_index, ending_index, threshold);
    if (index == -1) 
        return -1;
    if (BNAV_Player_GetPositionInformation(playback->bcmplayer, index, &pos) == -1) 
        return -1;
    return pos.timestamp;
}
