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

#include "pluto.h"
#include "/home/pi/libiio/iio.h"  // for Pluto
#include "timing.h"

lime_fft_buffer_t lime_fft_buffer;

extern bool NewFreq;
extern bool NewGain;
extern bool NewSpan;
extern bool NewCal;
extern int plutogain;
extern char PlutoIP[16];
extern bool FifthHarmonic;

// Display Defaults:
double bandwidth = 512e3; // 512ks
double frequency_actual_rx = 437000000;
double CalFreq;

// PLUTO Declarations

struct iio_context *ctx;                // Pluto Context structure

struct iio_device *phy;                 // Pluto physical transceiver device       "ad9361-phy"

struct iio_channel *rx_lo;              // Pluto RX LO frequency
struct iio_channel *rx_ctl;             // Pluto RX control
struct iio_channel *tx_lo;              // Pluto TX local oscillator
struct iio_channel *tx_ctl;             // Pluto TX control

struct iio_device *dds_core_lpc;        // Pluto DAC/TX output driver device       "cf-ad9361-dds-core-lpc"
struct iio_device *dev;                 // Pluto RX fpga driver device             "cf-ad9361-lpc"

struct iio_channel *tx0_i, *tx0_q, *tx1_i, *tx1_q, *rx0_i, *rx0_q, *rx1_i, *rx1_q;

struct iio_buffer *rxbuf;


