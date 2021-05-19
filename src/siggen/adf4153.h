#ifndef __ADF4153_H__
#define __ADF4153_H__

/*****************************************************************************/
/****************************** Include Files ********************************/
/*****************************************************************************/
#include <stdint.h>
//#include "gpio.h"
//#include "spi.h"

/*****************************************************************************/
/*  Device specific MACROs                                                   */
/*****************************************************************************/

/* Control Bits */
#define ADF4153_CTRL_MASK                   0x3

#define ADF4153_CTRL_N_DIVIDER              0          /* N Divider Register */
#define ADF4153_CTRL_R_DIVIDER              1          /* R Divider Register */
#define ADF4153_CTRL_CONTROL                2          /* Control Register */
#define ADF4153_CTRL_NOISE_SPUR             3          /* Noise and Spur Reg*/

/* N Divider Register */

/* 12-bit fractional value */
#define ADF4153_R0_FRAC_OFFSET              2
#define ADF4153_R0_FRAC_MASK                0xFFFul
#define ADF4153_R0_FRAC(x)                  ((x) & ADF4153_R0_FRAC_MASK) \
                                              << ADF4153_R0_FRAC_OFFSET
/* 9-bit integer value */
#define ADF4153_R0_INT_OFFSET               14
#define ADF4153_R0_INT_MASK                 0x1FFul
#define ADF4153_R0_INT(x)                   ((x) & ADF4153_R0_INT_MASK) \
                                              << ADF4153_R0_INT_OFFSET

/* Fast-Lock */
#define ADF4153_R0_FASTLOCK_OFFSET          23
#define ADF4153_R0_FASTLOCK_MASK            0x1
#define ADF4153_R0_FASTLOCK(x)              ((x) & ADF4153_R0_FASTLOCK_MASK) \
                                              << ADF4153_R0_FASTLOCK_OFFSET

/* R Divider Register */

/* 12-bit interpolator modulus value */
#define ADF4153_R1_MOD_OFFSET               2
#define ADF4153_R1_MOD_MASK                 0xFFFul
#define ADF4153_R1_MOD(x)                   ((x) & ADF4153_R1_MOD_MASK) \
                                              << ADF4153_R1_MOD_OFFSET
/* 4-bit R Counter */
#define ADF4153_R1_RCOUNTER_OFFSET          14
#define ADF4153_R1_RCOUNTER_MASK            0xFul
#define ADF4153_R1_RCOUNTER(x)              ((x) & ADF4153_R1_RCOUNTER_MASK) \
                                              << ADF4153_R1_RCOUNTER_OFFSET
/* Prescale */
#define ADF4153_R1_PRESCALE_OFFSET          18
#define ADF4153_R1_PRESCALE_MASK            0x1ul
#define ADF4153_R1_PRESCALE(x)              ((x) & ADF4153_R1_PRESCALE_MASK) \
                                              << ADF4153_R1_PRESCALE_OFFSET
/* MUXOUT */
#define ADF4153_R1_MUXOUT_OFFSET            20
#define ADF4153_R1_MUXOUT_MASK              0x7
#define ADF4153_R1_MUXOUT(x)                ((x) & ADF4153_R1_MUXOUT_MASK) \
                                              << ADF4153_R1_MUXOUT_OFFSET
/* Load Control */
#define ADF4153_R1_LOAD_OFFSET              23
#define ADF4153_R1_LOAD_MASK                0x1
#define ADF4153_R1_LOAD(x)                  ((x) & ADF4153_R1_LOAD_MASK) \
                                              << ADF4153_R1_LOAD_OFFSET

/* Control Register */

/* Counter Reset */
#define ADF4153_R2_COUNTER_RST_OFFSET       2
#define ADF4153_R2_COUNTER_RST_MASK         0x1ul
#define ADF4153_R2_COUNTER_RST(x)           ((x) & ADF4153_R2_COUNTER_RST_MASK)\
                                               << ADF4153_R2_COUNTER_RST_OFFSET
/* CP Three-State */
#define ADF4153_R2_CP_3STATE_OFFSET         3
#define ADF4153_R2_CP_3STATE_MASK           0x1
#define ADF4153_R2_CP_3STATE(x)             ((x) & ADF4153_R2_CP_3STATE_MASK) \
                                               << ADF4153_R2_CP_3STATE_OFFSET
/* Power-down */
#define ADF4153_R2_POWER_DOWN_OFFSET        4
#define ADF4153_R2_POWER_DOWN_MASK          0x1
#define ADF4153_R2_POWER_DOWN(x)            ((x) & ADF4153_R2_POWER_DOWN_MASK) \
                                               <<   ADF4153_R2_POWER_DOWN_OFFSET
/* LDP */
#define ADF4153_R2_LDP_OFFSET               5
#define ADF4153_R2_LDP_MASK                 0x1
#define ADF4153_R2_LDP(x)                   ((x) & ADF4153_R2_LDP_MASK) \
                                               << ADF4153_R2_LDP_OFFSET
