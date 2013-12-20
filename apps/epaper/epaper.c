#include <wixel.h>
#include <sleep.h>

#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

#include <radio_com.h>
#include <radio_link.h>

#include <spi0_master.h>
#include <i2c.h>

#include "common.h"
#include "flash.h"
#include "epd.h"

// i2c constants and addresses
#define ADDR_FOR_WRITE(addr) ((addr) << 1)
#define ADDR_FOR_READ(addr) (((addr) << 1) | 0x01)

#define LM75B_I2C_ADDR 0x49
#define LM75B_REG_TEMP 0x00
#define LM75B_REG_CONF 0x01

#define MMA8452_ADDRESS 0x1D
#define MMA_PULSE_SRC 0x22


void spi_go_max_speed(uint8_t need_receive) {
    uint8_t baudE = need_receive ? 17 : 19; // F/8 if need to receive, else F/2
    U0UCR |= (1<<7); // U0UCR.FLUSH = 1
    U0BAUD = 0;
    U0GCR &= 0xE0; // preserve CPOL, CPHA, ORDER (7:5)
    U0GCR |= baudE; // UNGCR.BAUD_E (4:0)
}


void cmdFlashInfo() {
    uint8_t flash_manufacturer;
    uint16_t flash_device;
    flash_info(&flash_manufacturer, &flash_device);
    flash_info(&flash_manufacturer, &flash_device);
    if (flash_manufacturer == FLASH_MFG && flash_device == FLASH_ID) {
        printf("Flash good: ");
    } else {
        printf("Flash what: ");
    }
    printf("%x, %hx\n", flash_manufacturer, flash_device);
}

uint8_t read_nibble_hex() {
    uint8_t c = getchar();
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

uint8_t read_byte_hex() {
    uint8_t result = read_nibble_hex() << 4;
    result |= read_nibble_hex();
    return result;
}

uint32_t read_uint32_hex_msb() {
    uint32_t result = 0;
    result = read_byte_hex();
    result <<= 8; result |= read_byte_hex();
    result <<= 8; result |= read_byte_hex();
    result <<= 8; result |= read_byte_hex();
    return result;
}

void cmdFlashRead() {
    uint8_t i;
    uint32_t address = read_uint32_hex_msb();
    uint8_t XDATA buffer[16];
    printf("%lx\n", address);
    flash_read(buffer, address, 16);
    for (i=0; i<16; i++) {
        printf("%02x", buffer[i]);
    }
    putchar('\n');
}

void cmdWhite() {
    epd_begin();
    epd_clear();
    epd_end();
}

void epd_flash_read(uint8_t XDATA *buffer, uint32_t address, uint16_t length) __reentrant {
    setDigitalOutput(PIN_PWR, 0); // switch SSEL mux to flash (yes this removes power from the EPD... I hope that's ok.)
    spi_go_max_speed(1);
    flash_read(buffer, address, length);
    setDigitalOutput(PIN_PWR, 1);
    spi_go_max_speed(0);
}

void cmdImage(uint8_t compensate) {
    uint32_t address = read_byte_hex();
    address <<= 12;
    epd_begin();
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_compensate : EPD_inverse, 1, 0, 0);
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_white : EPD_normal, 2, 0, 0);
    epd_end();
}

void cmdFlashErase() {
    uint32_t sector = read_byte_hex();
    flash_write_enable();
    flash_sector_erase(sector << 12);
    flash_write_disable();
}

void cmdUpload() {
    uint32_t address = read_byte_hex();
    uint16_t bytes = 264L * 176 / 8;
    uint8_t XDATA buffer[8];
    address <<= 12;
    spi_go_max_speed(1);
    flash_write_enable();
    flash_sector_erase(address);
    delayMs(1);
    flash_write_enable();
    flash_sector_erase(address + (1<<12));
    delayMs(1);
    putchar('>'); // Go!
    while (bytes) {
        uint8_t bufSize = 0;
        while (bufSize < 8) {
            buffer[bufSize] = getchar();
            bufSize++;
        }
        flash_write_enable();
        flash_write(address, buffer, bufSize);
        putchar('.'); // block done.
        bytes -= bufSize;
        address += bufSize;
    }
    flash_spi_teardown();
    flash_write_disable();
    putchar('<');
}