void *sdr_thread(void *arg)
{
  bool *exit_requested = (bool *)arg;
  char URIPLUTO[20] = "ip:";
  int max_gain;

  strcat(URIPLUTO, PlutoIP);

  // Connect to the Pluto
  printf("Creating Pluto Context\n");
  ctx = iio_create_context_from_uri(URIPLUTO);
  if(ctx == NULL)
  {
    printf("Failed to Create Pluto Context\n");
  }

  // Find the AD9361 Direct Driver
  printf("Finding AD9361 Direct Driver\n");
  phy = iio_context_find_device(ctx, "ad9361-phy");
  if(phy == NULL)
  {
    printf("Failed to find ad9361-phy device\n");
  }

  // Find the AD9361 FPGA receive device driver
  dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
  if(dev == NULL)
  {
    printf("Failed to find cf-ad9361-lpc device\n");
  }

  // Find the AD9361 DAC/TX output device driver
  dds_core_lpc = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
  if(dds_core_lpc == NULL)
  {
    printf("Failed to find cf-ad9361-dds-core-lpc device\n");
  }

  // Set up the channels  More info using "iio_info"
  rx_lo = iio_device_find_channel(phy, "altvoltage0", true);      // RX local oscillator (output)
    // attr  0: external value: 0
    // attr  1: fastlock_load value: 0
    // attr  2: fastlock_recall ERROR: Invalid argument (-22)
    // attr  3: fastlock_save value: 0 166,166,166,166,166,166,166,166,166,166,166,166,166,166,166,166
    // attr  4: fastlock_store value: 0
    // attr  5: frequency value: 144999998
    // attr  6: frequency_available value: [70000000 1 6000000000]
    // attr  7: powerdown value: 0

  tx_lo = iio_device_find_channel(phy, "altvoltage1", true);      // TX local oscillator (output (true))
    // attr  0: external value: 0
    // attr  1: fastlock_load value: 0
    // attr  2: fastlock_recall ERROR: Invalid argument (-22)
    // attr  3: fastlock_save value: 0 166,166,166,166,166,166,166,166,166,166,166,166,166,166,166,166
    // attr  4: fastlock_store value: 0
    // attr  5: frequency value: 144999998
    // attr  6: frequency_available value: [70000000 1 6000000000]
    // attr  7: powerdown value: 0

  rx_ctl = iio_device_find_channel(phy, "voltage0", false);        // RX Control (input)
    // attr 0: bb_dc_offset_tracking_en value: 1
    // attr 1: filter_fir_en value: 1
    // attr 2: gain_control_mode value: manual
    // attr 3: gain_control_mode_available value: manual fast_attack slow_attack hybrid
    // attr 4: hardwaregain value: 71.000000 dB
    // attr 5: hardwaregain_available value: [-1 1 73]
    // attr 6: quadrature_tracking_en value: 1
    // attr 7: rf_bandwidth value: 1050000
    // attr 8: rf_bandwidth_available value: [200000 1 56000000]
    // attr 9: rf_dc_offset_tracking_en value: 1
    // attr 10: rf_port_select value: A_BALANCED
    // attr 11: rf_port_select_available value: A_BALANCED B_BALANCED C_BALANCED A_N A_P B_N B_P C_N C_P TX_MONITOR1 TX_MONITOR2 TX_MONITOR1_2
    // attr 12: rssi value: 99.50 dB
    // attr 13: sampling_frequency value: 1199999
    // attr 14: sampling_frequency_available value: [1041666 1 61440000]

  tx_ctl = iio_device_find_channel(phy, "voltage0", true);        // TX Control (output)
    // attr  0: filter_fir_en value: 1
    // attr  1: hardwaregain value: -10.000000 dB
    // attr  2: hardwaregain_available value: [-89.750000 0.250000 0.000000]
    // attr  3: rf_bandwidth value: 18000000
    // attr  4: rf_bandwidth_available value: [200000 1 40000000]
    // attr  5: rf_port_select value: A
    // attr  6: rf_port_select_available value: A B
    // attr  7: rssi value: 0.00 dB
    // attr  8: sampling_frequency value: 10240000
    // attr  9: sampling_frequency_available value: [2083333 1 61440000]
                        
  // Set up the streaming channels
  rx0_i = iio_device_find_channel(dev, "voltage0", 0);
  rx0_q = iio_device_find_channel(dev, "voltage1", 0);

  // Power down the TX LO
  iio_channel_attr_write_bool(tx_lo, "powerdown", true);

  // Set the cebtre frequency
  iio_channel_attr_write_longlong(rx_lo, "frequency", frequency_actual_rx);

  // Set the sampling rate
  iio_channel_attr_write_longlong(rx_ctl, "sampling_frequency", bandwidth);
  iio_channel_attr_write_longlong(rx_ctl, "rf_bandwidth", bandwidth * 1.2);

  // Ensure that base band and RF offset tracking is on
  iio_channel_attr_write_bool(rx_ctl, "bb_dc_offset_tracking_en", true);
  iio_channel_attr_write_bool(rx_ctl, "rf_dc_offset_tracking_en", true);

  // Turn any FIR filters off
  iio_channel_attr_write_bool(rx_ctl, "filter_fir_en", false);

  // Set the receiver gain (no auto gain mode)
  // Pluto gain seems to be an integer rather than 0.0 - 1.0  THIS WILL NEED TWEAKING!

  // Calculate max manual gain             (varies with frequency)

  if (frequency_actual_rx <= 1300000000)
  {
    max_gain = 73;
  }
  else if ((frequency_actual_rx > 1300000000) && (frequency_actual_rx <= 4000000000))
  {
    max_gain = 71;
  }
  else
  {
    max_gain = 62;
  }

  // now set manual gain
  iio_channel_attr_write(rx_ctl, "gain_control_mode", "manual");
  iio_channel_attr_write_double(rx_ctl, "hardwaregain", (double)(max_gain - 80 + plutogain));

  // Enable the streaming channels
  iio_channel_enable(rx0_i);
  iio_channel_enable(rx0_q);

  // Create the Receiver buffer
  rxbuf = iio_device_create_buffer(dev, 4096, false);
  if(!rxbuf)
  {
    printf("Could not create RX buffer\n");
  }

  const int buffersize = 8 * 1024; // 8192 I+Q samples
  float *buffer;
  buffer = malloc(buffersize * 2 * sizeof(float)); //buffer to hold complex values (2*samples))

  while(false == *exit_requested) 
  {
    void *p_dat, *p_end, *t_dat;
    ptrdiff_t p_inc;
 
    iio_buffer_refill(rxbuf);
 
    p_inc = iio_buffer_step(rxbuf);
    p_end = iio_buffer_end(rxbuf);
 
    int buffer_data_index = 0;
    for (p_dat = iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc, t_dat += p_inc)
    {
      const int16_t i = ((int16_t*)p_dat)[0]; // Real (I)
      const int16_t q = ((int16_t*)p_dat)[1]; // Imag (Q)

      buffer[buffer_data_index] = (float)i;
      buffer[buffer_data_index + 1] = (float)q;

      buffer_data_index = buffer_data_index + 2;
    }

    // Copy data into lime_fft_buffer.data
    pthread_mutex_lock(&lime_fft_buffer.mutex);

    // Reset index so FFT knows it is new data
    lime_fft_buffer.index = 0;

    memcpy(lime_fft_buffer.data, buffer, (8192 * 2 * sizeof(float)));

    lime_fft_buffer.size = (8192 * sizeof(float));
    pthread_cond_signal(&lime_fft_buffer.signal);
    pthread_mutex_unlock(&lime_fft_buffer.mutex);

    // Handle change of frequency
    if (NewFreq == true)
    {
      iio_channel_attr_write_longlong(rx_lo, "frequency", frequency_actual_rx);
      NewFreq = false;
      NewGain = true;  // Recalculate gain for new frequency
    }

    if (NewCal == true)
    {
      // Need to find out how to Cal Pluto RX
      // and do it here
      //NewCal = false;
    }

    if (NewGain == true)
    {
      if (frequency_actual_rx <= 1300000000)
      {
        max_gain = 73;
      }
      else if ((frequency_actual_rx > 1300000000) && (frequency_actual_rx <= 4000000000))
      {
        max_gain = 71;
      }
      else
      {
        max_gain = 62;
      }

      // now set manual gain
      iio_channel_attr_write(rx_ctl, "gain_control_mode", "manual");
      iio_channel_attr_write_double(rx_ctl, "hardwaregain", (double)(max_gain - 80 + plutogain));

      //printf("Attempted to set gain to %f\n", (double)(max_gain - 80 + plutogain));
      NewGain = false;
    }

    if (NewSpan == true)
    {
      iio_channel_attr_write_longlong(rx_ctl, "sampling_frequency", bandwidth);
      iio_channel_attr_write_longlong(rx_ctl, "rf_bandwidth", bandwidth * 1.2);
      NewSpan = false;
    }
  }
  
  usleep(1000000);
  iio_buffer_destroy(rxbuf);
  iio_context_destroy(ctx);

  free(buffer);

  return NULL;
}

