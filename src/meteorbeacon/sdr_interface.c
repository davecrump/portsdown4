/* GB3MBA receiver software for RPI                */
/*                                                 */
/* Heather Nickalls 2022, 2023                     */
/* Dave Crump 2022, 2023                           */
/*                                                 */
/* versions:                                       */
/*   Dec/2022 0.0 - Initial SDRPlay API working    */
/*   Jan/2023 0.1 - added API decimation and CIC   */ 

#include <stdio.h>
#include "sdrplay_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <libwebsockets.h>
#include "transport.h"
#include "protocol.h"
#include "websocket_server.h"
#include "sdr_interface.h"


/* this is the assigned number for the radio client this code is running on */
#define RADIO_CLIENT_NUMBER 6


int masterInitialised = 0;
int slaveUninitialised = 0;

sdrplay_api_DeviceT *chosenDevice = NULL;

/* HLN ----------------------------- stream processing functions -------------------------------- */

unsigned long long int total = 0;     /* for debug */
unsigned long long int buf_total = 0; /* for debug */

#define DECIMATION 8      /* we need to do a further factor of 8 decimation to get from 80 KSPS to 10 KSPS */

long int output_buffer_i[FFT_BUFFER_SIZE];
long int output_buffer_q[FFT_BUFFER_SIZE];
unsigned char transmit_buffer[LWS_PRE + FFT_BUFFER_SIZE*2*sizeof(short)];
unsigned char server_buffer[TCP_DATA_PRE + FFT_BUFFER_SIZE*2*sizeof(short)]=
      {'H','e','a','t','h','e','r','\0'}; 
/* --------------------------------------------------------------------- */
void fft() {
/* --------------------------------------------------------------------- */
/* display the waterfall on the RPI 7" touch screen                      */
/* --------------------------------------------------------------------- */

    /**********************  DAVE's FFT CODE hooks into here **********************/
    /* fft() gets called when there are 1024 points in the above 2 buffers        */
    /* the buffer size is defined by   FFT_BUFFER_SIZE  in case 1024 isn't useful */
    /* note the output buffer is long int sized, can be reduced if needed         */
    /******************************************************************************/

}

/* --------------------------------------------------------------------- */
void buffer_send() {
/* --------------------------------------------------------------------- */
/* send the buffer over the internet to the central server               */
/* --------------------------------------------------------------------- */
//    printf("TX\n");
//    printf("IN: %li %li %li %li %li %li %li %li\n", output_buffer_i[0], output_buffer_q[0],
//                                            output_buffer_i[1], output_buffer_q[1],
//                                            output_buffer_i[1022], output_buffer_q[1022],
//                                            output_buffer_i[1023], output_buffer_q[1023]);
//    printf("TX: %i %i %i %i %i %i %i %i\n", transmit_buffer[LWS_PRE], transmit_buffer[LWS_PRE+1],
//                                  transmit_buffer[LWS_PRE+2], transmit_buffer[LWS_PRE+3],
//                                  transmit_buffer[LWS_PRE+4092], transmit_buffer[LWS_PRE+4093],
//                                  transmit_buffer[LWS_PRE+4094], transmit_buffer[LWS_PRE+4095]);
    transport_send(server_buffer);
    /* now we tell all our clients that we have data ready for them. That way when they  */
    /* come back and say they are writeable, we will be able to send them the new buffer */
    lws_callback_on_writable_all_protocol(context, protocol);
}

