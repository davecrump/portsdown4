/**************************************************************************//***
 *  @file    pe4312.c
 *  @author  Ray M0DHP
 *  @date    2018-02-16  
 *  @version 0.3
*******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <wiringPi.h>
#include "pe4312.h"


/***************************************************************************//**
 *  @brief Sets the attenuation level on the PE4312 attenuator
 *  
 *  @param level Attenuation level in dB (positive float) e.g. "12.5"
 *
 *  @return 1 if level is out of bounds, 0 otherwise
*******************************************************************************/

int pe4312_set_level(float level)
{
  if (level >= PE4312_MIN_ATTENUATION && level <= PE4312_MAX_ATTENUATION)
  {
    uint8_t integer_level = round(level * 2.0);
    printf("DEBUG: setting %s attenuation level to %.1f dB\n", PE4312_DISPLAY_NAME, integer_level / 2.0);

    // Nominate pins using WiringPi numbers

    const uint8_t LE_4312_GPIO = 16;
    const uint8_t CLK_4312_GPIO = 21;
    const uint8_t DATA_4312_GPIO = 22;

    // Set all nominated pins to outputs


    pinMode(LE_4312_GPIO, OUTPUT);
    pinMode(CLK_4312_GPIO, OUTPUT);
    pinMode(DATA_4312_GPIO, OUTPUT);

    // Set idle conditions

    usleep(PE4312_SLEEP);
    digitalWrite(LE_4312_GPIO, LOW);
    digitalWrite(CLK_4312_GPIO, LOW);

    // Shift out data 

    int8_t bit;
    for (bit = 5; bit >= 0; bit--)
    {
      digitalWrite(DATA_4312_GPIO, (integer_level >> bit) & 0x01);
      usleep(PE4312_SLEEP);
      digitalWrite(CLK_4312_GPIO, HIGH);
      usleep(PE4312_SLEEP);
      digitalWrite(CLK_4312_GPIO, LOW);
      usleep(PE4312_SLEEP);
    }
    digitalWrite(LE_4312_GPIO, HIGH);
    usleep(PE4312_SLEEP);
    digitalWrite(LE_4312_GPIO, LOW);
    usleep(PE4312_SLEEP);
    return 0;
  }
  else
  {
    printf("ERROR: level %.3f dB is outside limits for %s attenuator\n", level, PE4312_DISPLAY_NAME);
    return 1;
  }
}