void cmdTemp() {
    uint16_t temp = 0;
    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(LM75B_I2C_ADDR));
    i2cWriteByte(LM75B_REG_TEMP);
    i2cStart();
    i2cWriteByte(ADDR_FOR_READ(LM75B_I2C_ADDR));
    temp = i2cReadByte(0);
    temp <<= 8;
    temp |= i2cReadByte(1);
    i2cStop();
    printf("Temp: %hd\r\n", temp);
}

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

void cmdAccelerometer() {
    printf("WHOAMI: %x\r\n", mmaReadRegister(0x0D));
    printf("PULSE_SRC: %x\r\n", mmaReadRegister(MMA_PULSE_SRC));
}


#define anyRxAvailable() (radioComRxAvailable() || usbComRxAvailable())
#define getReceivedByte() (radioComRxAvailable() ? radioComRxReceiveByte() : usbComRxReceiveByte())
#define comServices() do { boardService(); radioComTxService(); usbComService(); } while (0)

void remoteControlService() {
    if (!anyRxAvailable()) return;

    switch(getReceivedByte()) {
    case 'f': cmdFlashInfo(); break;
    case 'd': cmdFlashRead(); break;
    case 'e': cmdFlashErase(); break;
    case 'u': cmdUpload(); break;
    case 'w': cmdWhite(); break;
    case 'i': cmdImage(0); break;
    case 'r': cmdImage(1); break;
    case 's': sleepMode2(read_byte_hex()); break;
    case 'a': cmdAccelerometer(); break;
    case 't': cmdTemp(); break;
    default: printf("? ");
    }
}

// Called by printf.
void putchar(char c)
{
    while (!radioComTxAvailable()) radioComTxService();
    if (usbComTxAvailable()) usbComTxSendByte(c);
    if (radioComTxAvailable()) radioComTxSendByte(c);
}

// Called by scanf.
char getchar() {
    while (!anyRxAvailable()) comServices();
    return getReceivedByte();
}

void shutdownLM75B() {
    // set the LM75B to shutdown mode to save power.
    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(LM75B_I2C_ADDR));
    i2cWriteByte(LM75B_REG_CONF);
    i2cWriteByte(0x01); // shutdown=1, else default.
    i2cStop();

    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(LM75B_I2C_ADDR));
    i2cWriteByte(LM75B_REG_CONF);
    i2cStart();
    i2cWriteByte(ADDR_FOR_READ(LM75B_I2C_ADDR));
    printf("Config now: %x\r\n", i2cReadByte(1));
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

void main()
{
    systemInit();
    sleepInit();

    // SPI
    spi0MasterInit();
    spi0MasterSetFrequency(3000000);
    spi0MasterSetClockPhase(SPI_PHASE_EDGE_LEADING);
    spi0MasterSetClockPolarity(SPI_POLARITY_IDLE_LOW);
    spi0MasterSetBitOrder(SPI_BIT_ORDER_MSB_FIRST);

    setDigitalOutput(PIN_SSEL, LOW);
    // For the e-paper board, we also need to drive PWR to switch to the flash chip.
    setDigitalOutput(PIN_PWR, LOW);

    // Other e-ink GPIOs
    setDigitalOutput(PIN_BORDER, LOW);
    setDigitalOutput(PIN_RESET, LOW);
    setDigitalOutput(PIN_DISCHARGE, LOW);

    // I2C
    i2cPinScl = PIN_SCL;
    i2cPinSda = PIN_SDA;
    i2cSetFrequency(100);
    i2cSetTimeout(10);

    // Let's keep USB, to make programming easier. TODO: does this increase power consumption any?
    usbInit();

    // Don't "enforce ordering" because we're not going to use the "control signals".
    radioComRxEnforceOrdering = 0;
    radioComInit();

    shutdownLM75B();

    initMMA8452(
        2, // SCALE: Sets full-scale range to +/-2, 4, or 8g. Used to calc real g values.
        0 // dataRate: 0=800Hz, 1=400, 2=200, 3=100, 4=50, 5=12.5, 6=6.25, 7=1.56
        );

    while(1)
    {
        boardService();
        radioComTxService();
        usbComService();

        remoteControlService();
    }
}