/* --------------------------------------------------------------------- */
void cic_filter(short xi, short xq, int reset) {
/* --------------------------------------------------------------------- */
/* implementing a 3 stage CIC filter with R = D = 8                      */
/* for 14 bits in, and decimation by 8, we need 14 + 3.log2(8) = 23 bits */
/* this is a long int on the RPI                                         */
/* the CIC looks like:                                                   */
/*                                                                       */
/*   int1 -> int2 -> int3 -> R(down) -> comb1 -> comb2 -> comb3          */
/*                                                                       */
/* --------------------------------------------------------------------- */
    static int decimation_cnt;
    static int buf_ptr;

    static long int i_int1_out,     i_int2_out,      i_int3_out;
    static long int i_comb1_out,    i_comb2_out,     i_comb3_out;
    static long int i_int3_out_old, i_comb2_out_old, i_comb1_out_old;

    static long int q_int1_out,     q_int2_out,      q_int3_out;
    static long int q_comb1_out,    q_comb2_out,     q_comb3_out;
    static long int q_int3_out_old, q_comb2_out_old, q_comb1_out_old;

    if (reset) {

        printf("Got reset\n"); /* for debugging */
        /* need to reset all the filter delays and the counters */
        decimation_cnt = DECIMATION;
        buf_ptr = 0;
  
        i_int1_out  = 0;
        i_int2_out  = 0;
        i_int3_out  = 0;
        i_comb1_out = 0;
        i_comb2_out = 0;
        i_comb3_out = 0;
        i_int3_out_old  = 0;
        i_comb2_out_old = 0;
        i_comb1_out_old = 0;

        q_int1_out  = 0;
        q_int2_out  = 0;
        q_int3_out  = 0;
        q_comb1_out = 0;
        q_comb2_out = 0;
        q_comb3_out = 0;
        q_int3_out_old  = 0;
        q_comb2_out_old = 0;
        q_comb1_out_old = 0;

    } else {

        /* for efficiency we do the decimation (by factor R) first */
        decimation_cnt--;
        if (decimation_cnt == 0) {
            decimation_cnt = DECIMATION;

            /* and then the comb filters (work right to left) */
            i_comb3_out     = i_comb2_out - i_comb2_out_old;
            i_comb2_out_old = i_comb2_out;
            i_comb2_out     = i_comb1_out - i_comb1_out_old;
            i_comb1_out_old = i_comb1_out;
            i_comb1_out     = i_int3_out  - i_int3_out_old;
            i_int3_out_old  = i_int3_out;

            /* finally we have a data point to send out so off it goes */
            output_buffer_i[buf_ptr]   = i_comb3_out;

            /* and we fill a transmit buffer with interleaved I,Q samples */
            *(short *)(transmit_buffer + LWS_PRE      + (2 * 2*buf_ptr)+2) = i_comb3_out;
            *(short *)(server_buffer   + TCP_DATA_PRE + (2 * 2*buf_ptr)+2) = i_comb3_out;

            /* since we always do the i and q in sync, no need for separate deimation counts or buffer pointers */
            q_comb3_out     = q_comb2_out - q_comb2_out_old;
            q_comb2_out_old = q_comb2_out;
            q_comb2_out     = q_comb1_out - q_comb1_out_old;
            q_comb1_out_old = q_comb1_out;
            q_comb1_out     = q_int3_out  - q_int3_out_old;
            q_int3_out_old  = q_int3_out;
            
            output_buffer_q[buf_ptr]     = q_comb3_out;
            *(short *)(transmit_buffer + LWS_PRE      + (2 * 2*buf_ptr)) = q_comb3_out;  
            *(short *)(server_buffer   + TCP_DATA_PRE + (2 * 2*buf_ptr)) = q_comb3_out;

            /* if we have filled the output buffer, then we can output it to the FFT and the internet */
            buf_ptr++;
            if (buf_ptr == FFT_BUFFER_SIZE) {
                fft();
                buffer_send();
                buf_ptr = 0;
            }
        }

        /* for efficiency we do the integrators last, again right to left, so that we don't overwrite any values */
        i_int3_out = i_int2_out   + i_int3_out;
        i_int2_out = i_int1_out   + i_int2_out;
        i_int1_out = (long int)xi + i_int1_out;

        q_int3_out = q_int2_out   + q_int3_out;
        q_int2_out = q_int1_out   + q_int2_out;
        q_int1_out = (long int)xq + q_int1_out;
    }
}



