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
extern bool NewCal;
extern float gain;

// Display Defaults:
double bandwidth = 512e3; // 512ks
double frequency_actual_rx = 437000000;
double CalFreq;

//Device structure, should be initialize to NULL
static lms_device_t* device = NULL;

void *lime_thread(void *arg)
{
  bool *exit_requested = (bool *)arg;

  // Find devices
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

  // open the first device
  if (LMS_Open(&device, list[0], NULL))
  {
    return NULL;
  }

  // Query and display the device details
  const lms_dev_info_t *device_info;
  double Temperature;

  device_info = LMS_GetDeviceInfo(device);

  printf("%s Serial: 0x%016" PRIx64 "\n", device_info->deviceName, device_info->boardSerialNumber);
  printf(" - Hardware: v%s, Firmware: v%s, Gateware: v%s\n", device_info->hardwareVersion, device_info->firmwareVersion,
          device_info->gatewareVersion);

  //printf(" - Protocol version: %s\n", device_info->protocolVersion);
  //printf("gateware target: %s\n", device_info->gatewareTargetBoard);

  LMS_GetChipTemperature(device, 0, &Temperature);
  printf(" - Temperature: %.0f°C\n", Temperature);

  // Initialize device with default configuration
  if (LMS_Init(device) != 0)
  {
    LMS_Close(device);
    return NULL;
  }

  // Enable the RX channel
  if (LMS_EnableChannel(device, LMS_CH_RX, 0, true) != 0)
  {
    LMS_Close(device);
    return NULL;
  }

  // Set the RX center frequency
  if (LMS_SetLOFrequency(device, LMS_CH_RX, 0, (frequency_actual_rx)) != 0)
  {
    LMS_Close(device);
    return NULL;
  }

  // Set the initial sample rate and decimation
  if (bandwidth < 200000.0)                    // 100 kHz bandwidth
  {
    LMS_SetSampleRate(device, bandwidth, 32);
  }
  else if (bandwidth < 400000.0)              // 200 kHz bandwidth
  {
    LMS_SetSampleRate(device, bandwidth, 16);
  }
  else if (bandwidth < 1000000.0)             // 500 kHz bandwidth
  {
    LMS_SetSampleRate(device, bandwidth, 8);
  }
  else                                       // 1 - 20 MHz bandwidth
  {
    LMS_SetSampleRate(device, bandwidth, 4);
  }

  double sr_host, sr_rf;
  if (LMS_GetSampleRate(device, LMS_CH_RX, 0, &sr_host, &sr_rf) < 0)
  {
    fprintf(stderr, "Warning : LMS_GetSampleRate() : %s\n", LMS_GetLastErrorMessage());
    return NULL;
  }
  printf("Lime RX Samplerate: Host: %.1fKs, RF: %.1fKs\n", (sr_host/1000.0), (sr_rf/1000.0));

  // Set the RX gain
  if (LMS_SetNormalizedGain(device, LMS_CH_RX, 0, gain) != 0)  // gain is 0  to 1.0
  {
    LMS_Close(device);
    return NULL;
  }

  // Perform the initial calibration
  if (LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0) < 0)
  {
    fprintf(stderr, "LMS_Calibrate() : %s\n", LMS_GetLastErrorMessage());
    // Reset gain to zero 
    LMS_SetNormalizedGain(device, true, 0, 0);
    return false;
  }
  CalFreq = frequency_actual_rx;

  // Report the actual LO Frequency
  double rxfreq = 0;
  LMS_GetLOFrequency(device, LMS_CH_RX, 0, &rxfreq);
  printf("RXFREQ after cal = %f\n", rxfreq);

  // Antenna is set automatically by LimeSuite for LimeSDR Mini
  // but needs setting for LimeSDR USB.  Options:
  // LMS_SetAntenna(device, LMS_CH_RX, 0, 1); //LNA_H
  // LMS_SetAntenna(device, LMS_CH_RX, 0, 2); //LNA_L
  // LMS_SetAntenna(device, LMS_CH_RX, 0, 3); //LNA_W

  if (strcmp(device_info->deviceName, "LimeSDR-USB") == 0)
  {
    if (frequency_actual_rx >= 2e9)
    {
      LMS_SetAntenna(device, LMS_CH_RX, 0, 1); //LNA_H
      //printf("LNA_H Success\n");
    }
    else
    {
      LMS_SetAntenna(device, LMS_CH_RX, 0, 2); //LNA_L
      //printf("LNA_L Success\n");
    }
  }

  // Set the Analog LPF bandwidth (minimum of device is 1.4 MHz)
  if (LMS_SetLPFBW(device, LMS_CH_RX, 0, (bandwidth * 1.2)) != 0)
  {
    LMS_Close(device);
    printf("Failed to set analog bandwidth\n");
    return NULL;
  }

  // Disable test signals generation in RX channel
  if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NONE, 0, 0) != 0)
  {
    LMS_Close(device);
    printf("Failed to disable test signal\n");
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
    printf("Failed to set-up stream\n");
    return NULL;
  }

  // Initialize data buffers
  const int buffersize = 8 * 1024; // 8192 I+Q samples
  float *buffer;
  buffer = malloc(buffersize * 2 * sizeof(float)); //buffer to hold complex values (2*samples))

  // Start streaming
  if (LMS_StartStream(&rx_stream) != 0)
  {
    LMS_Close(device);
    printf("Failed to start streaming\n");
    return NULL;
  }

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

  //uint32_t samples_rx_transferred = samplesRead / 2.0;

  // Calibrate again now all settings settled
  LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0);

  while(false == *exit_requested) 
  {
    // Receive samples
    samplesRead = LMS_RecvStream(&rx_stream, buffer, buffersize, &rx_metadata, 1000);
    //printf("Sampleas Read = %d", samplesRead);

    // Break out of loop if required
    if (*exit_requested) 
    {
      break;
    }

    // Deal with parameter changes

    // Frequency
    if (NewFreq == true)
    {
      LMS_SetLOFrequency(device, LMS_CH_RX, 0, frequency_actual_rx);

      // Calibrate on significant frequency change
      if ((frequency_actual_rx > (1.1 * CalFreq)) || (frequency_actual_rx < (0.9 * CalFreq)))
      {
        // Pause to let frequency settle
        usleep(10000); 
        LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0);
        CalFreq = frequency_actual_rx;
      }
      NewFreq = false;

      LMS_GetLOFrequency(device, LMS_CH_RX, 0, &rxfreq);
      printf("RXFREQ after cal = %f\n", rxfreq);

      // Set correct Antenna port for LimeSDR_ USB:
      if (strcmp(device_info->deviceName, "LimeSDR-USB") ==0)
      {
        if (frequency_actual_rx >= 2e9)
        {
          LMS_SetAntenna(device, LMS_CH_RX, 0, 1); //LNA_H
        }
        else
        {
          LMS_SetAntenna(device, LMS_CH_RX, 0, 2); //LNA_L
        }
      }
    }

    // Calibration requested
    if (NewCal == true)
    {
      LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0);
      CalFreq = frequency_actual_rx;
      NewCal = false;
    }

    if (NewGain == true)
    {
      LMS_SetNormalizedGain(device, LMS_CH_RX, 0, gain);

      // Pause to let gain settle
      usleep(10000); 
      LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0);
      CalFreq = frequency_actual_rx;

      NewGain = false;
    }

    // Change of span (sample rate and decimation)
    if (NewSpan == true)
    {
      // Stop the stream
      LMS_StopStream(&rx_stream);

      // Set the new bandwidth
      LMS_SetLPFBW(device, LMS_CH_RX, 0, (bandwidth * 1.2));

      // Set the new sample rate with the correct decimation
      if (bandwidth < 200000.0)
      {
        LMS_SetSampleRate(device, bandwidth, 32);
      }
      else if (bandwidth < 400000.0)
      {
        LMS_SetSampleRate(device, bandwidth, 16);
      }
      else if (bandwidth < 1000000.0)
      {
        LMS_SetSampleRate(device, bandwidth, 8);
      }
      else
      {
        LMS_SetSampleRate(device, bandwidth, 4);
      }
          
      // Restart the stream
      LMS_StartStream(&rx_stream);

      // Calibrate after a pause
      usleep(10000); 
      LMS_Calibrate(device, LMS_CH_RX, 0, bandwidth, 0);

      NewSpan = false;
    }

#if 0
    if(!monotonic_started)
    {
      start_monotonic = monotonic_ms();
      monotonic_started = true;
    }
#endif

    // Copy out buffer for Band FFT

    // Lock the fft buffer
    pthread_mutex_lock(&lime_fft_buffer.mutex);

    // Reset index so FFT knows it's new data
    lime_fft_buffer.index = 0;
    memcpy(lime_fft_buffer.data, buffer, (samplesRead * 2 * sizeof(float)));
    lime_fft_buffer.size = (samplesRead * sizeof(float));

    // Signal that buffer is ready and unlock buffer
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
    printf(" - Head: %d, Tail: %d, Capacity: %d, Occupied: %d\n", head, tail, capacity, occupied);
#endif

#if 0
        samples_total += samplesRead;
        printf("Lime samplerate: %.3f (total: %lld\n", (float)(samples_total * 1000) / (monotonic_ms() - start_monotonic), samples_total);
#endif
  }  // end of streaming loop.  8k samples each iteration

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

