#include <wixel.h>
#include <i2c.h>
#include "common.h"

#include "mma.h"

// Read a single byte from address and return it as a byte
uint8_t mmaReadRegister(uint8_t address) {
    uint8_t data;

    i2cStart();
    i2cWriteByte((MMA8452_ADDRESS<<1)); // Write
    i2cWriteByte(address); // Write register address
    i2cStart();

    i2cWriteByte((MMA8452_ADDRESS<<1)|0x01); // Read
    data = i2cReadByte(1); // Send a NACK -> conclusion of transfer.

    i2cStop();

    return data;
}

void mmaWriteRegister(uint8_t address, uint8_t data) {
    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(MMA8452_ADDRESS));
    i2cWriteByte(address);
    i2cWriteByte(data);
    i2cStop();
}

// Sets the MMA8452 to standby mode.
// It must be in standby to change most register settings
void MMA8452Standby()
{
  uint8_t c = mmaReadRegister(0x2A);
  mmaWriteRegister(0x2A, c & ~(0x01));
}

// Sets the MMA8452 to active mode.
// Needs to be in this mode to output data
void MMA8452Active()
{
  uint8_t c = mmaReadRegister(0x2A);
  mmaWriteRegister(0x2A, c | 0x01);
}

// Initialize the MMA8452 registers
// See the many application notes for more info on setting all of these registers:
// http://www.freescale.com/webapp/sps/site/prod_summary.jsp?code=MMA8452Q
// Feel free to modify any values, these are settings that work well for me.
void initMMA8452(uint8_t fsr, uint8_t dataRate)
{
  MMA8452Standby();  // Must be in standby to change registers

  // Set up the full scale range to 2, 4, or 8g.
  if ((fsr==2)||(fsr==4)||(fsr==8))
    mmaWriteRegister(0x0E, fsr >> 2);
  else
    mmaWriteRegister(0x0E, 0);

  // Setup the 3 data rate bits, from 0 to 7
  mmaWriteRegister(0x2A, mmaReadRegister(0x2A) & ~(0x38));
  if (dataRate <= 7)
    mmaWriteRegister(0x2A, mmaReadRegister(0x2A) | (dataRate << 3));

  // Set up portrait/landscap registers - 4 steps:
  // 1. Enable P/L
  // 2. Set the back/front angle trigger points (z-lock)
  // 3. Set the threshold/hysteresis angle
  // 4. Set the debouce rate
  // For more info check out this app note: http://cache.freescale.com/files/sensors/doc/app_note/AN4068.pdf
  mmaWriteRegister(0x11, 0x40);  // 1. Enable P/L
  mmaWriteRegister(0x13, 0x44);  // 2. 29deg z-lock (don't think this register is actually writable)
  mmaWriteRegister(0x14, 0x84);  // 3. 45deg thresh, 14deg hyst (don't think this register is writable either)
  mmaWriteRegister(0x12, 0x50);  // 4. debounce counter at 100ms (at 800 hz)

  /* Set up single and double tap - 5 steps:
   1. Set up single and/or double tap detection on each axis individually.
   2. Set the threshold - minimum required acceleration to cause a tap.
   3. Set the time limit - the maximum time that a tap can be above the threshold
   4. Set the pulse latency - the minimum required time between one pulse and the next
   5. Set the second pulse window - maximum allowed time between end of latency and start of second pulse
   for more info check out this app note: http://cache.freescale.com/files/sensors/doc/app_note/AN4072.pdf */
  mmaWriteRegister(0x21, 0x7F);  // 1. enable single/double taps on all axes
  // mmaWriteRegister(0x21, 0x55);  // 1. single taps only on all axes
  // mmaWriteRegister(0x21, 0x6A);  // 1. double taps only on all axes
  mmaWriteRegister(0x23, 0x20);  // 2. x thresh at 2g, multiply the value by 0.0625g/LSB to get the threshold
  mmaWriteRegister(0x24, 0x20);  // 2. y thresh at 2g, multiply the value by 0.0625g/LSB to get the threshold
  mmaWriteRegister(0x25, 0x08);  // 2. z thresh at .5g, multiply the value by 0.0625g/LSB to get the threshold
  mmaWriteRegister(0x26, 0x30);  // 3. 30ms time limit at 800Hz odr, this is very dependent on data rate, see the app note
  mmaWriteRegister(0x27, 0xA0);  // 4. 200ms (at 800Hz odr) between taps min, this also depends on the data rate
  mmaWriteRegister(0x28, 0xFF);  // 5. 318ms (max value) between taps max

  // Set up interrupt 1 and 2
  mmaWriteRegister(0x2C, 0x02);  // Active high, push-pull interrupts
  mmaWriteRegister(0x2D, 0x19);  // DRDY, P/L and tap ints enabled
  mmaWriteRegister(0x2E, 0x01);  // DRDY on INT1, P/L and taps on INT2

  MMA8452Active();  // Set to active to start reading
}
