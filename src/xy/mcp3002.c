#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <wiringPi.h>
#include "mcp3002.h"

/***************************************************************************//**
 * @brief Reads the output value of an MCP3002 on a selected channel.
 *
 * @param data - channel to read
 *
 * @return 0 - 1023 adc output
*******************************************************************************/
uint32_t mcp3002_value(uint32_t channel)
{
  // Nominate pins using WiringPi numbers

  // LE   pin 5 wPi 9
  // CLK  pin 29 wPi 21
  // Data to ADC pin 31 wPi 22
  // Data from ADC pin 3 wPi 8


  uint8_t LE_3002_GPIO = 9;
  uint8_t CLK_3002_GPIO = 21;
  uint8_t DATA_TO_3002_GPIO = 22;
  uint8_t DATA_FM_3002_GPIO = 8;

  uint32_t halfclock = 0;
  uint32_t value = 0;
  uint16_t i;
  uint16_t bit;

  // Set up nominated pins
  pinMode(LE_3002_GPIO, OUTPUT);
  pinMode(CLK_3002_GPIO, OUTPUT);
  pinMode(DATA_TO_3002_GPIO, OUTPUT);
  pinMode(DATA_FM_3002_GPIO, INPUT);

  // /Set start conditions
  digitalWrite(LE_3002_GPIO, LOW);
  digitalWrite(CLK_3002_GPIO, LOW);
  digitalWrite(DATA_TO_3002_GPIO, LOW);

  // Send Start bit (High)
  digitalWrite(DATA_TO_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, LOW);

  // Send SGL/DIFF bit (High)
  digitalWrite(DATA_TO_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, LOW);

  // Send Channel select (low=0, High=1)
  if (channel == 0)
  {
    digitalWrite(DATA_TO_3002_GPIO, LOW);
  }
  else
  {
    digitalWrite(DATA_TO_3002_GPIO, HIGH);
  }
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, LOW);

  // Send MSB First Command (High)
  digitalWrite(DATA_TO_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, HIGH);
  //usleep(halfclock);
  digitalWrite(CLK_3002_GPIO, LOW);

  // Set data low for tidiness
  digitalWrite(DATA_TO_3002_GPIO, LOW);

  // Receive Null + 10 bits
  for (i = 0; i <11; i++)
  {
    // Shift data left.  0 on first iteration
    value <<= 1;

    //usleep(halfclock);
    digitalWrite(CLK_3002_GPIO, HIGH);

    bit = digitalRead(DATA_FM_3002_GPIO);
    value = value + bit;

    //usleep(halfclock);
    digitalWrite(CLK_3002_GPIO, LOW);
  }

  // Set LE high as now finished
  digitalWrite(LE_3002_GPIO, HIGH);
  //usleep(halfclock);

  return value;
}