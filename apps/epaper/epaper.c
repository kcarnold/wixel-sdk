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
#include "mma.h"

// i2c constants and addresses
#define LM75B_I2C_ADDR 0x49
#define LM75B_REG_TEMP 0x00
#define LM75B_REG_CONF 0x01


// Comm services
#define anyRxAvailable() (radioComRxAvailable() || usbComRxAvailable())
#define getReceivedByte() (radioComRxAvailable() ? radioComRxReceiveByte() : usbComRxReceiveByte())
#define comServices() do { boardService(); radioComTxService(); usbComService(); } while (0)

#define MIN_AWAKE_INTERVAL 5000

uint16_t sleep_interval_sec = 10;

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
    comServices();
    spi_go_max_speed(0);
}

void show_image(uint8_t image_idx, uint8_t compensate) {
    uint32_t address = image_idx;
    address <<= (12 + 1); // each image takes up two sectors.
    epd_begin();
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_compensate : EPD_inverse, 1, 0, 0);
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_white : EPD_normal, 2, 0, 0);
    epd_end();
}

void cmdImage(uint8_t compensate) {
    show_image(read_byte_hex(), compensate);
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


void cmdAccelerometer() {
    uint8_t pulse_src = mmaReadRegister(MMA_PULSE_SRC);
    printf("WHOAMI: %x\r\n", mmaReadRegister(0x0D));
    printf("PULSE_SRC: %x\r\n", pulse_src);


  if ((pulse_src & 0x10)==0x10) // If AxX bit is set
  {
    if ((pulse_src & 0x08)==0x08) // If DPE (double puls) bit is set
      printf(" Double Tap (2) on X"); // tabbing here for visibility
    else
      printf("Single (1) tap on X");

    if ((pulse_src & 0x01)==0x01) // If PoIX is set
      printf(" +\r\n");
    else
      printf(" -\r\n");
  }
  if ((pulse_src & 0x20)==0x20) // If AxY bit is set
  {
    if ((pulse_src & 0x08)==0x08) // If DPE (double puls) bit is set
      printf(" Double Tap (2) on Y");
    else
      printf("Single (1) tap on Y");

    if ((pulse_src & 0x02)==0x02) // If PoIY is set
      printf(" +\r\n");
    else
      printf(" -\r\n");
  }
  if ((pulse_src & 0x40)==0x40) // If AxZ bit is set
  {
    if ((pulse_src & 0x08)==0x08) // If DPE (double puls) bit is set
      printf(" Double Tap (2) on Z");
    else
      printf("Single (1) tap on Z");
    if ((pulse_src & 0x04)==0x04) // If PoIZ is set
      printf(" +\r\n");
    else
      printf(" -\r\n");
  }
}

void shutdownLM75B() {
    // set the LM75B to shutdown mode to save power.
    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(LM75B_I2C_ADDR));
    i2cWriteByte(LM75B_REG_CONF);
    i2cWriteByte(0x01); // shutdown=1, else default.
    i2cStop();
}

void cmdReadLM75BConfig() {
    i2cStart();
    i2cWriteByte(ADDR_FOR_WRITE(LM75B_I2C_ADDR));
    i2cWriteByte(LM75B_REG_CONF);
    i2cStart();
    i2cWriteByte(ADDR_FOR_READ(LM75B_I2C_ADDR));
    printf("%x", i2cReadByte(1));
    i2cStop();
}


void power_off_epd() {
    setDigitalInput(PIN_EPD_3V3, HIGH_IMPEDANCE);
}

void power_on_epd() {
    setDigitalOutput(PIN_EPD_3V3, 1);
    delayMs(100);
    shutdownLM75B();
}


void goToSleep(uint16_t duration_sec) {
    power_off_epd();
    sleepMode2(duration_sec);
    power_on_epd();
}


#define MAX_SEQUENCE_COMMANDS 32
#define EVENT_TIMER  1
#define EVENT_TAP    2
uint16_t cur_image = 0;
uint8_t num_seq_commands = 0;
struct {
    uint8_t event_type;
    uint8_t event_arg;
    uint8_t src_image;
    uint8_t tgt_image;
} XDATA sequence_commands[MAX_SEQUENCE_COMMANDS];

uint8_t get_next_image(uint8_t event_type, uint8_t event_arg) {
    uint8_t i;
    for (i=0; i<num_seq_commands; i++) {
        if (sequence_commands[i].src_image != cur_image) continue;
        if (sequence_commands[i].event_type == event_type) {
            if (event_type == EVENT_TIMER) {
                return sequence_commands[i].tgt_image;
            }
        }
    }
    return 0; // default image, in case something goes wrong.
}

void updateDisplay() {
    // Assume for the moment that the cause of wake-up was always the timer.
    cur_image = get_next_image(EVENT_TIMER, 0);
    show_image(cur_image, 0);
}

// Updates the list of sequence commands.
void cmdLoadSeqCommands() {
    uint8_t i;
    num_seq_commands = getchar();
    putchar('>'); // Go!
    for (i=0; i<num_seq_commands; i++) {
        sequence_commands[i].event_type = getchar();
        sequence_commands[i].event_arg = getchar();
        sequence_commands[i].src_image = getchar();
        sequence_commands[i].tgt_image = getchar();
        putchar('.');
    }
    putchar('<'); // done.
}

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
    case 'L': cmdLoadSeqCommands(); break;
    case 's': goToSleep(read_byte_hex()); break;
    case 'a': cmdAccelerometer(); break;
    case 't': cmdTemp(); break;
    case 'T': cmdReadLM75BConfig(); break;
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

    power_on_epd();

    initMMA8452(
        2, // SCALE: Sets full-scale range to +/-2, 4, or 8g. Used to calc real g values.
        2 // dataRate: 0=800Hz, 1=400, 2=200, 3=100, 4=50, 5=12.5, 6=6.25, 7=1.56
        );

    {
        // Temporarily load in some sequence commands.
        uint8_t i;
        num_seq_commands = 5;
        for (i=0; i<5; i++) {
            sequence_commands[i].event_type = EVENT_TIMER;
            sequence_commands[i].src_image = (i+1) % 5;
            sequence_commands[i].tgt_image = i;
        }
    }

    while(1)
    {
        uint32_t woke_up_at = getMs();
        putchar('!');
        comServices();
        updateDisplay();
        while (getMs() - woke_up_at < MIN_AWAKE_INTERVAL) {
            comServices();
            remoteControlService();
        }
        goToSleep(sleep_interval_sec);
    }
}
