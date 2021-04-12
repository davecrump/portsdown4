#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <lime/LimeSuite.h>

#include "lime.h"
#include "timing.h"
#include "buffer/buffer_circular.h"

lime_fft_buffer_t lime_fft_buffer;

extern bool NewFreq;
extern bool NewGain;
extern bool NewSpan;
extern float gain;

// Display Defaults:
double bandwidth = 512e3; // 512ks
double frequency_actual_rx = 437000000;

//Device structure, should be initialize to NULL
static lms_device_t* device = NULL;

void *lime_thread(void *arg)
{
    bool *exit_requested = (bool *)arg;

    //Find devices
    int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
    {
        return NULL;
    }

    if (n < 1)
    {
        fprintf(stderr, "Error: No LimeSDR device found, aborting.\n");
        return NULL;
    }

    if(n > 1)
    {
        printf("Warning: Multiple LimeSDR devices found (%d devices)\n", n);
    }

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
    {
        return NULL;
    }

    const lms_dev_info_t *device_info;
    double Temperature;

    device_info = LMS_GetDeviceInfo(device);

    printf("%s Serial: 0x%016" PRIx64 "\n",
        device_info->deviceName,
        device_info->boardSerialNumber
    );
    printf(" - Hardware: v%s, Firmware: v%s, Gateware: v%s\n",
        device_info->hardwareVersion,
        device_info->firmwareVersion,
        device_info->gatewareVersion
    );
    //printf(" - Protocol version: %s\n", device_info->protocolVersion);
    //printf("gateware target: %s\n", device_info->gatewareTargetBoard);

    LMS_GetChipTemperature(device, 0, &Temperature);
    printf(" - Temperature: %.0f°C\n", Temperature);

    //Initialize device with default configuration
    //Do not use if you want to keep existing configuration
    //Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI

    if (LMS_Init(device) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    //Enable RX channel
    if (LMS_EnableChannel(device, LMS_CH_RX, 0, true) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    printf("Lime Frequency Plan:\n"
            "   - Actual RX: %.0fHz\n",
        frequency_actual_rx        
    );

    //Set RX center frequency
    if (LMS_SetLOFrequency(device, LMS_CH_RX, 0, (frequency_actual_rx)) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    //Set sample rate to 10 MHz, preferred oversampling in RF 4x
    if (LMS_SetSampleRate(device, bandwidth, 4) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    double sr_host, sr_rf;
    if (LMS_GetSampleRate(device, LMS_CH_RX, 0, &sr_host, &sr_rf) < 0)
    {
        fprintf(stderr, "Warning : LMS_GetSampleRate() : %s\n", LMS_GetLastErrorMessage());
        return NULL;
    }
    printf("Lime RX Samplerate: Host: %.1fKs, RF: %.1fKs\n", (sr_host/1000.0), (sr_rf/1000.0));

    //Set RX gain
    if (LMS_SetNormalizedGain(device, LMS_CH_RX, 0, gain) != 0)  // gain is 0  to 1.0
    {
        LMS_Close(device);
        return NULL;
    }

    if (LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0) < 0)
    {
        fprintf(stderr, "LMS_Calibrate() : %s\n", LMS_GetLastErrorMessage());
        // Reset gain to zero 
        LMS_SetNormalizedGain(device, true, 0, 0);
        return false;
    }

    // Report actual LO Frequency
    double rxfreq = 0;
    LMS_GetLOFrequency(device, LMS_CH_RX, 0, &rxfreq);
    printf("RXFREQ after cal = %f\n", rxfreq);

    // Antenna is set automatically by LimeSuite

    // Set the Analog LPF bandwidth
    LMS_SetLPFBW(device, LMS_CH_RX, 0, (bandwidth * 1.2));

    // Disable test signals generation in RX channel
    if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NONE, 0, 0) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    // Streaming Setup
    lms_stream_t rx_stream;

    // Initialize streams
    rx_stream.channel = 0; //channel number
    rx_stream.fifoSize = 1024 * 1024; //fifo size in samples
    rx_stream.throughputVsLatency = 0.5;
    rx_stream.isTx = false; //RX channel
    rx_stream.dataFmt = LMS_FMT_F32;
    if (LMS_SetupStream(device, &rx_stream) != 0)
    {
        LMS_Close(device);
        return NULL;
    }

    // Initialize data buffers
    const int buffersize = 8 * 1024; // 8192 I+Q samples
    float *buffer;
    buffer = malloc(buffersize * 2 * sizeof(float)); //buffer to hold complex values (2*samples))

    // Start streaming
    LMS_StartStream(&rx_stream);

    // Streaming
    lms_stream_meta_t rx_metadata; //Use metadata for additional control over sample receive function behavior
    rx_metadata.flushPartialPacket = false; //currently has no effect in RX
    rx_metadata.waitForTimestamp = false; //currently has no effect in RX

    //uint64_t last_stats = 0;

    int samplesRead = 0;
    //lms_stream_status_t status;

#if 0
    bool monotonic_started = false;
    uint64_t samples_total = 0;
    uint64_t start_monotonic = 0;
#endif

//    uint32_t samples_rx_transferred = samplesRead / 2.0;
    while(false == *exit_requested) 
    {
        // Receive samples
        samplesRead = LMS_RecvStream(&rx_stream, buffer, buffersize, &rx_metadata, 1000);
        if (*exit_requested) 
        {
            break;
        }

        // Deal with parameter changes

        if (NewFreq == true)
        {
          LMS_SetLOFrequency(device, LMS_CH_RX, 0, frequency_actual_rx);
          NewFreq = false;

          LMS_GetLOFrequency(device, LMS_CH_RX, 0, &rxfreq);
          printf("RXFREQ after cal = %f\n", rxfreq);
        }

        if (NewGain == true)
        {
          LMS_SetNormalizedGain(device, LMS_CH_RX, 0, gain);
          NewGain = false;
        }

        if (NewSpan == true)
        {
          LMS_StopStream(&rx_stream);
          LMS_SetLPFBW(device, LMS_CH_RX, 0, (bandwidth * 1.2));
          LMS_SetSampleRate(device, bandwidth, 4);
          NewSpan = false;
          LMS_StartStream(&rx_stream);
        }


#if 0
        if(!monotonic_started)
        {
            start_monotonic = monotonic_ms();
            monotonic_started = true;
        }
#endif

        /* Copy out for Band FFT */
        pthread_mutex_lock(&lime_fft_buffer.mutex);
        /* Reset index so FFT knows it's new data */
        lime_fft_buffer.index = 0;
        memcpy(
            lime_fft_buffer.data,
            buffer,
            (samplesRead * 2 * sizeof(float))
        );
        lime_fft_buffer.size = (samplesRead * sizeof(float));
        pthread_cond_signal(&lime_fft_buffer.signal);
        pthread_mutex_unlock(&lime_fft_buffer.mutex);

        if(*exit_requested)
        {
            printf("\nLime Exit Requested\n\n");
            break;
        }

#if 0
        uint32_t head, tail, capacity, occupied;
        buffer_circular_stats(&buffer_circular_iq_main, &head, &tail, &capacity, &occupied);
        printf(" - Head: %d, Tail: %d, Capacity: %d, Occupied: %d\n",
            head, tail, capacity, occupied);
#endif

#if 0
        samples_total += samplesRead;
        printf("Lime samplerate: %.3f (total: %lld\n", (float)(samples_total * 1000) / (monotonic_ms() - start_monotonic), samples_total);
#endif
    }

    // Stop streaming
    LMS_StopStream(&rx_stream); //stream is stopped but can be started again with LMS_StartStream()

    LMS_DestroyStream(device, &rx_stream); //stream is deallocated and can no longer be used
    
    free(buffer);

    // Tidily shut down the Lime

    LMS_Reset(device);
    LMS_Init(device);
    int nb_channel = LMS_GetNumChannels( device, LMS_CH_RX );
    int c;
    for( c = 0; c < nb_channel; c++ )
    {
      LMS_EnableChannel(device, LMS_CH_RX, c, false);
    }
    nb_channel = LMS_GetNumChannels( device, LMS_CH_TX );
    for( c = 0; c < nb_channel; c++ )
    {
      LMS_EnableChannel(device, LMS_CH_TX, c, false);
    }

    LMS_Close(device);
    printf("LimeSDR Closed\n");

    return NULL;
}