/* --------------------------------------------------------------------- */
void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                     unsigned int numSamples, unsigned int reset, void *cbContext) {
/* --------------------------------------------------------------------- */
/* the data comes into this callback and we feed it out to the CIC       */
/* and then on to the FFT for display and the internet for server access */
/* --------------------------------------------------------------------- */
	short *p_xi, *p_xq;
        int count;

        if (reset) {
            printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
            cic_filter(0,0, true);
        }

        /* note that the API decimation means that we only get 84 bytes each callback                */
	/* We have already done API decimation to get from 2.56 MSPS down to 2.56/32 = 80 KSPS       */
        /* so now we need to do the CIC filtering to get to an audio data stream of 10 kSPS          */
        /* we do this by doing a CIC filter on each I and Q seperately, using factor of 8 decimation */
        total += numSamples;  /* for debug purposes */
        p_xi = xi;
        p_xq = xq;
        
        for (count=0; count<numSamples; count++) {
            /* we may need to reset our CIC fiter to provide a good starting point */
            cic_filter(*p_xi, *p_xq, false);
            p_xi++; /* pointer maths ... ugggy but efficient */
            p_xq++;
        }
        return;
}

/* HLN ------------------------------------------------------------------------------------------------------------------------------------ */


void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext) {
	if (reset) printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
        // Process stream callback data here - this callback will only be used in dual tuner mode
	return;
}

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext) {
	switch(eventId) {
		case sdrplay_api_GainChange:
			printf("sdrplay_api_EventCb: %s, tuner=%s gRdB=%d lnaGRdB=%d systemGain=%.2f\n", "sdrplay_api_GainChange",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A":"sdrplay_api_Tuner_B",
				params->gainParams.gRdB, params->gainParams.lnaGRdB,params->gainParams.currGain);
			break;
		case sdrplay_api_PowerOverloadChange:
			printf("sdrplay_api_PowerOverloadChange: tuner=%s powerOverloadChangeType=%s\n",
				(tuner == sdrplay_api_Tuner_A)? "sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected) ? "sdrplay_api_Overload_Detected":"sdrplay_api_Overload_Corrected");
			// Send update message to acknowledge power overload message received
			sdrplay_api_Update(chosenDevice->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck,sdrplay_api_Update_Ext1_None);
			break;
		case sdrplay_api_RspDuoModeChange:
			printf("sdrplay_api_EventCb: %s, tuner=%s modeChangeType=%s\n",
				"sdrplay_api_RspDuoModeChange", (tuner == sdrplay_api_Tuner_A)?
				"sdrplay_api_Tuner_A": "sdrplay_api_Tuner_B",
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)?
				"sdrplay_api_MasterInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveAttached)?
				"sdrplay_api_SlaveAttached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDetached)?
				"sdrplay_api_SlaveDetached":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveInitialised)?
				"sdrplay_api_SlaveInitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)?
				"sdrplay_api_SlaveUninitialised":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)?
				"sdrplay_api_MasterDllDisappeared":
				(params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveDllDisappeared)?
				"sdrplay_api_SlaveDllDisappeared": "unknown type");
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterInitialised)
				masterInitialised = 1;
			if (params->rspDuoModeParams.modeChangeType == sdrplay_api_SlaveUninitialised)
				slaveUninitialised = 1;
			break;
		case sdrplay_api_DeviceRemoved:
			printf("sdrplay_api_EventCb: %s\n", "sdrplay_api_DeviceRemoved");
			break;
		default:
			printf("sdrplay_api_EventCb: %d, unknown event\n", eventId);
			break;
	}
}

void usage(void) {
	printf("Usage: sample_app.exe [A|B] [ms]\n");
	exit(1);
}

