#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <lime/LimeSuite.h>

#define _FILE_OFFSET_BITS 64


// To run: cd /home/pi/rpidatv/src/wav2lime
//         gcc -o wav642lime wav642lime.c -lLimeSuite
//         ./wav642lime -i /home/pi/05-May-2023-085701-216-50-408MHz.wav -g 0.9 -f 50.408

typedef enum
{ 
    WAVFORMAT_I8 = 0, 
    WAVFORMAT_I16 = 1, 
    WAVFORMAT_F32 = 2
} wav_format_t;

static int exit_requested = 0;
static void sighandler(int sig, siginfo_t *siginfo, void *context)
{
    (void)sig;
    (void)siginfo;
    (void)context;

    exit_requested = 1;
}
 
static void print_usage(const char *progname)
{
  printf("Usage: %s [option]" "\n"
         "\t" "-i <filename> or --input <filename> Filename of input WAV" "\n"
         "\t" "-f <freq in MHz> or --frequency <freq in MHz> Frequency in MHz (default: 437.000MHz)" "\n"
         "\t" "-g <gain> or --gain <gain> with gain in [0.0 .. 1.0] set the normalized RF gain in LimeSDR (default: 0.8)" "\n"
         "\t" "-u <upsample> or --upsample <upsample> with integer value for upsampling.  Optional, default = 1" "\n"
         "\t" "-s <scale> or --scale <scale> with integer value for increasing gain.  Optional, default = 1" "\n"
        "Example:" "\n"
        "\t" "./%s -i SDRsharp.wav -f 2395.0 -g 0.7\n", progname, progname);
  exit(0);
}

typedef struct {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    uint32_t wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"
    
    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    uint32_t fmt_chunk_size; // Should be 16 for PCM
    uint16_t audio_format; // Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    uint16_t sample_alignment; // num_channels * Bytes Per Sample
    uint16_t bit_depth; // Number of bits per sample
    
    // Data
    char data_header[4]; // Contains "data"
    uint32_t data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
    // uint8_t bytes[]; // Remainder of wave file is bytes
} wav_header_t;

typedef struct
{
  // Header
  char file_head[4]; // Contains "RIFF" or "RF64"
}
wav_head_t;


