/**************************************************************************//***
 *  @file    pe43713.c
 *  @author  Ray M0DHP
 *  @date    2018-02-16  
 *  @version 0.3
*******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <wiringPi.h>
#include "pe43713.h"


/***************************************************************************//**
 *  @brief Sets the attenuation level on the PE43713 attenuator
 *  
 *  @param level Attenuation level in dB (positive float) e.g. "12.5"
 *
 *  @return 1 if level is out of bounds, 0 otherwise
*******************************************************************************/

int pe43713_set_level(float level)
{
  if (level >= PE43713_MIN_ATTENUATION && level <= PE43713_MAX_ATTENUATION)
  {
    uint8_t integer_level = round(level * 4.0);
    printf("DEBUG: setting %s attenuation level to %.2f dB\n", PE43713_DISPLAY_NAME, integer_level / 4.0);

    // Nominate pins using WiringPi numbers

    const uint8_t LE_43713_GPIO = 16;
    const uint8_t CLK_43713_GPIO = 21;
    const uint8_t DATA_43713_GPIO = 22;

    // Set all nominated pins to outputs

    pinMode(LE_43713_GPIO, OUTPUT);
    pinMode(CLK_43713_GPIO, OUTPUT);
    pinMode(DATA_43713_GPIO, OUTPUT);

    // Set idle conditions

    usleep(PE43713_SLEEP);
    digitalWrite(LE_43713_GPIO, LOW);
    digitalWrite(CLK_43713_GPIO, LOW);

    // Shift out data 

    uint8_t bit;
    for (bit = 0; bit <= 7; bit++)
    {
      digitalWrite(DATA_43713_GPIO, (integer_level >> bit) & 0x01);
      usleep(PE43713_SLEEP);
      digitalWrite(CLK_43713_GPIO, HIGH);
      usleep(PE43713_SLEEP);
      digitalWrite(CLK_43713_GPIO, LOW);
      usleep(PE43713_SLEEP);
    }

    // Shift out address


    for (bit = 0; bit <= 7; bit++)
    {
      digitalWrite(DATA_43713_GPIO, (PE43713_ADDRESS >> bit) & 0x01);
      usleep(PE43713_SLEEP);
      digitalWrite(CLK_43713_GPIO, HIGH);
      usleep(PE43713_SLEEP);
      digitalWrite(CLK_43713_GPIO, LOW);
      usleep(PE43713_SLEEP);
    }

    // Latch data and address

    digitalWrite(LE_43713_GPIO, HIGH);
    usleep(PE43713_SLEEP);
    digitalWrite(LE_43713_GPIO, LOW);
    usleep(PE43713_SLEEP);

    return 0;
  }
  else
  {
    printf("ERROR: level %.3f dB is outside limits for %s attenuator\n", level, PE43713_DISPLAY_NAME);
    return 1;
  }
}

