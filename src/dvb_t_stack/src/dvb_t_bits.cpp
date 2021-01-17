#include "dvb_t.h"

static uint32_t m_raw_bitrate;;
// 7 MHz Channel
const int dvb_t_bitrates[60]=
{
    4354000, 4838000, 5123000, 5278000,
    5806000, 6451000, 6830000, 7037000,
    6532000, 7257000, 7684000, 7917000,
	7257000, 8064000, 8538000, 8797000,
    7620000, 8467000, 8965000, 9237000,  
	
	8709000,  9676000,  10246000, 10556000, 
	11612000, 12902000, 13661000, 14075000, 
	13063000, 14515000, 15369000, 15834000, 
	14515000, 16127000, 17076000, 17594000, 	
	15240000, 16934000, 17930000, 18473000,

    13063000, 14515000, 15369000, 15834000,
    17418000, 19353000, 20491000, 21112000,
    19595000, 21772000, 23053000, 23751000,
    21772000, 24191000, 25614000, 26390000,
    22861000, 25401000, 26895000, 27710000
};
//
// Return the channel usuable bit rate in bps
//
uint32_t dvb_t_get_raw_bitrate(void)
{
	return m_raw_bitrate;
}
uint32_t dvb_t_config_raw_bitrate(DVBTFormat *fmt)
{
	int index;
	index = 0;

    if( fmt->co == CO_QPSK  ) index = 0;
    if( fmt->co == CO_16QAM ) index = 20;
    if( fmt->co == CO_64QAM ) index = 40;

    if( fmt->fec == FEC_12 ) index += 0;
    if( fmt->fec == FEC_23 ) index += 4;
    if( fmt->fec == FEC_34 ) index += 8;
    if( fmt->fec == FEC_56 ) index += 12;
    if( fmt->fec == FEC_78 ) index += 16;

    if( fmt->gi == GI_14 )  index += 0;
    if( fmt->gi == GI_18 )  index += 1;
    if( fmt->gi == GI_116 ) index += 2;
    if( fmt->gi == GI_132 ) index += 3;

    double rate = dvb_t_bitrates[index] * ((float)fmt->chan_bw_hz)/7000000.0;
    m_raw_bitrate = (uint32_t)rate;
    fmt->chan_capacity = (uint32_t)rate;
    return m_raw_bitrate;
}