int main(int argc, char *argv[]) {
        //int a; /* HLN --------------- for debugging purposes ----------------------- */
        server_buffer[8]=RADIO_CLIENT_NUMBER;
        transport_init();
printf("done transport init\n");

	sdrplay_api_DeviceT devs[6];
	unsigned int ndev;
	int i;
	float ver = 0.0;
	sdrplay_api_ErrT err;
	sdrplay_api_DeviceParamsT *deviceParams = NULL;
	sdrplay_api_CallbackFnsT cbFns;
	sdrplay_api_RxChannelParamsT *chParams;
	int reqTuner = 0;
	int master_slave = 0;
	unsigned int chosenIdx = 0;

	if ((argc > 1) && (argc < 4)) {
		if (!strcmp(argv[1], "A")) {
			reqTuner = 0;
		} else if (!strcmp(argv[1], "B")) {
			reqTuner = 1;
		} else usage();
		if (argc == 3) {
			if (!strcmp(argv[2], "ms")) {
				master_slave = 1;
			} else usage();
		}
	} else if (argc >= 4) {
		usage();
	}
 
	printf("requested Tuner=%c Mode=%s\n", (reqTuner == 0)? 'A': 'B', (master_slave == 0)?
		"Single_Tuner": "Master/Slave");

	// Open API
	if ((err = sdrplay_api_Open()) != sdrplay_api_Success) {
		printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
	} else {
		// Enable debug logging output
		if ((err = sdrplay_api_DebugEnable(NULL, 1)) != sdrplay_api_Success) {
			printf("sdrplay_api_DebugEnable failed %s\n", sdrplay_api_GetErrorString(err));
		}
	
		// Check API versions match
		if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success) {
			printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
		}
		if (ver != SDRPLAY_API_VERSION) {
			printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
			goto CloseApi;
		}
 
		// Lock API while device selection is performed
		sdrplay_api_LockDeviceApi();

		// Fetch list of available devices
		if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success) {
			printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
			goto UnlockDeviceAndCloseApi;
		}
		printf("MaxDevs=%d NumDevs=%d\n", sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);
		if (ndev > 0) {
			for (i = 0; i < (int)ndev; i++) {
				if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
					printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i,devs[i].SerNo, devs[i].hwVer , devs[i].tuner, devs[i].rspDuoMode);
				else
					printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo,devs[i].hwVer, devs[i].tuner);
			}
		
			// Choose device
			if ((reqTuner == 1) || (master_slave == 1)) { // requires RSPduo
				// Pick first RSPduo
				for (i = 0; i < (int)ndev; i++) {
					if (devs[i].hwVer == SDRPLAY_RSPduo_ID) {
						chosenIdx = i;
						break;
					}
				}
			} else {
				// Pick first device of any type
				for (i = 0; i < (int)ndev; i++) {
					chosenIdx = i;
					break;
				}
			}
			if (i == ndev) {
				printf("Couldn't find a suitable device to open - exiting\n");
				goto UnlockDeviceAndCloseApi;
			}
			printf("chosenDevice = %d\n", chosenIdx);
			chosenDevice = &devs[chosenIdx];
 
			// If chosen device is an RSPduo, assign additional fields
			if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID) {
				// If master device is available, select device as master
				if (chosenDevice->rspDuoMode & sdrplay_api_RspDuoMode_Master) {
					// Select tuner based on user input (or default to TunerA)
					chosenDevice->tuner = sdrplay_api_Tuner_A;
					if (reqTuner == 1) chosenDevice->tuner = sdrplay_api_Tuner_B;
					// Set operating mode
					if (!master_slave) { // Single tuner mode
						chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
						printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x\n", chosenIdx,chosenDevice->rspDuoMode, chosenDevice->tuner);
					} else {
						chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
						// Need to specify sample frequency in master/slave mode
						chosenDevice->rspDuoSampleFreq = 6000000.0;
						printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x rspDuoSampleFreq=%.1f\n",
							chosenIdx, chosenDevice->rspDuoMode,
							chosenDevice->tuner, chosenDevice->rspDuoSampleFreq);
					}
				} else { // Only slave device available
					// Shouldn't change any parameters for slave device
				}
			}
 
			// Select chosen device
			if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success) {
				printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
				goto UnlockDeviceAndCloseApi;
			}
		
			// Unlock API now that device is selected
			sdrplay_api_UnlockDeviceApi();
			
			// Retrieve device parameters so they can be changed if wanted
			if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success) {
				printf("sdrplay_api_GetDeviceParams failed %s\n",sdrplay_api_GetErrorString(err));
				goto CloseApi;
			}
			
			// Check for NULL pointers before changing settings
			if (deviceParams == NULL) {
				printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
				goto CloseApi;
			}
		
			// Configure dev parameters
			if (deviceParams->devParams != NULL) {
				// This will be NULL for slave devices, only the master can change these parameters
				// Only need to update non-default settings
				if (master_slave == 0) {
                                        /* HLN ----------------------------  we choose to sample at 256 * 10 KSPS to make decimation to 10 KSPS easier ------------------- */
					deviceParams->devParams->fsFreq.fsHz = 2560000.0;
                                        /* HLN --------------------------------------------------------------------------------------------------------------------------- */
	
				} else {
					// Can't change Fs in master/slave mode
				}
			}
			// Configure tuner parameters (depends on selected Tuner which parameters to use)
			chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B)? deviceParams->rxChannelB: deviceParams->rxChannelA;
			if (chParams != NULL) {
				chParams->tunerParams.rfFreq.rfHz = 50407500.0;
				/* HLN choose the smallest bandwidth as we are going to narrow it down a lot more soon */
                                chParams->tunerParams.bwType = sdrplay_api_BW_0_200;
				if (master_slave == 0) { // Change single tuner mode to ZIF
					chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
				}
				chParams->tunerParams.gain.gRdB = 40;
				//chParams->tunerParams.gain.LNAstate = 5;
				chParams->tunerParams.gain.LNAstate = 3;

                                /* HLN --------------------------------  setup the Decimation to max ------------------------------------ */
				chParams->ctrlParams.decimation.enable = 1;            /* default 0 (off), 1=on                           */
				chParams->ctrlParams.decimation.decimationFactor = 32; /* default 1, max 32                               */
				/* wideband = 1 uses better filters but less efficient on cpu useage                                      */
				chParams->ctrlParams.decimation.wideBandSignal = 1;    /* default 0                                       */
                                /* HLN -------------------------------------------------------------------------------------------------- */
			
				// Disable AGC
				chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
			} else {
				printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
				goto CloseApi;
			}
		
			// Assign callback functions to be passed to sdrplay_api_Init()
			cbFns.StreamACbFn = StreamACallback;
			cbFns.StreamBCbFn = StreamBCallback;
			cbFns.EventCbFn = EventCallback;
		
			// Now we're ready to start by calling the initialisation function
			// This will configure the device and start streaming
			if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success) {
				printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
				if (err == sdrplay_api_StartPending) { // This can happen if we're starting inmaster/slave mode as a slave and the master is not yet running
					while(1) {
						usleep(1000);
						if (masterInitialised) { // Keep polling flag set in event callback until the master is initialised
							// Redo call - should succeed this time
							if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success) {
								printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
							}
							goto CloseApi;
						}
						printf("Waiting for master to initialise\n");
					}
				} else {
					sdrplay_api_ErrorInfoT *errInfo = sdrplay_api_GetLastError(NULL);
					if (errInfo != NULL) printf("Error in %s: %s(): line %d: %s\n", errInfo->file, errInfo->function, errInfo->line, errInfo->message);
					goto CloseApi;
				}
			}

                        /* HLN ------------ now we start serving the data to the websocket interface ------------*/
                        websocket_create();
                        /* HLN ------------ When debugging this provides a 10 seconds run time ---- ------------ */
                        //for (a=0; a<10; a++) {
                        //   usleep(1000000);
                        //}
                        /* HLN --------------------------------------------------------------------------------- */
                        
			// Finished with device so uninitialise it
			if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) {
				printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
				if (err == sdrplay_api_StopPending) {
					// We’re stopping in master/slave mode as a master and the slave is still running
					while(1) {
						usleep(1000);
						if (slaveUninitialised) {
							// Keep polling flag set in event callback until the slave is uninitialised
							// Repeat call - should succeed this time
							if ((err = sdrplay_api_Uninit(chosenDevice->dev)) != sdrplay_api_Success) {
								printf("sdrplay_api_Uninit failed %s\n", sdrplay_api_GetErrorString(err));
							}
							slaveUninitialised = 0;
							goto CloseApi;
						}
						printf("Waiting for slave to uninitialise\n");
					}
				}
				goto CloseApi;
			}
			// Release device (make it available to other applications)
			sdrplay_api_ReleaseDevice(chosenDevice);
		}
 
UnlockDeviceAndCloseApi:
		// Unlock API
		sdrplay_api_UnlockDeviceApi();
CloseApi:
		// Close API
		sdrplay_api_Close();
	}

        /* HLN ---------------------- for debug purposes --------------------------------- */
        printf("Final total = %llu words, rate=%llu words/second, number of buffers=%llu\n", total, total/10, buf_total);
	return 0;
}
