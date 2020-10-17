/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: main.c                                                                      */
/*    - an implementation of the Serit NIM controlling software for the MiniTiouner Hardware          */
/*    - the top level (main) and command line procesing                                               */
/* Copyright 2019 Heather Lomond                                                                      */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of longmynd.

    Longmynd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Longmynd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with longmynd.  If not, see <https://www.gnu.org/licenses/>.
*/

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- INCLUDES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "main.h"
#include "ftdi.h"
#include "stv0910.h"
#include "stv0910_regs.h"
#include "stv0910_utils.h"
#include "stv6120.h"
#include "stvvglna.h"
#include "nim.h"
#include "errors.h"
#include "fifo.h"
#include "ftdi_usb.h"
#include "udp.h"
#include "beep.h"
#include "ts.h"

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- DEFINES ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

/* Milliseconds between each i2c control loop */
#define I2C_LOOP_MS  100

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- GLOBALS ------------------------------------------------------------------------ */
/* -------------------------------------------------------------------------------------------------- */

static longmynd_config_t longmynd_config = {
    .new = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .freq_index = 0,
    .sr_index = 0
};

static longmynd_status_t longmynd_status = {
    .service_name = "\0",
    .service_provider_name = "\0",
    .last_updated_monotonic = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .signal = PTHREAD_COND_INITIALIZER,
    .ts_packet_count_nolock = 0
};

static pthread_t thread_ts_parse;
static pthread_t thread_ts;
static pthread_t thread_i2c;
static pthread_t thread_beep;

/* -------------------------------------------------------------------------------------------------- */
/* ----------------- ROUTINES ----------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------------------------- */

/* NB: This overwrites any multiple-frequency config */
void config_set_frequency(uint32_t frequency)
{
    if (frequency <= 2450000 && frequency >= 144000)
    {
        pthread_mutex_lock(&longmynd_config.mutex);

        longmynd_config.freq_requested[0] = frequency;
        longmynd_config.freq_requested[1] = 0;
        longmynd_config.freq_requested[2] = 0;
        longmynd_config.freq_requested[3] = 0;
        longmynd_config.freq_index = 0;
        longmynd_config.new = true;

        pthread_mutex_unlock(&longmynd_config.mutex);
    }
}

/* NB: This overwrites any multiple-symbolrate config */
void config_set_symbolrate(uint32_t symbolrate)
{
    if (symbolrate <= 27500 && symbolrate >= 33)
    {
        pthread_mutex_lock(&longmynd_config.mutex);

        longmynd_config.sr_requested[0] = symbolrate;
        longmynd_config.sr_requested[1] = 0;
        longmynd_config.sr_requested[2] = 0;
        longmynd_config.sr_requested[3] = 0;
        longmynd_config.sr_index = 0;
        longmynd_config.new = true;

        pthread_mutex_unlock(&longmynd_config.mutex);
    }
}

/* NB: This overwrites any multiple-frequency or multiple-symbolrate config */
void config_set_frequency_and_symbolrate(uint32_t frequency, uint32_t symbolrate)
{
    if (frequency <= 2450000 && frequency >= 144000
        && symbolrate <= 27500 && symbolrate >= 33)
    {
        pthread_mutex_lock(&longmynd_config.mutex);

        longmynd_config.freq_requested[0] = frequency;
        longmynd_config.freq_requested[1] = 0;
        longmynd_config.freq_requested[2] = 0;
        longmynd_config.freq_requested[3] = 0;
        longmynd_config.freq_index = 0;

        longmynd_config.sr_requested[0] = symbolrate;
        longmynd_config.sr_requested[1] = 0;
        longmynd_config.sr_requested[2] = 0;
        longmynd_config.sr_requested[3] = 0;
        longmynd_config.sr_index = 0;

        longmynd_config.new = true;

        pthread_mutex_unlock(&longmynd_config.mutex);
    }
}

void config_set_lnbv(bool enabled, bool horizontal)
{
    pthread_mutex_lock(&longmynd_config.mutex);

    longmynd_config.polarisation_supply = enabled;
    longmynd_config.polarisation_horizontal = horizontal;
    longmynd_config.new = true;

    pthread_mutex_unlock(&longmynd_config.mutex);
}

void config_reinit(bool increment_frsr)
{
    pthread_mutex_lock(&longmynd_config.mutex);

    if(increment_frsr)
    {
        /* Cycle symbolrate for a given frequency */
        do {
            /* Increment modulus 4 */
            longmynd_config.sr_index = (longmynd_config.sr_index + 1) & 0x3;
            /* Check if we've just cycled all symbolrates */
            if(longmynd_config.sr_index == 0) {
                /* Cycle frequences once we've tried all symbolrates */
                do {
                    /* Increment modulus 4 */
                    longmynd_config.freq_index = (longmynd_config.freq_index + 1) & 0x3;
                } while (longmynd_config.freq_requested[longmynd_config.freq_index] == 0);
            }
        } while (longmynd_config.sr_requested[longmynd_config.sr_index] == 0);
    }

    longmynd_config.new = true;

    pthread_mutex_unlock(&longmynd_config.mutex);

    if(increment_frsr)
    {
        printf("Flow: Config cycle: Frequency [%d] = %d KHz, Symbol Rate [%d] = %d KSymbols/s\n",
            longmynd_config.freq_index, longmynd_config.freq_requested[longmynd_config.freq_index],
            longmynd_config.sr_index, longmynd_config.sr_requested[longmynd_config.sr_index]
        );
    }
}

