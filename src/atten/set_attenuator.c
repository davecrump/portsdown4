/**************************************************************************//***
 *  @file    set_attenuator.c
 *  @author  Ray M0DHP
 *  @date    2017-12-22  
 *  @version 0.1
 *  
 *  @brief Sets the attenuation level on the external attenuator
 *  
 *  @param attenuator-type "PE4312", "PE43713" or "HMC1119"
 *  @param level Attenuation level in dB (positive float) e.g. "12.5"
 *
 *  @return 1 if invalid attenuator type or level is out of bounds, 0 otherwise
*******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wiringPi.h>
#include "pe4312.h"
#include "pe43713.h"
#include "hmc1119.h"


int main(int argc, char *argv[])
{
  // Kick Wiring Pi into life
  if (wiringPiSetup() == -1);

  // Check that two parameters have been supplied
  if (argc != 3)
  {
    printf("ERROR: specify attenuator type and attenuation level\n");
    return 1;
  }
  // Check attenuator type
  else if (strcmp(argv[1], "PE4312") == 0)
  {
    int rc = pe4312_set_level(atof(argv[2]));
    printf("DEBUG: rc = %d\n", rc);
    return rc;
  }
  else if (strcmp(argv[1], "PE43713") == 0)
  {
    int rc = pe43713_set_level(atof(argv[2]));
    printf("DEBUG: rc = %d\n", rc);
    return rc;
  }
  else if (strcmp(argv[1], "HMC1119") == 0)
  {
    int rc = hmc1119_set_level(atof(argv[2]));
    printf("DEBUG: rc = %d\n", rc);
    return rc;
  }
  else
  {
    printf("ERROR: attenuator type '%s' not recognised\n", argv[1]);
    return 1;
  }
}

