#define MP_T_SYNC 0x47
#define DVBS_T_ISYNC 0xB8
#define DVBS_T_PAYLOAD_LEN 187
#define MP_T_FRAME_LEN 188
#define DVBS_RS_BLOCK_DATA 239
#define DVBS_RS_BLOCK_PARITY 16
#define DVBS_RS_BLOCK (DVBS_RS_BLOCK_DATA+DVBS_RS_BLOCK_PARITY)
#define DVBS_PARITY_LEN 16
#define DVBS_T_CODED_FRAME_LEN (MP_T_FRAME_LEN+DVBS_PARITY_LEN)
#define DVBS_T_FRAMES_IN_BLOCK 8
#define DVBS_T_BIT_WIDTH 8
#define DVBS_T_SCRAM_SEQ_LENGTH 1503

extern void dvb_encode_init(int FEC);
extern int dvb_encode_frame(uint8_t *tp, uint8_t *dibit);