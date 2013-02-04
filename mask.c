#include <stdio.h>

#define TRANSFER_MODE_MASK_MEDIAPL              (1 << 0)
#define SCMS_FLAG_MASK_MEDIAPL                  (1 << 1)
#define GET_CONTENT_FEATURES_MASK_MEDIAPL       (1 << 2)
#define CONTENT_FEATURES_MASK_MEDIAPL           (1 << 3)
#define TIME_SEEK_RANGE_MASK_MEDIAPL            (1 << 4)
#define PLAY_SPEED_MASK_MEDIAPL                 (1 << 5)
#define GET_AVAILABLE_SEEK_RANGE_MASK_MEDIAPL   (1 << 6)
#define AVAILABLE_SEEK_RANGE_MASK_MEDIAPL       (1 << 7)
#define PEER_MANAGER_MASK_MEDIAPL               (1 << 8)
#define FRIENDLY_NAME_MASK_MEDIAPL              (1 << 9)
#define SCID_MASK_MEDIAPL                       (1 << 10)
#define REAL_TIME_INFO_MASK_MEDIAPL             (1 << 11)
  /** For OCAP HN Ext. */
#define MAX_TRICK_MODE_BANDWIDTH_MASK_MEDIAPL   (1 << 12)
#define CURRENT_DECODE_PTS_MASK_MEDIAPL         (1 << 13)
#define CHUNK_ENCODING_MODE_MASK_MEDIAPL        (1 << 14)
#define MAX_GOPS_PER_CHUNK_MASK_MEDIAPL         (1 << 15)
#define MAX_FRAMES_PER_GOP_MASK_MEDIAPL         (1 << 16)
#define SERVERSIDE_PACED_STREAMING_MASK_MEDIAPL (1 << 17)
#define FRAME_TYPES_IN_TRICK_MODE_MASK_MEDIAPL  (1 << 18)
#define FRAME_RATE_IN_TRICK_MODE_MASK_MEDIAPL   (1 << 19)
#define PRESENTATION_TIME_STAMPS_MASK_MEDIAPL   (1 << 20)
#define AV_STREAM_PARAMETERS_MASK_MEDIAPL       (1 << 21)
  /** For HTTP. */
#define RANGE_MASK_MEDIAPL                      (1 << 22)
#define CONTENT_LENGTH_MASK_MEDIAPL             (1 << 23)
#define CONTENT_RANGE_MASK_MEDIAPL              (1 << 24)
#define ACCEPT_RANGES_MASK_MEDIAPL              (1 << 25)
#define CONNECTION_MASK_MEDIAPL                 (1 << 26)
#define TRANSFER_ENCODING_MASK_MEDIAPL          (1 << 27)
int main() {
	double startTime = 60;
	int mask = 0x4000001;
	printf("0x%x\n", (1 << 25));
	printf("0x%x\n", (1 << 26));
	printf("0x%x\n", (1 << 27));
	printf("%x\n", (mask & CONNECTION_MASK_MEDIAPL));
	printf("%d\n", (mask & TRANSFER_ENCODING_MASK_MEDIAPL));

	printf("startTime:%f\n", startTime);
}
