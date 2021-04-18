#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "temperature.h"

bool temperature_cpu(float *result)
{
  FILE *temperatureFile;
  double T;

  temperatureFile = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

  if(temperatureFile != NULL)
  {
    fscanf(temperatureFile, "%lf", &T);

    fclose(temperatureFile);
    
    *result = T/1000.0;
    return true;
  }

  return false;
}