int main(int argc, char *const argv[])
{
  // Catch Signal Actions
  struct sigaction control_c;
  memset(&control_c, 0, sizeof(control_c));
  control_c.sa_sigaction = &sighandler;
  control_c.sa_flags = SA_SIGINFO;

  // Declare utility variables
  int i;
  char file_type[7];

  // Declare and set defaults for input variables 
  char *filename = NULL;
  double frequency = 0.0;
  double gain = 0.8;
  int scale = 0;
  int upsample = 1;

  // Declare Program variables
  off_t file_size = 0;    
  off_t data_start_byte = 0;
  wav_format_t input_format;

  // Read in the command line
  while (1) 
  {
    int option_index = 0;
    static struct option long_options[] = 
    {
      {"input",     required_argument, 0, 'i' },
      {"frequency", optional_argument, 0, 'f' },
      {"gain",      optional_argument, 0, 'g' },
      {"upsample",  optional_argument, 0, 'u' },
      {"scale",     optional_argument, 0, 's' },
      {0,                           0, 0,  0  }
    };

    int c = getopt_long(argc, argv, "i:f:g:u:s", long_options, &option_index);
    if (c == -1)
    {
      break;
    }

    switch (c)
    {
      case 0:
        #if 1
          fprintf(stderr, "option %s", long_options[option_index].name);
          if (optarg)
          {
            fprintf(stderr, " with arg %s", optarg);
            fprintf(stderr, "\n");
          }
        #endif
        break;
      case 'i':
        filename = strdup(optarg);
      break;
      case 'f':
        frequency = (strtod(optarg, NULL) * 1.0e6);
      break;
      case 'g':
        gain = strtod(optarg, NULL);
      break;
      case 'u':
        upsample = atoi(optarg);
      break;
      case 's':
        scale = atoi(optarg);
      break;
      default:
        print_usage(argv[0]);
      break;
    }
  }



  // Print parameters
  printf("Using filename %s" "\n", filename);
  if (frequency > 1)
  {
    printf("Requested Output Frequency %f Hz\n", frequency);
  }
  else
  {
    printf("No output frequency specified,  Default is 437 MHz\n");
    printf("Unless file type RF64 specifies frequency, in which case that will be used\n");
  }
  printf("Requested Lime Gain %f / 1.0\n", gain);
  printf("Upsampling = %d\n", upsample);
  printf("Input Scaling = %d\n", scale);

  // Check File size
  struct stat buf;
  stat(filename, &buf);
  file_size = buf.st_size;
  printf("File size = %ld bytes\n\n", file_size);

  // Open the file
  FILE *fileptr = fopen(filename, "r");
  if(fileptr == NULL)
  {
    printf("Cannot open file for reading\nExiting\n");
    exit(EXIT_FAILURE);
  }

  // Check if file is RIFF (SDR Sharp) or RF64 (SDR Console) 
  wav_head_t *wav_head_ptr = malloc(sizeof(wav_head_t));
  if(wav_head_ptr == NULL)
  {
    printf("Cannot allocate memory for wav_head\nExiting\n");
    exit(EXIT_FAILURE);
  }
  if(fread(wav_head_ptr, sizeof(wav_head_t), 1, fileptr) < 1)  // returns number of items (1) , not size
  {
    printf("Failed to read WAV header from input file\nExiting\n");
    exit(EXIT_FAILURE);
  }
  strncpy(file_type, wav_head_ptr->file_head, 4);
  file_type[4] = '\0';
  printf ("File Type: %s\n", file_type);
  fclose(fileptr);

  // Re-open the file and analyse depending on File Type
  fileptr = fopen(filename, "r");
  if(fileptr == NULL)
  {
    printf("Cannot open file for reading\nExiting\n");
    exit(EXIT_FAILURE);
  }

  wav_header_t *wav_header_ptr = malloc(sizeof(wav_header_t));

  if (strcmp(file_type, "RIFF") == 0)                                // Filetype RIFF
  {
    if (frequency < 1)                                              // Set default frequency
    {
        frequency = 437e6;
    }
    if(wav_header_ptr == NULL)                                       // Check header read
    {
      printf("Cannot allocate for wav_header\nExiting\n");
      exit(EXIT_FAILURE);
    }
    if(fread(wav_header_ptr, sizeof(wav_header_t), 1, fileptr) < 1)  // Read file header
    {
      printf("Failed to read WAV header from input file\nExiting\n");
      exit(EXIT_FAILURE);
    }

    if(wav_header_ptr->audio_format == 3)
    {
        input_format = WAVFORMAT_F32;
    }
    else if(wav_header_ptr->audio_format == 1)
    {
      if(wav_header_ptr->fmt_chunk_size == 8)
      {
        input_format = WAVFORMAT_I8;
      }
      else if(wav_header_ptr->fmt_chunk_size == 16)
      {
        input_format = WAVFORMAT_I16;
        if (scale == 0)  // So not set on the command line or adjusted above
        {
          scale = 1;     // as this is normally 12 bit that needs boosting
        }
      }
      else
      {
        printf("Unknown WAV PCM bit-depth\n");
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      printf("Unknown WAV format\n");
    }
    if (scale == 0)  // So not set on the command line or adjusted above
    {
      scale = 1;
    }
  }
  else if (strcmp(file_type, "RF64") == 0)                          // Filetype RF64
  {
    if(wav_header_ptr == NULL)
    {
      printf("Cannot allocate for wav_header\nExiting\n");
      exit(EXIT_FAILURE);
    }
    if(fread(wav_header_ptr, sizeof(wav_header_t), 1, fileptr) < 1)  // returns number of items (1) , not size
    {
      printf("Failed to read WAV header from input file\nExiting\n");
      exit(EXIT_FAILURE);
    }

    char test_char;
    char bin_char;
    char BitsPerSample[31] = "BitsPerSample=";
    char BytesPerSecond[31] = "BytesPerSecond=";
    char RadioCenterFreq[31] = "RadioCenterFreq=";
    char SampleRate[31] = "SampleRate=";
    char data[15] = "data";

    bool exit_decode = false;
    bool reading_value = false;
    bool awaiting_quote = false;

    uint32_t BytesPerSecondValue = 0;
    uint32_t BitsPerSampleValue = 0;
    uint64_t RadioCenterFreqValue = 0;
    uint32_t SampleRateValue = 0;

    int string_test = 0;
    int string_next_pos = 0;

    for (i = 0; i < 3072; i++)
    {
      test_char = fgetc(fileptr);

      if (string_test == 0)  // Not in a string, so check for first character
      {
        if (test_char == 'B')
        {
          string_test = 1;                                // Could be BitsPerSample (2) or BytesPerSecond (3)
          string_next_pos = 0;
          bin_char = fgetc(fileptr);                      // Read and discard second unicode byte
        }
        if (test_char == 'R')
        {
          string_test = 4;   // Could be RadioCenterFreq
          string_next_pos = 1;
          bin_char = fgetc(fileptr);                      // Read and discard second unicode byte
        }
        if (test_char == 'S')
        {
          string_test = 5;   // Could be SampleRate
          string_next_pos = 1;
          bin_char = fgetc(fileptr);                      // Read and discard second unicode byte
        }
        if (test_char == 'd')
        {
          string_test = 6;   // Could be data
          string_next_pos = 1;
        }
      }
      else                  // in a string
      {
        switch (string_test)
        {
          case 1:                                            // Could be BitsPerSample (2) or BytesPerSecond (3)
            if (test_char == 'i')
            {
              string_test = 2;   // Could be BitsPerSample
              string_next_pos = 2;
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
            }
            else if (test_char == 'y')
            {
              string_test = 3;   // Could be BytesPerSecond
              string_next_pos = 2;
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
            }
            else
            {
              string_test = 0;  // Not BytesPerSecond
              string_next_pos = 0;
            }
          break;
          case 2:                   // Could be BitsPerSample (2)
            if ((test_char == BitsPerSample[string_next_pos]) && (reading_value == false))
            {
              string_test = 2;   // Could be BitsPerSample
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
              string_next_pos++;

              if (string_next_pos == strlen(BitsPerSample))
              {
                reading_value = true;
                awaiting_quote = true;
              }
            }
            else if (reading_value == true)
            {
              if ((test_char == 34) && (awaiting_quote == true))   // Found initial quote
              {
                awaiting_quote = false;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char >= 48) && (test_char <= 57) && (awaiting_quote == false))  // number
              {
                BitsPerSampleValue = BitsPerSampleValue * 10;
                BitsPerSampleValue = BitsPerSampleValue + test_char - 48;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char == 34) && (awaiting_quote == false))   // Found end quote
              {
                reading_value = false;
                string_test = 0;  // No longer BitsPerSample
                string_next_pos = 0;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
                printf ("The value of BitsPerSample is %d\n", BitsPerSampleValue);
              }
              else
              {
                awaiting_quote = false;
                reading_value = false;
                string_test = 0;  // An error has occured
                string_next_pos = 0;
              }
            }              
            else
            {
              string_test = 0;  // Not BitsPerSample
              string_next_pos = 0;
            }
          break;
          case 3:                   // Could be BytesPerSecond (3)
            if ((test_char == BytesPerSecond[string_next_pos]) && (reading_value == false))
            {
              string_test = 3;   // Could be BytesPerSecond
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
              string_next_pos++;
              
              if (string_next_pos == strlen(BytesPerSecond))
              {
                reading_value = true;
                awaiting_quote = true;
              }  
            }
            else if (reading_value == true)
            {
              if ((test_char == 34) && (awaiting_quote == true))   // Found initial quote
              {
                awaiting_quote = false;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char >= 48) && (test_char <= 57) && (awaiting_quote == false))  // number
              {
                BytesPerSecondValue = BytesPerSecondValue * 10;
                BytesPerSecondValue = BytesPerSecondValue + test_char - 48;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char == 34) && (awaiting_quote == false))   // Found end quote
              {
                reading_value = false;
                string_test = 0;  // No longer BytesPerSecond
                string_next_pos = 0;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
                printf ("The value of BytesPerSecond is %d\n", BytesPerSecondValue);
              }
              else
              {
                awaiting_quote = false;
                reading_value = false;
                string_test = 0;  // An error has occured
                string_next_pos = 0;
              }
            }              
            else
            {
              string_test = 0;  // Not BytesPerSecond
              string_next_pos = 0;
            }
          break;
          case 4:                   // Could be RadioCenterFreq (4)
            if ((test_char == RadioCenterFreq[string_next_pos]) && (reading_value == false))
            {
              string_test = 4;   // Could be RadioCenterFreq
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
              string_next_pos++;

              if (string_next_pos == strlen(RadioCenterFreq))
              {
                reading_value = true;
                awaiting_quote = true;
              }
            }
            else if (reading_value == true)
            {
              if ((test_char == 34) && (awaiting_quote == true))   // Found initial quote
              {
                awaiting_quote = false;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char >= 48) && (test_char <= 57) && (awaiting_quote == false))  // number
              {
                RadioCenterFreqValue = RadioCenterFreqValue * 10;
                RadioCenterFreqValue = RadioCenterFreqValue + test_char - 48;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char == 34) && (awaiting_quote == false))   // Found end quote
              {
                reading_value = false;
                string_test = 0;  // No longer RadioCenterFreq
                string_next_pos = 0;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
                printf ("The value of RadioCenterFreq is %lld\n", RadioCenterFreqValue);
              }
              else
              {
                awaiting_quote = false;
                reading_value = false;
                string_test = 0;  // An error has occured
                string_next_pos = 0;
                printf("Error reading RadioCenterFreq\n");
              }
            }              
            else
            {
              string_test = 0;  // Not RadioCenterFreq
              string_next_pos = 0;
            }
          break;
          case 5:                   // Could be SampleRate (5)
            if ((test_char == SampleRate[string_next_pos]) && (reading_value == false))
            {
              string_test = 5;   // Could be SampleRate
              bin_char = fgetc(fileptr);                    // Read and discard second unicode byte
              string_next_pos++;

              if (string_next_pos == strlen(SampleRate))
              {
                reading_value = true;
                awaiting_quote = true;
              }
            }
            else if (reading_value == true)
            {
              if ((test_char == 34) && (awaiting_quote == true))   // Found initial quote
              {
                awaiting_quote = false;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char >= 48) && (test_char <= 57) && (awaiting_quote == false))  // number
              {
                SampleRateValue = SampleRateValue * 10;
                SampleRateValue = SampleRateValue + test_char - 48;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
              }
              else if ((test_char == 34) && (awaiting_quote == false))   // Found end quote
              {
                reading_value = false;
                string_test = 0;  // No longer SampleRate
                string_next_pos = 0;
                bin_char = fgetc(fileptr);                         // Read and discard second unicode byte
                printf ("The value of SampleRate is %d\n", SampleRateValue);
              }
              else
              {
                awaiting_quote = false;
                reading_value = false;
                string_test = 0;  // An error has occured
                string_next_pos = 0;
                printf("Error reading SampleRate\n");
              }
            }              
            else
            {
              string_test = 0;  // Not SampleRate
              string_next_pos = 0;
            }
          break;
          case 6:                   // Could be data    
            if (test_char == data[string_next_pos])
            {
              string_next_pos++;
              if (string_next_pos == 4) // string "data" found so move on 4 and exit decode
              {
                 fread(data, 4, 1, fileptr);
                 printf("Found data\n");

                 data_start_byte = ftell(fileptr);
                 //printf("Data Start Position = %d\n", data_start_byte);
                 //printf("Data Size = %d bytes\n", file_size - data_start_byte);
                 exit_decode = true;
              }
            }
            else                    // Not data
            {
              string_test = 0;
              string_next_pos = 0;
            }
          break;
        }
      }
      if (exit_decode)         // get out
      {
        break;
      }
    }

    wav_header_ptr->fmt_chunk_size = BitsPerSampleValue;
    wav_header_ptr->audio_format = 1;
    wav_header_ptr->sample_rate = SampleRateValue;
    wav_header_ptr->bit_depth = BitsPerSampleValue;
    wav_header_ptr->data_bytes = file_size - data_start_byte - 350; // data finishes about 350 bytes before EOF

    if (frequency < 1)
    {
      frequency = (double)RadioCenterFreqValue;
    }

    if(wav_header_ptr->fmt_chunk_size == 32)
    {
        input_format = WAVFORMAT_F32;
    }
    else if(wav_header_ptr->fmt_chunk_size == 8)
    {
      input_format = WAVFORMAT_I8;
    }
    else if(wav_header_ptr->fmt_chunk_size == 16)
    {
      input_format = WAVFORMAT_I16;
    }
    else
    {
      printf("Unknown WAV PCM bit-depth\n");
      exit(EXIT_FAILURE);
    }
    if (scale == 0)  // So not set on the command line
    {
      scale = 1;
    }

  }
  else
  {
    printf("WAV header %s is not not recognised.\nExiting\n", file_type);
    exit(EXIT_FAILURE);
  }

  // Command line and input file parsing complete, so Print results

  printf(" - frequency : %f MHz\n", frequency / 1e6);
  printf(" - fmt_chunk_size : %d\n", wav_header_ptr->fmt_chunk_size);
  printf(" - audio_format : %d\n", wav_header_ptr->audio_format);
  printf(" - sample_rate : %d\n", wav_header_ptr->sample_rate);
  printf(" - bit_depth : %d\n", wav_header_ptr->bit_depth);
  printf(" - bytes: %d\n", wav_header_ptr->data_bytes);
  printf(" - duration: %d seconds\n", wav_header_ptr->data_bytes / (wav_header_ptr->sample_rate * (wav_header_ptr->bit_depth / 4)));

  // Intialise and calibrate LimeSDR

  double sampleRate = (double)wav_header_ptr->sample_rate;
  double rfBandwidth = (double)wav_header_ptr->sample_rate * 1.5;

  // Determine correct Antenna first
  char const *antenna = "BAND1";  // correct for < 2 GHz LimeSDR USB, or > 2 GHz LimeSDR Mini or LMN

  //if ((LimeSDR_USB == true) && (freq > 2000000000))
  //{
  //  antenna = "BAND2";
  //}
  //if ((LimeSDR_USB == false) && (freq < 2000000000))
  //{
  //  antenna = "BAND2";
  //}


  int device_count = LMS_GetDeviceList(NULL);
  if(device_count < 1){
    printf("Error: No LimeSDR device found\n");
    return -1;
  }
  lms_info_str_t *device_list = malloc(sizeof(lms_info_str_t) * device_count);
  device_count = LMS_GetDeviceList(device_list);

  printf("LimeSDR Devices:\n");
  for(i = 0; i < device_count; i++)
  {
    printf("  %d: %s\n", i, device_list[i]);
    i++;
  }

  lms_device_t *device = NULL;

  int device_index = 0;
  if(LMS_Open(&device, device_list[device_index], NULL)){
    return -1;
  }

  // Normalized gain shall be in [0.0 .. 1.0]
  if(gain < 0.0)
  {
    gain = 0.0;
  }
  if(gain > 1.0)
  {
    gain = 1.0;
  }
  printf("Using normalized gain %lf (0.0-1.0)\n", gain);

  int lmsReset = LMS_Reset(device);
  if(lmsReset)
  {
    printf("lmsReset %d(%s)" "\n", lmsReset, LMS_GetLastErrorMessage());
  }

  int lmsInit = LMS_Init(device);
  if(lmsInit)
  {
    printf("lmsInit %d(%s)" "\n", lmsInit, LMS_GetLastErrorMessage());
  }

  int channel = 0;
  int channel_count = LMS_GetNumChannels(device, LMS_CH_TX);
  printf("Tx channel count %d" "\n", channel_count);
  printf("Using channel %d" "\n", channel);
    
  LMS_SetNormalizedGain(device, LMS_CH_TX, channel, gain);

  // Disable all other channels
  //LMS_EnableChannel(device, LMS_CH_TX, 1 - channel, false);

  // LMS_EnableChannel(device, LMS_CH_RX, 0, true); /* LimeSuite bug workaround (needed since LimeSuite git rev 52d6129 - or v18.06.0) */

  //LMS_EnableChannel(device, LMS_CH_RX, 1, false);

  // Enable our Tx channel
  LMS_EnableChannel(device, LMS_CH_TX, channel, true);

  printf("Using frequency %.6lf MHz\n", (frequency / 1.0e6));

  int setLOFrequency = LMS_SetLOFrequency(device, LMS_CH_TX, channel, frequency);
  if(setLOFrequency)
  {
    printf("setLOFrequency(%lf)=%d(%s)" "\n", frequency, setLOFrequency, LMS_GetLastErrorMessage());
  }

  lms_range_t sampleRateRange;
  int getSampleRateRange = LMS_GetSampleRateRange(device, LMS_CH_TX, &sampleRateRange);
  printf("Sample Rate Max %f, Min %f, Step %f\n", sampleRateRange.max, sampleRateRange.min, sampleRateRange.step);

  if(getSampleRateRange)
  {
    printf("getSampleRateRange=%d(%s)" "\n", getSampleRateRange, LMS_GetLastErrorMessage());
  }

  printf("Set sample rate to %.6lf MS/s ...\n", (upsample *sampleRate / 1.0e6));
  int setSampleRate = LMS_SetSampleRate(device, upsample * sampleRate, 0);

  if(setSampleRate)
  {
    printf("setSampleRate=%d(%s)" "\n", setSampleRate, LMS_GetLastErrorMessage());
  }
  double actualHostSampleRate = 0.0;
  double actualRFSampleRate = 0.0;
  int getSampleRate = LMS_GetSampleRate(device, LMS_CH_TX, channel, &actualHostSampleRate, &actualRFSampleRate);
  if(getSampleRate)
  {
    printf("getSampleRate=%d(%s)" "\n", getSampleRate, LMS_GetLastErrorMessage());
  }
  else
  {
    printf("Samplerate Host: %.6lf MS/s -> RF: %.6lf MS/s" "\n", (actualHostSampleRate / 1.0e6), (actualRFSampleRate / 1.0e6));
  }

  printf("Calibrating ..." "\n");
  int calibrate = LMS_Calibrate(device, LMS_CH_TX, channel, rfBandwidth, 0);
  if(calibrate)
  {
    printf("calibrate=%d(%s)" "\n", calibrate, LMS_GetLastErrorMessage());
  }

  printf("Setup TX stream ..." "\n");
  lms_stream_t tx_stream = {.channel = channel, .fifoSize = 1024*1024, .throughputVsLatency = 0.5, .isTx = true, .dataFmt = LMS_FMT_I12};
  int setupStream = LMS_SetupStream(device, &tx_stream);
  if(setupStream)
  {
    printf("setupStream=%d(%s)" "\n", setupStream, LMS_GetLastErrorMessage());
  }

  int nSampleSize = 0;
  int nSamples = (int)sampleRate / 100;  // 10 ms of samples
  printf("nSamples = %d\n", nSamples);

  struct s8iq_sample_s
  {
    signed char i;
    signed char q;
  };
  struct s16iq_sample_s
  {
    int16_t i;
    int16_t q;
  };
  struct f32iq_sample_s
  {
    float i;
    float q;
  };

  if(input_format == WAVFORMAT_I8)
  {
    nSampleSize = sizeof(struct s8iq_sample_s);
  }
  else if(input_format == WAVFORMAT_I16)
  {
    nSampleSize = sizeof(struct s16iq_sample_s);
  }
  else if(input_format == WAVFORMAT_F32)
  {
    nSampleSize = sizeof(struct f32iq_sample_s);
  }

  void *fileSamples = malloc(nSampleSize * nSamples);

  struct s16iq_sample_s *sdrSamples = malloc(sizeof(struct s16iq_sample_s) * upsample * nSamples);
  int  nSamplesRead;
    //int *dummy;

  nSamplesRead = fread(fileSamples, nSampleSize, nSamples, fileptr);
  int TotalSamplesRead = nSamplesRead;
  printf("Read in %d samples\n", nSamplesRead);

  // Completed Lime Setup.  Pause here if required

  int loop = 0;
  int sendStream;
  LMS_StartStream(&tx_stream);

    while((0 == exit_requested) && (nSamplesRead > 0))
    {
      // Scale input data
      if(input_format == WAVFORMAT_I8)
      {
        // Up-Scale to 12-bit
        for(i = 0; i < nSamples; i++)
        {
          sdrSamples[i].i = (((struct s8iq_sample_s*)fileSamples)[i].i << 4);
          sdrSamples[i].q = (((struct s8iq_sample_s*)fileSamples)[i].q << 4);
        }
      }
      else if(input_format == WAVFORMAT_I16)
      {
        // Copy samples
        for(i = 0; i < nSamples; i++)
        {
          sdrSamples[i * upsample].i = scale * (((struct s16iq_sample_s*)fileSamples)[i].i >> 4);
          sdrSamples[i * upsample].q = scale * (((struct s16iq_sample_s*)fileSamples)[i].q >> 4);

          // Add zero samples for up-sampling
          for (int j = 1; j <= (upsample - 1); j++)
          {
            sdrSamples[i * upsample + j].i = 0; 
            sdrSamples[i * upsample + j].q = 0; 
          }
        }
      }
      else if(input_format == WAVFORMAT_F32)
      {
        // Scale from normalised to 12-bit
        for(i = 0; i < nSamples; i++)
        {
          sdrSamples[i].i = (int16_t)(((struct f32iq_sample_s*)fileSamples)[i].i * 2048.0);
          sdrSamples[i].q = (int16_t)(((struct f32iq_sample_s*)fileSamples)[i].q * 2048.0);
        }
      }

      // Check that samples are in range after scaling
      for(i = 0; i < nSamples; i++)
      {
        if ((sdrSamples[i].i > 2047) || (sdrSamples[i].i < -2046)
         || (sdrSamples[i].q > 2047) || (sdrSamples[i].q < -2046))
        {
          printf("Sample Out of Range i = %d, sample q = %d\n", sdrSamples[i].i, sdrSamples[i].q);
        }
      }


      // Output to Lime
      sendStream = LMS_SendStream(&tx_stream, sdrSamples, upsample * nSamples, NULL, 1000);
      if(sendStream < 0)
      {
        printf("sendStream error %d(%s)" "\n", sendStream, LMS_GetLastErrorMessage());
      }

      loop++;

      //Read next data
      nSamplesRead = fread(fileSamples, nSampleSize, nSamples, fileptr);
      TotalSamplesRead = TotalSamplesRead + nSamplesRead;
    }

  // End of TX, so shut everything down nicely

  fclose(fileptr);

  LMS_StopStream(&tx_stream);
  LMS_DestroyStream(device, &tx_stream);

  LMS_Reset(device);
  LMS_Init(device);
  int nb_channel = LMS_GetNumChannels(device, LMS_CH_RX);
  for (i = 0; i < nb_channel; i++)
  {
    LMS_EnableChannel(device, LMS_CH_RX, i, false);
  }
  nb_channel = LMS_GetNumChannels(device, LMS_CH_TX);
  for( i = 0; i < nb_channel; i++ )
  {
    LMS_EnableChannel(device, LMS_CH_TX, i, false);
  }
  LMS_Close(device);

  return(0);
}