
#ifndef DVB_H_
#define DVB_H_

#include <stdint.h>
#include <limits.h>
#include "dvb_types.h"

/*#include "express.h"
#include "dvb_gen.h"
#include "dvb_config.h"
#include "dvb_buffer.h"
*/
typedef enum { FEC_14, FEC_13, FEC_25, FEC_12, FEC_35, FEC_23, FEC_34, FEC_45, FEC_56, FEC_89, FEC_78, FEC_910 }Fec;

#define TAPS 12
#define ITP 5
#define BLOCKS 80
#define TMP_CSIZE 200

#define DVB_FILTER_BLK_LEN (TAPS*BLOCKS)

#define S_VERSION    "2.03"

#define S_DVB_S    "DVB-S"
#define S_DVB_S2   "DVB-S2"
#define S_DVB_C    "DVB-C"
#define S_DVB_T    "DVB-T"
#define S_DVB_T2   "DVB-T2"
#define S_EXPRESS_AUTO "Express Auto"
#define S_EXPRESS_16 "Express 16 bit"
#define S_EXPRESS_8  "Express 8 bit"
#define S_EXPRESS_TS "Express TS"
#define S_EXPRESS_UDP "Express UDP"
#define S_USRP2    "USRP2"
#define S_NONE     "NONE"
#define S_DIGILITE "Digilite"
#define S_UDP_TS   "UDP TS"
#define S_UDP_PS   "UDP PS"
#define S_PVRXXX   "PVRXXX"
#define S_PVRHD    "PVRHD"
#define S_FIREWIRE "FIREWIRE"
#define S_FEC_1_2  "1/2"
#define S_FEC_2_3  "2/3"
#define S_FEC_3_4  "3/4"
#define S_FEC_5_6  "5/6"
#define S_FEC_7_8  "7/8"
#define S_FEC_1_4  "1/4"
#define S_FEC_1_3  "1/3"
#define S_FEC_2_5  "2/5"
#define S_FEC_3_5  "3/5"
#define S_FEC_4_5  "4/5"
#define S_FEC_5_6  "5/6"
#define S_FEC_8_9  "8/9"
#define S_FEC_9_10 "9/10"
#define S_M_QPSK   "QPSK"
#define S_M_8PSK   "8PSK"
#define S_M_16APSK "16APSK"
#define S_M_16QAM  "16QAM"
#define S_M_32APSK "32APSK"
#define S_M_64QAM  "64QAM"
#define S_YES      "Yes"
#define S_NO       "No"
#define S_RO_0_35  "0.35"
#define S_RO_0_25  "0.25"
#define S_RO_0_20  "0.20"
#define S_FFT_2K   "2K"
#define S_FFT_8K   "8K"
#define S_GP_1_4   "1/4"
#define S_GP_1_8   "1/8"
#define S_GP_1_16  "1/16"
#define S_GP_1_32  "1/32"
#define S_CH_8MHZ  "8MHz"
#define S_CH_7MHZ  "7MHz"
#define S_CH_6MHZ  "6MHz"
#define S_CH_4MHZ  "4MHz"
#define S_CH_3MHZ  "3MHz"
#define S_CH_2MHZ  "2MHz"
#define S_CH_1MHZ  "1MHz"
#define S_CH_500KHZ  "500KHz"
#define S_FM_NORMAL "Normal"
#define S_FM_SHORT  "Short"
#define S_ALPHA_NH  "Alpha NH"
#define S_ALPHA_1   "Alpha 1"
#define S_ALPHA_2   "Alpha 2"
#define S_ALPHA_4   "Alpha 4"


#endif