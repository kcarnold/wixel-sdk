#include <wixel.h>
#include <sleep.h>

#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

#include <radio_com.h>
#include <radio_link.h>

#include <spi0_master.h>

#include "common.h"
#include "flash.h"
#include "epd.h"

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
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_compensate : EPD_inverse, 0, 0);
    epd_frame_cb(address, epd_flash_read, compensate ? EPD_white : EPD_normal, 0, 0);
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
    flash_sector_erase(address + 1);
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
    case 's': sleepMode2(5); break;
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

    usbInit();

    // Don't "enforce ordering" because we're not going to use the "control signals".
    radioComRxEnforceOrdering = 0;
    radioComInit();

    while(1)
    {
        boardService();
        radioComTxService();
        usbComService();

        remoteControlService();
    }
}