/* PD Polarity */
#define ADF4153_R2_PD_POL_OFFSET            6
#define ADF4153_R2_PD_POL_MASK              0x1
#define ADF4153_R2_PD_POL(x)                ((x) & ADF4153_R2_PD_POL_MASK) \
                                               << ADF4153_R2_PD_POL_OFFSET
/* CP Current Settings and CP/2 */
#define ADF4153_R2_CP_CURRENT_OFFSET        7
#define ADF4153_R2_CP_CURRENT_MASK          0xF
#define ADF4153_R2_CP_CURRENT(x)            ((x) & ADF4153_R2_CP_CURRENT_MASK) \
                                               << ADF4153_R2_CP_CURRENT_OFFSET
/* Reference doubler */
#define ADF4153_R2_REF_DOUBLER_OFFSET       11
#define ADF4153_R2_REF_DOUBLER_MASK         0x1
#define ADF4153_R2_REF_DOUBLER(x)           ((x) & ADF4153_R2_REF_DOUBLER_MASK)\
                                              << ADF4153_R2_REF_DOUBLER_OFFSET
/* Resync */
#define ADF4153_R2_RESYNC_OFFSET            12
#define ADF4153_R2_RESYNC_MASK              0x7
#define ADF4153_R2_RESYNC(x)                ((x) & ADF4153_R2_RESYNC_MASK) \
                                              << ADF4153_R2_RESYNC_OFFSET

/* Noise and spur register */

/* Noise and spur mode */
#define ADF4153_R3_NOISE_SPURG_MASK         0x3C4
#define ADF4153_R3_NOISE_SPURG(x)           ( (((x) << 0x2) & 0x7) | \
                                              (((x) >> 0x1) << 0x6) ) &\
                                               ADF4153_R3_NOISE_SPURG_MASK

/* Fast-Lock definitions */
#define ADF4153_FASTLOCK_DISABLED           0
#define ADF4153_FASTLOCK_ENABLED            1
/* Prescale definitions */
#define ADF4153_PRESCALER_4_5               0
#define ADF4153_PRESCALER_8_9               1
/* Muxout definitions */
#define ADF4153_MUXOUT_THREESTATE           0
#define ADF4153_MUXOUT_DIGITAL_LOCK         1
#define ADF4153_MUXOUT_NDIV_OUTPUT          2
#define ADF4153_MUXOUT_LOGICHIGH            3
#define ADF4153_MUXOUT_RDIV_OUTPUT          4
#define ADF4153_MUXOUT_ANALOG_LOCK          5
#define ADF4153_MUXOUT_FASTLOCK             6
#define ADF4153_MUXOUT_LOGICLOW             7
/* Load Control definitions */
#define ADF4153_LOAD_NORMAL                 0
#define ADF4153_LOAD_RESYNC                 1
/* Counter Reset Definitions */
#define ADF4153_CR_DISABLED                 0
#define ADF4153_CR_ENABLED                  1
/* CP Three-state definitions */
#define ADF4153_CP_DISABLED                 0
#define ADF4153_CP_THREE_STATE              1
/* Power-down definitions */
#define ADF4153_PD_DISABLED                 0
#define ADF4153_PD_ENABLED                  1
/* LDP definitions */
#define ADF4153_LDP_24                      0
#define ADF4153_LDP_40                      1
/* PD Polarity definitions */
#define ADF4153_PD_POL_NEGATIV              0
#define ADF4153_PD_POL_POSITIVE             1
/* CR Current Settings definitions */
#define ADF4153_CP_CURRENT_0_63             0
#define ADF4153_CP_CURRENT_1_25             1
#define ADF4153_CP_CURRENT_1_88             2
#define ADF4153_CP_CURRENT_2_50             3
#define ADF4153_CP_CURRENT_3_13             4
#define ADF4153_CP_CURRENT_3_75             5
#define ADF4153_CP_CURRENT_4_38             6
#define ADF4153_CP_CURRENT_5_00             7
#define ADF4153_CP2_CURRENT_0_31            8
#define ADF4153_CP2_CURRENT_0_63            9
#define ADF4153_CP2_CURRENT_0_94            10
#define ADF4153_CP2_CURRENT_1_25            11
#define ADF4153_CP2_CURRENT_1_57            12
#define ADF4153_CP2_CURRENT_1_88            13
#define ADF4153_CP2_CURRENT_2_19            14
#define ADF4153_CP2_CURRENT_2_50            15

/* Reference doubler definition */
#define ADF4153_REF_DOUBLER_DIS             0
#define ADF4153_REF_DOUBLER_EN              1
/* Noise and Spur mode definitions */
#define ADF4153_LOW_SPUR_MODE               0b00000
#define ADF4153_LOW_NOISE_SPUR              0b11100
#define ADF4153_LOWEST_NOISE                0b11111

#define FREQ_2_GHZ          2000000000

#endif // __ADF4153_H__