/* -------------------------------------------------------------------------------------------------- */
uint64_t monotonic_ms(void) {
/* -------------------------------------------------------------------------------------------------- */
/* Returns current value of a monotonic timer in milliseconds                                         */
/* return: monotonic timer in milliseconds                                                            */
/* -------------------------------------------------------------------------------------------------- */
    struct timespec tp;

    if(clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t process_command_line(int argc, char *argv[], longmynd_config_t *config) {
/* -------------------------------------------------------------------------------------------------- */
/* processes the command line arguments, sets up the parameters in main from them and error checks    */
/* All the required parameters are passed in                                                          */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;
    uint8_t param;
    bool main_usb_set=false;
    bool ts_ip_set=false;
    bool ts_fifo_set=false;
    bool status_ip_set=false;
    bool status_fifo_set=false;

    /* Defaults */
    config->port_swap = false;
    config->beep_enabled = false;
    config->device_usb_addr = 0;
    config->device_usb_bus = 0;
    config->ts_use_ip = false;
    strcpy(config->ts_fifo_path, "longmynd_main_ts");
    config->status_use_ip = false;
    strcpy(config->status_fifo_path, "longmynd_main_status");
    config->polarisation_supply=false;
    char polarisation_str[8];
    config->ts_timeout = 5*1000;

    param=1;
    while (param<argc-2) {
        if (argv[param][0]=='-') {
          switch (argv[param++][1]) {
            case 'u':
                config->device_usb_bus =(uint8_t)strtol(argv[param++],NULL,10);
                config->device_usb_addr=(uint8_t)strtol(argv[param  ],NULL,10);
                main_usb_set=true;
                break;
            case 'i':
                strncpy(config->ts_ip_addr,argv[param++], (16-1));
                config->ts_ip_port=(uint16_t)strtol(argv[param],NULL,10);
                config->ts_use_ip=true;
                ts_ip_set = true;
                break;
            case 't':
                strncpy(config->ts_fifo_path, argv[param], (128-1));
                ts_fifo_set=true;
                break;
            case 'I':
                strncpy(config->status_ip_addr,argv[param++], (16-1));
                config->status_ip_port=(uint16_t)strtol(argv[param],NULL,10);
                config->status_use_ip=true;
                status_ip_set = true;
                break;
            case 's':
                strncpy(config->status_fifo_path, argv[param], (128-1));
                status_fifo_set=true;
                break;
            case 'p':
                strncpy(polarisation_str, argv[param], (8-1));
                config->polarisation_supply=true;
                break;
            case 'w':
                config->port_swap=true;
                param--; /* there is no data for this so go back */
                break;
            case 'b':
                config->beep_enabled=true;
                param--; /* there is no data for this so go back */
                break;
            case 'r':
                config->ts_timeout=strtol(argv[param],NULL,10);
                break;
          }
        }
        param++;
    }

    if ((argc-param)<2) {
        err=ERROR_ARGS_INPUT;
        printf("ERROR: Main Frequency and Main Symbol Rate not found.\n");
    }

    if (err==ERROR_NONE) {
        /* Parse frequencies requested */
        char *arg_ptr = argv[param];
        char *comma_ptr;
        for(int i = 0; (i < 4) && (err == ERROR_NONE); i++)
        {
            /* Look for comma */
            comma_ptr = strchr(arg_ptr, ',');
            if(comma_ptr != NULL)
            {
                /* Set comma to NULL to end string here */
                *comma_ptr = '\0';
            }

            /* Parse up to NULL */
            config->freq_requested[i] = (uint32_t)strtol(arg_ptr,NULL,10);

            if(config->freq_requested[i] == 0) {
                err=ERROR_ARGS_INPUT;
                printf("ERROR: Main Frequency not in a valid format.\n");
            }

            if(comma_ptr == NULL)
            {
                /* No further commas, zero out rest of the config */
                for(i++; i < 4; i++)
                {
                    config->freq_requested[i] = 0;
                }
                /* Implicit drop out of wider loop here */
            }
            else
            {
                /* Move arg_ptr to other side of the comma and carry on */
                arg_ptr = comma_ptr + sizeof(char);
            }
        }
        param++;
    }

    if (err==ERROR_NONE) {
        /* Parse Symbolrates requested */
        char *arg_ptr = argv[param];
        char *comma_ptr;
        for(int i = 0; (i < 4) && (err == ERROR_NONE); i++) {
            /* Look for comma */
            comma_ptr = strchr(arg_ptr, ',');
            if(comma_ptr != NULL) {
                /* Set comma to NULL to end string here */
                *comma_ptr = '\0';
            }

            /* Parse up to NULL */
            config->sr_requested[i] = (uint32_t)strtol(arg_ptr,NULL,10);

            if(config->sr_requested[0] == 0) {
                err=ERROR_ARGS_INPUT;
                printf("ERROR: Main Symbol Rate not in a valid format.\n");
            }

            if(comma_ptr == NULL) {
                /* No further commas, zero out rest of the config */
                for(i++; i < 4; i++) {
                    config->sr_requested[i] = 0;
                }
                /* Implicit drop out of wider loop here */
            } else {
                /* Move arg_ptr to other side of the comma and carry on */
                arg_ptr = comma_ptr + sizeof(char);
            }
        }
    }

    /* Process LNB Voltage Supply parameter */
    if (err==ERROR_NONE && config->polarisation_supply) {
        if(0 == strcasecmp("h", polarisation_str)) {
            config->polarisation_horizontal=true;
        }
        else if(0 == strcasecmp("v", polarisation_str)) {
            config->polarisation_horizontal=false;
        }
        else {
            config->polarisation_supply = false;
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Polarisation voltage supply parameter not recognised\n");
        }
    }

    if (err==ERROR_NONE) {
        /* Check first frequency given */
        if (config->freq_requested[0]>2450000) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Freq (%d) must be <= 2450 MHz\n", config->freq_requested[0]);
        } else if (config->freq_requested[0]<144000) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Freq (%d) must be >= 144 MHz\n", config->freq_requested[0]);
        } else if (config->freq_requested[1] != 0) {
            /* A frequency list have been given */
            if (config->ts_timeout == -1) {
                err=ERROR_ARGS_INPUT;
                printf("ERROR: TS Timeout must be enabled when multiple frequencies are specified.\n");
            }
            /* Then check the other given frequencies */
            for(int i = 1; (i < 4) && (config->freq_requested[i] != 0); i++) {
                if (config->freq_requested[i]>2450000) {
                    err=ERROR_ARGS_INPUT;
                    printf("ERROR: Freq (%d) must be <= 2450 MHz\n", config->freq_requested[i]);
                } else if (config->freq_requested[i]<144000) {
                    err=ERROR_ARGS_INPUT;
                    printf("ERROR: Freq (%d) must be >= 144 MHz\n", config->freq_requested[i]);
                }
            }
        }
    }
    if (err==ERROR_NONE) {
        /* Check first symbolrate given */
        if (config->sr_requested[0]>27500) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: SR (%d) must be <= 27 Msymbols/s\n", config->sr_requested[0]);
        } else if (config->sr_requested[0]<33) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: SR (%d) must be >= 33 Ksymbols/s\n", config->sr_requested[0]);
        } else if (config->sr_requested[1] != 0) {
            /* A symbolrate list has been given */
            if (config->ts_timeout == -1) {
                err=ERROR_ARGS_INPUT;
                printf("ERROR: TS Timeout must be enabled when multiple symbolrates are specified.\n");
            }
            /* Then check the other given symbolrates */
            for(int i = 1; (i < 4) && (config->sr_requested[i] != 0); i++) {
                if (config->sr_requested[i]>27500) {
                    err=ERROR_ARGS_INPUT;
                    printf("ERROR: SR (%d) must be <= 27 Msymbols/s\n", config->sr_requested[i]);
                } else if (config->sr_requested[i]<33) {
                    err=ERROR_ARGS_INPUT;
                    printf("ERROR: SR (%d) must be >= 33 Ksymbols/s\n", config->sr_requested[i]);
                }
            }
        }
    }
    if (err==ERROR_NONE) {
        if (ts_ip_set && ts_fifo_set) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Cannot set TS FIFO and TS IP address\n");
        } else if (status_ip_set && status_fifo_set) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Cannot set Status FIFO and Status IP address\n");
        } else if (config->ts_use_ip && config->status_use_ip && (config->ts_ip_port == config->status_ip_port) && (0==strcmp(config->ts_ip_addr, config->status_ip_addr))) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: Cannot set Status IP & Port identical to TS IP & Port\n");
        } else if (config->ts_timeout != -1 && config->ts_timeout<=500) {
            err=ERROR_ARGS_INPUT;
            printf("ERROR: TS Timeout if enabled must be >500ms.\n");
        } else { /* err==ERROR_NONE */
             printf("      Status: Main Frequency=%i KHz\n",config->freq_requested[0]);
             for(int i = 1; (i < 4) && (config->freq_requested[i] != 0); i++) {
                printf("              Alternative Frequency=%i KHz\n",config->freq_requested[i]);
             }
             printf("              Main Symbol Rate=%i KSymbols/s\n",config->sr_requested[0]);
             for(int i = 1; (i < 4) && (config->sr_requested[i] != 0); i++) {
                printf("              Alternative Symbol Rate=%i KSymbols/s\n",config->sr_requested[i]);
             }
             if (!main_usb_set)       printf("              Using First Minitiouner detected on USB\n");
             else                     printf("              USB bus/device=%i,%i\n",config->device_usb_bus,config->device_usb_addr);
             if (!config->ts_use_ip)  printf("              Main TS output to FIFO=%s\n",config->ts_fifo_path);
             else                     printf("              Main TS output to IP=%s:%i\n",config->ts_ip_addr,config->ts_ip_port);
             if (!config->status_use_ip)  printf("              Main Status output to FIFO=%s\n",config->status_fifo_path);
             else                     printf("              Main Status output to IP=%s:%i\n",config->status_ip_addr,config->status_ip_port);
             if (config->port_swap)   printf("              NIM inputs are swapped (Main now refers to BOTTOM F-Type\n");
             else                     printf("              Main refers to TOP F-Type\n");
             if (config->beep_enabled) printf("              MER Beep enabled\n");
             if (config->polarisation_supply) printf("              Polarisation Voltage Supply enabled: %s\n", (config->polarisation_horizontal ? "H, 18V" : "V, 13V"));
             if (config->ts_timeout != -1) printf("              TS Timeout Period =%i milliseconds\n",config->ts_timeout);
             else                          printf("              TS Timeout Disabled.\n");
        }
    }

    if (err!=ERROR_NONE) {
        printf("Please refer to the longmynd manual page via:\n");
        printf("    man -l longmynd.1\n");
    }

    config->new = true;

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t do_report(longmynd_status_t *status) {
/* -------------------------------------------------------------------------------------------------- */
/* interrogates the demodulator to find the interesting info to report                                */
/*  status: the state struct                                                                          */
/* return: error code                                                                                 */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;

    /* LNAs if present */
    if (status->lna_ok) {
        uint8_t lna_gain, lna_vgo;
        if (err==ERROR_NONE) stvvglna_read_agc(NIM_INPUT_TOP, &lna_gain, &lna_vgo);
        status->lna_gain = (lna_gain<<5) | lna_vgo;
    }

    /* I,Q powers */
    if (err==ERROR_NONE) err=stv0910_read_power(STV0910_DEMOD_TOP, &status->power_i, &status->power_q);

    /* constellations */
    if (err==ERROR_NONE) {
        for (uint8_t count=0; (err==ERROR_NONE && count<NUM_CONSTELLATIONS); count++) {
            err=stv0910_read_constellation(STV0910_DEMOD_TOP, &status->constellation[count][0], &status->constellation[count][1]);
        }
    }
    
    /* puncture rate */
    if (err==ERROR_NONE) err=stv0910_read_puncture_rate(STV0910_DEMOD_TOP, &status->puncture_rate);

    /* carrier frequency offset we are trying */
    if (err==ERROR_NONE) err=stv0910_read_car_freq(STV0910_DEMOD_TOP, &status->frequency_offset);

    /* symbol rate we are trying */
    if (err==ERROR_NONE) err=stv0910_read_sr(STV0910_DEMOD_TOP, &status->symbolrate);

    /* viterbi error rate */
    if (err==ERROR_NONE) err=stv0910_read_err_rate(STV0910_DEMOD_TOP, &status->viterbi_error_rate);

    /* BER */
    if (err==ERROR_NONE) err=stv0910_read_ber(STV0910_DEMOD_TOP, &status->bit_error_rate);

    /* BCH Uncorrected Flag */
    if (err==ERROR_NONE) err=stv0910_read_errors_bch_uncorrected(STV0910_DEMOD_TOP, &status->errors_bch_uncorrected);

    /* BCH Error Count */
    if (err==ERROR_NONE) err=stv0910_read_errors_bch_count(STV0910_DEMOD_TOP, &status->errors_bch_count);

    /* LDPC Error Count */
    if (err==ERROR_NONE) err=stv0910_read_errors_ldpc_count(STV0910_DEMOD_TOP, &status->errors_ldpc_count);

    /* MER */
    if(status->state==STATE_DEMOD_S || status->state==STATE_DEMOD_S2) {
        if (err==ERROR_NONE) err=stv0910_read_mer(STV0910_DEMOD_TOP, &status->modulation_error_rate);
    } else {
        status->modulation_error_rate = 0;
    }

    /* MODCOD, Short Frames, Pilots */
    if (err==ERROR_NONE) err=stv0910_read_modcod_and_type(STV0910_DEMOD_TOP, &status->modcod, &status->short_frame, &status->pilots);
    if(status->state!=STATE_DEMOD_S2) {
        /* short frames & pilots only valid for S2 DEMOD state */
        status->short_frame = 0;
        status->pilots = 0;
    }

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
void *loop_i2c(void *arg) {
/* -------------------------------------------------------------------------------------------------- */
/* Runs a loop to configure and monitor the Minitiouner Receiver                                      */
/*  Configuration is read from the configuration struct                                               */
/*  Status is written to the status struct                                                            */
/* -------------------------------------------------------------------------------------------------- */
    thread_vars_t *thread_vars=(thread_vars_t *)arg;
    longmynd_status_t *status=(longmynd_status_t *)thread_vars->status;
    uint8_t *err = &thread_vars->thread_err;

    *err=ERROR_NONE;

    longmynd_config_t config_cpy;
    longmynd_status_t status_cpy;

    uint32_t last_ts_packet_count = 0;

    uint64_t last_i2c_loop = monotonic_ms();
    while (*err==ERROR_NONE && *thread_vars->main_err_ptr==ERROR_NONE) {
        /* Receiver State Machine Loop Timer */
        do {
            /* Sleep for at least 10ms */
            usleep(10*1000);
        } while (monotonic_ms() < (last_i2c_loop + I2C_LOOP_MS));

        status_cpy.last_ts_or_reinit_monotonic = 0;

        /* Check if there's a new config */
        if(thread_vars->config->new)
        {
            /* Lock config struct */
            pthread_mutex_lock(&thread_vars->config->mutex);
            /* Clone status struct locally */
            memcpy(&config_cpy, thread_vars->config, sizeof(longmynd_config_t));
            /* Clear new config flag */
            thread_vars->config->new = false;
            /* Set flag to clear ts buffer */
            thread_vars->config->ts_reset = true;
            pthread_mutex_unlock(&thread_vars->config->mutex);

            status_cpy.frequency_requested = config_cpy.freq_requested[config_cpy.freq_index];
            status_cpy.symbolrate_requested = config_cpy.sr_requested[config_cpy.sr_index];

            uint8_t tuner_err = ERROR_NONE; // Seperate to avoid triggering main() abort on handled tuner error.
            int32_t tuner_lock_attempts = STV6120_PLL_ATTEMPTS;
            do
            {
                /* init all the modules */
                if (*err==ERROR_NONE) *err=nim_init();
                /* we are only using the one demodulator so set the other to 0 to turn it off */
                if (*err==ERROR_NONE) *err=stv0910_init(config_cpy.sr_requested[config_cpy.sr_index],0);
                /* we only use one of the tuners in STV6120 so freq for tuner 2=0 to turn it off */
                if (*err==ERROR_NONE) tuner_err=stv6120_init(config_cpy.freq_requested[config_cpy.freq_index],0,config_cpy.port_swap);
                
                /* Tuner Lock timeout on some NIMs - Print message and pause, do..while() handles the retry logic */
                if (*err==ERROR_NONE && tuner_err==ERROR_TUNER_LOCK_TIMEOUT)
                {
                    printf("Flow: Caught tuner lock timeout, %"PRIu32" attempts at stv6120_init() remaining.\n", tuner_lock_attempts);
                    /* Power down the synthesizers to potentially improve success on retry. */
                    /* - Everything else gets powered down as well to stay within datasheet-defined states */
                    *err=stv6120_powerdown_both_paths();
                    if (*err==ERROR_NONE) usleep(200*1000);
                }
            } while (*thread_vars->main_err_ptr==ERROR_NONE
                    && *err==ERROR_NONE
                    && tuner_err==ERROR_TUNER_LOCK_TIMEOUT
                    && tuner_lock_attempts-- > 0);

            /* Propagate up tuner error from stv6120_init() */
            if (*err==ERROR_NONE) *err = tuner_err;

            /* we turn on the LNA we want and turn the other off (if they exist) */
            if (*err==ERROR_NONE) *err=stvvglna_init(NIM_INPUT_TOP,    (config_cpy.port_swap) ? STVVGLNA_OFF : STVVGLNA_ON,  &status_cpy.lna_ok);
            if (*err==ERROR_NONE) *err=stvvglna_init(NIM_INPUT_BOTTOM, (config_cpy.port_swap) ? STVVGLNA_ON  : STVVGLNA_OFF, &status_cpy.lna_ok);

            if (*err!=ERROR_NONE) printf("ERROR: failed to init a device - is the NIM powered on?\n");

            /* Enable/Disable polarisation voltage supply */
            if (*err==ERROR_NONE) *err=ftdi_set_polarisation_supply(config_cpy.polarisation_supply, config_cpy.polarisation_horizontal);
            if (*err==ERROR_NONE) {
                status_cpy.polarisation_supply = config_cpy.polarisation_supply;
                status_cpy.polarisation_horizontal = config_cpy.polarisation_horizontal;
            }

            /* now start the whole thing scanning for the signal */
            if (*err==ERROR_NONE) {
                *err=stv0910_start_scan(STV0910_DEMOD_TOP);
                status_cpy.state=STATE_DEMOD_HUNTING;
            }

            status_cpy.last_ts_or_reinit_monotonic = monotonic_ms();
        }

        /* Main receiver state machine */
        switch(status_cpy.state) {
            case STATE_DEMOD_HUNTING:
                if (*err==ERROR_NONE) *err=do_report(&status_cpy);
                /* process state changes */
                if (*err==ERROR_NONE) *err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &status_cpy.demod_state);
                if (status_cpy.demod_state==DEMOD_FOUND_HEADER) {
                    status_cpy.state=STATE_DEMOD_FOUND_HEADER;
                }
                else if (status_cpy.demod_state==DEMOD_S2) {
                    status_cpy.state=STATE_DEMOD_S2;
                }
                else if (status_cpy.demod_state==DEMOD_S) {
                    status_cpy.state=STATE_DEMOD_S;
                }
                else if ((status_cpy.demod_state!=DEMOD_HUNTING) && (*err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    *err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_FOUND_HEADER:
                if (*err==ERROR_NONE) *err=do_report(&status_cpy);
                /* process state changes */
                *err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &status_cpy.demod_state);
                if (status_cpy.demod_state==DEMOD_HUNTING) {
                    status_cpy.state=STATE_DEMOD_HUNTING;
                }
                else if (status_cpy.demod_state==DEMOD_S2)  {
                    status_cpy.state=STATE_DEMOD_S2;
                }
                else if (status_cpy.demod_state==DEMOD_S)  {
                    status_cpy.state=STATE_DEMOD_S;
                }
                else if ((status_cpy.demod_state!=DEMOD_FOUND_HEADER) && (*err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    *err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_S2:
                if (*err==ERROR_NONE) *err=do_report(&status_cpy);
                /* process state changes */
                *err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &status_cpy.demod_state);
                if (status_cpy.demod_state==DEMOD_HUNTING) {
                    status_cpy.state=STATE_DEMOD_HUNTING;
                }
                else if (status_cpy.demod_state==DEMOD_FOUND_HEADER)  {
                    status_cpy.state=STATE_DEMOD_FOUND_HEADER;
                }
                else if (status_cpy.demod_state==DEMOD_S) {
                    status_cpy.state=STATE_DEMOD_S;
                }
                else if ((status_cpy.demod_state!=DEMOD_S2) && (*err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    *err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            case STATE_DEMOD_S:
                if (*err==ERROR_NONE) *err=do_report(&status_cpy);
                /* process state changes */
                *err=stv0910_read_scan_state(STV0910_DEMOD_TOP, &status_cpy.demod_state);
                if (status_cpy.demod_state==DEMOD_HUNTING) {
                    status_cpy.state=STATE_DEMOD_HUNTING;
                }
                else if (status_cpy.demod_state==DEMOD_FOUND_HEADER)  {
                    status_cpy.state=STATE_DEMOD_FOUND_HEADER;
                }
                else if (status_cpy.demod_state==DEMOD_S2) {
                    status_cpy.state=STATE_DEMOD_S2;
                }
                else if ((status_cpy.demod_state!=DEMOD_S) && (*err==ERROR_NONE)) {
                    printf("ERROR: demodulator returned a bad scan state\n");
                    *err=ERROR_BAD_DEMOD_HUNT_STATE; /* not allowed to have any other states */
                } /* no need for another else, all states covered */
                break;

            default:
                *err=ERROR_STATE; /* we should never get here so panic if we do */
                break;
        }

        if(status->ts_packet_count_nolock > 0
            && last_ts_packet_count != status->ts_packet_count_nolock)
        {
            status_cpy.last_ts_or_reinit_monotonic = monotonic_ms();
            last_ts_packet_count = status->ts_packet_count_nolock;
        }

        /* Copy local status data over global object */
        pthread_mutex_lock(&status->mutex);

        /* Copy out other vars */
        status->state = status_cpy.state;
        status->demod_state = status_cpy.demod_state;
        status->lna_ok = status_cpy.lna_ok;
        status->lna_gain = status_cpy.lna_gain;
        status->power_i = status_cpy.power_i;
        status->power_q = status_cpy.power_q;
        status->frequency_requested = status_cpy.frequency_requested;
        status->frequency_offset = status_cpy.frequency_offset;
        status->polarisation_supply = status_cpy.polarisation_supply;
        status->polarisation_horizontal = status_cpy.polarisation_horizontal;
        status->symbolrate_requested = status_cpy.symbolrate_requested;
        status->symbolrate = status_cpy.symbolrate;
        status->viterbi_error_rate = status_cpy.viterbi_error_rate;
        status->bit_error_rate = status_cpy.bit_error_rate;
        status->modulation_error_rate = status_cpy.modulation_error_rate;
        status->errors_bch_uncorrected = status_cpy.errors_bch_uncorrected;
        status->errors_bch_count = status_cpy.errors_bch_count;
        status->errors_ldpc_count = status_cpy.errors_ldpc_count;
        memcpy(status->constellation, status_cpy.constellation, (sizeof(uint8_t) * NUM_CONSTELLATIONS * 2));
        status->puncture_rate = status_cpy.puncture_rate;
        status->modcod = status_cpy.modcod;
        status->short_frame = status_cpy.short_frame;
        status->pilots = status_cpy.pilots;
        if(status_cpy.last_ts_or_reinit_monotonic != 0) {
            status->last_ts_or_reinit_monotonic = status_cpy.last_ts_or_reinit_monotonic;
        }

        /* Set monotonic value to signal new data */
        status->last_updated_monotonic = monotonic_ms();
        /* Trigger pthread signal */
        pthread_cond_signal(&status->signal);
        pthread_mutex_unlock(&status->mutex);

        last_i2c_loop = monotonic_ms();
    }
    return NULL;
}

/* -------------------------------------------------------------------------------------------------- */
uint8_t status_all_write(longmynd_status_t *status, uint8_t (*status_write)(uint8_t, uint32_t), uint8_t (*status_string_write)(uint8_t, char*)) {
/* -------------------------------------------------------------------------------------------------- */
/* Reads the past status struct out to the passed write function                                      */
/*  Returns: error code                                                                               */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err=ERROR_NONE;

    /* Main status */
    if (err==ERROR_NONE) err=status_write(STATUS_STATE,status->state);
    /* LNAs if present */
    if (status->lna_ok) {
        if (err==ERROR_NONE) err=status_write(STATUS_LNA_GAIN,status->lna_gain);
    }
    /* I,Q powers */
    if (err==ERROR_NONE) err=status_write(STATUS_POWER_I, status->power_i);
    if (err==ERROR_NONE) err=status_write(STATUS_POWER_Q, status->power_q);
    /* constellations */
    for (uint8_t count=0; count<NUM_CONSTELLATIONS; count++) {
        if (err==ERROR_NONE) err=status_write(STATUS_CONSTELLATION_I, status->constellation[count][0]);
        if (err==ERROR_NONE) err=status_write(STATUS_CONSTELLATION_Q, status->constellation[count][1]);
    }
    /* puncture rate */
    if (err==ERROR_NONE) err=status_write(STATUS_PUNCTURE_RATE, status->puncture_rate);
    /* carrier frequency offset we are trying */
    /* note we now have the offset, so we need to add in the freq we tried to set it to */
    if (err==ERROR_NONE) err=status_write(STATUS_CARRIER_FREQUENCY, (uint32_t)(status->frequency_requested+(status->frequency_offset/1000)));
    /* LNB Voltage Supply Enabled: true / false */
    if (err==ERROR_NONE) err=status_write(STATUS_LNB_SUPPLY, status->polarisation_supply);
    /* LNB Voltage Supply is Horizontal Polarisation: true / false */
    if (err==ERROR_NONE) err=status_write(STATUS_LNB_POLARISATION_H, status->polarisation_horizontal);
    /* symbol rate we are trying */
    if (err==ERROR_NONE) err=status_write(STATUS_SYMBOL_RATE, status->symbolrate);
    /* viterbi error rate */
    if (err==ERROR_NONE) err=status_write(STATUS_VITERBI_ERROR_RATE, status->viterbi_error_rate);
    /* BER */
    if (err==ERROR_NONE) err=status_write(STATUS_BER, status->bit_error_rate);
    /* MER */
    if (err==ERROR_NONE) err=status_write(STATUS_MER, status->modulation_error_rate);
    /* BCH Uncorrected Errors Flag */
    if (err==ERROR_NONE) err=status_write(STATUS_ERRORS_BCH_UNCORRECTED, status->errors_bch_uncorrected);
    /* BCH Corrected Errors Count */
    if (err==ERROR_NONE) err=status_write(STATUS_ERRORS_BCH_COUNT, status->errors_bch_count);
    /* LDPC Corrected Errors Count */
    if (err==ERROR_NONE) err=status_write(STATUS_ERRORS_LDPC_COUNT, status->errors_ldpc_count);
    /* Service Name */
    if (err==ERROR_NONE) err=status_string_write(STATUS_SERVICE_NAME, status->service_name);
    /* Service Provider Name */
    if (err==ERROR_NONE) err=status_string_write(STATUS_SERVICE_PROVIDER_NAME, status->service_provider_name);
    /* TS Null Percentage */
    if (err==ERROR_NONE) err=status_write(STATUS_TS_NULL_PERCENTAGE, status->ts_null_percentage);
    /* TS Elementary Stream PIDs */
    for (uint8_t count=0; count<NUM_ELEMENT_STREAMS; count++) {
        if(status->ts_elementary_streams[count][0] > 0)
        {
            if (err==ERROR_NONE) err=status_write(STATUS_ES_PID, status->ts_elementary_streams[count][0]);
            if (err==ERROR_NONE) err=status_write(STATUS_ES_TYPE, status->ts_elementary_streams[count][1]);
        }
    }
    /* MODCOD */
    if (err==ERROR_NONE) err=status_write(STATUS_MODCOD, status->modcod);
    /* Short Frames */
    if (err==ERROR_NONE) err=status_write(STATUS_SHORT_FRAME, status->short_frame);
    /* Pilots */
    if (err==ERROR_NONE) err=status_write(STATUS_PILOTS, status->pilots);

    return err;
}

/* -------------------------------------------------------------------------------------------------- */
static uint8_t *sigterm_handler_err_ptr;
void sigterm_handler(int sig) {
/* -------------------------------------------------------------------------------------------------- */
/*    Runs on SIGTERM or SIGINT (Ctrl+C).                                                             */
/*    Sets main error variable to cause all threads to cleanly exit                                   */
/* -------------------------------------------------------------------------------------------------- */
    (void)sig;
    /* There are some internally handled errors, so we blindly set here to ensure we exit */
    *sigterm_handler_err_ptr = ERROR_SIGNAL_TERMINATE;
}


/* -------------------------------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
/* -------------------------------------------------------------------------------------------------- */
/*    command line processing                                                                         */
/*    module initialisation                                                                           */
/*    Print out of status information to requested interface, triggered by pthread condition variable */
/* -------------------------------------------------------------------------------------------------- */
    uint8_t err = ERROR_NONE;
    uint8_t (*status_write)(uint8_t,uint32_t);
    uint8_t (*status_string_write)(uint8_t,char*);

    sigterm_handler_err_ptr = &err;
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    printf("Flow: main\n");

    if (err==ERROR_NONE) err=process_command_line(argc, argv, &longmynd_config);

    /* first setup the fifos, udp socket, ftdi and usb */
    if(longmynd_config.status_use_ip) {
        if (err==ERROR_NONE) err=udp_status_init(longmynd_config.status_ip_addr, longmynd_config.status_ip_port);
        status_write = udp_status_write;
        status_string_write = udp_status_string_write;
    } else {
        if (err==ERROR_NONE) err=fifo_status_init(longmynd_config.status_fifo_path);
        status_write = fifo_status_write;
        status_string_write = fifo_status_string_write;
    }

    if (err==ERROR_NONE) err=ftdi_init(longmynd_config.device_usb_bus, longmynd_config.device_usb_addr);

    thread_vars_t thread_vars_ts = {
        .main_err_ptr = &err,
        .thread_err = ERROR_NONE,
        .config = &longmynd_config,
        .status = &longmynd_status
    };

    if(err==ERROR_NONE)
    {
        if(0 == pthread_create(&thread_ts, NULL, loop_ts, (void *)&thread_vars_ts))
        {
            pthread_setname_np(thread_ts, "TS Transport");
        }
        else
        {
            fprintf(stderr, "Error creating loop_ts pthread\n");
            err = ERROR_THREAD_ERROR;
        }
    }

    thread_vars_t thread_vars_ts_parse = {
        .main_err_ptr = &err,
        .thread_err = ERROR_NONE,
        .config = &longmynd_config,
        .status = &longmynd_status
    };

    if(err==ERROR_NONE)
    {
        if(0 == pthread_create(&thread_ts_parse, NULL, loop_ts_parse, (void *)&thread_vars_ts_parse))
        {
            pthread_setname_np(thread_ts_parse, "TS Parse");
        }
        else
        {
            fprintf(stderr, "Error creating loop_ts_parse pthread\n");
            err = ERROR_THREAD_ERROR;
        }
    }

    thread_vars_t thread_vars_i2c = {
        .main_err_ptr = &err,
        .thread_err = ERROR_NONE,
        .config = &longmynd_config,
        .status = &longmynd_status
    };

    if(err==ERROR_NONE)
    {
        if(0 == pthread_create(&thread_i2c, NULL, loop_i2c, (void *)&thread_vars_i2c))
        {
            pthread_setname_np(thread_i2c, "Receiver");
        }
        else
        {
            fprintf(stderr, "Error creating loop_i2c pthread\n");
            err = ERROR_THREAD_ERROR;
        }
    }

    thread_vars_t thread_vars_beep = {
        .main_err_ptr = &err,
        .thread_err = ERROR_NONE,
        .config = &longmynd_config,
        .status = &longmynd_status
    };

    if(err==ERROR_NONE)
    {
        if(0 == pthread_create(&thread_beep, NULL, loop_beep, (void *)&thread_vars_beep))
        {
            pthread_setname_np(thread_beep, "Beep Audio");
        }
        else
        {
            fprintf(stderr, "Error creating loop_beep pthread\n");
            err = ERROR_THREAD_ERROR;
        }
    }

    uint64_t last_status_sent_monotonic = 0;
    longmynd_status_t longmynd_status_cpy;

    if(err==ERROR_NONE)
    {
        /* Initialise TS data re-init timer to prevent immediate reset */
        pthread_mutex_lock(&longmynd_status.mutex);
        longmynd_status.last_ts_or_reinit_monotonic = monotonic_ms();
        pthread_mutex_unlock(&longmynd_status.mutex);
    }
    while (err==ERROR_NONE) {
        /* Test if new status data is available */
        if(longmynd_status.last_updated_monotonic != last_status_sent_monotonic) {
            /* Acquire lock on global status struct */
            pthread_mutex_lock(&longmynd_status.mutex);
            /* Clone status struct locally */
            memcpy(&longmynd_status_cpy, &longmynd_status, sizeof(longmynd_status_t));
            /* Release lock on global status struct */
            pthread_mutex_unlock(&longmynd_status.mutex);

            /* Send all status via configured output interface from local copy */
            err=status_all_write(&longmynd_status_cpy, status_write, status_string_write);

            /* Update monotonic timestamp last sent */
            last_status_sent_monotonic = longmynd_status_cpy.last_updated_monotonic;
        } else {
            /* Sleep 10ms */
            usleep(10*1000);
        }
        /* Check for errors on threads */
        if(err==ERROR_NONE &&
            (thread_vars_ts.thread_err!=ERROR_NONE
            || thread_vars_ts_parse.thread_err!=ERROR_NONE
            || thread_vars_beep.thread_err!=ERROR_NONE
            || thread_vars_i2c.thread_err!=ERROR_NONE)) {
            err=ERROR_THREAD_ERROR;
        }

        if(longmynd_config.ts_timeout != -1
        	&& monotonic_ms() > (longmynd_status.last_ts_or_reinit_monotonic + longmynd_config.ts_timeout))
        {
            /* Had a while with no TS data, reinit config to pull NIM search loops back in, or fix -S fascination */
            printf("Flow: No-data timeout, re-init config.\n");
            config_reinit(true);

            /* We've queued up a reinit so reset the timer */
            pthread_mutex_lock(&longmynd_status.mutex);
            longmynd_status.last_ts_or_reinit_monotonic = monotonic_ms();
            pthread_mutex_unlock(&longmynd_status.mutex);
        }
    }

    printf("Flow: Main loop aborted, waiting for threads.\n");

    /* No fatal errors are currently possible here, so don't currently check return values */
    pthread_join(thread_ts_parse, NULL);
    pthread_join(thread_ts, NULL);
    pthread_join(thread_i2c, NULL);
    pthread_join(thread_beep, NULL);

    printf("Flow: All threads accounted for. Exiting cleanly.\n");

    return err;
}
