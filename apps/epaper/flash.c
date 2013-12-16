#include <wixel.h>
#include <spi0_master.h>

#include "common.h"
#include "flash.h"

// maximum bytes that can be written by one write command
#define FLASH_PAGE_SIZE 128

// to shift sector number (0..FLASH_SECTOR_COUNT) to an address for erase
#define FLASH_SECTOR_SHIFT 12

// erase size is one sector
#define FLASH_SECTOR_SIZE 4096

// total available sectors
#define FLASH_SECTOR_COUNT 256


// FLASH MX25V8005 8Mbit flash chip command set (50MHz max clock)
enum {
    FLASH_WREN = 0x06,
    FLASH_WRDI = 0x04,
    FLASH_RDID = 0x9f,
    FLASH_RDSR = 0x05,
    FLASH_WRSR = 0x01,
    FLASH_READ = 0x03,       // read at half frequency
    FLASH_FAST_READ = 0x0b,  // read at full frequency
    FLASH_SE = 0x20,
    FLASH_BE = 0x52,
    FLASH_CE = 0x60,
    FLASH_PP = 0x02,
    FLASH_DP = 0xb9,
    FLASH_RDP = 0xab,
    FLASH_REMS = 0x90,
    FLASH_NOP = 0xff,

    // status register bits
    FLASH_WIP = 0x01,
    FLASH_WEL = 0x02,
    FLASH_BP0 = 0x04,
    FLASH_BP1 = 0x08,
    FLASH_BP2 = 0x10
};

void flash_spi_setup() {
    setDigitalOutput(PIN_SSEL, HIGH);
    spi0MasterSendByte(FLASH_NOP);
    delayMicroseconds(50);
    setDigitalOutput(PIN_SSEL, LOW);
    delayMicroseconds(10);
}

void flash_spi_teardown() {
    delayMicroseconds(50);
    setDigitalOutput(PIN_SSEL, HIGH);
    spi0MasterSendByte(0);
}

void flash_info(uint8_t *maufacturer, uint16_t *device) {
    uint8_t id_high, id_low;
    flash_spi_setup();
    spi0MasterSendByte(FLASH_RDID);
    *maufacturer = spi0MasterReceiveByte();
    id_high = spi0MasterReceiveByte();
    id_low = spi0MasterReceiveByte();
    *device = (id_high << 8) | id_low;
    flash_spi_teardown();
}

void flash_read(uint8_t XDATA *buffer, uint32_t address, uint16_t length) {
    flash_spi_setup();
    spi0MasterSendByte(FLASH_FAST_READ);
    spi0MasterSendByte(address >> 16);
    spi0MasterSendByte(address >> 8);
    spi0MasterSendByte(address);
    spi0MasterSendByte(FLASH_NOP); // read dummy byte
    for (; length != 0; --length) {
        *buffer++ = spi0MasterSendByte(FLASH_NOP);
    }
    flash_spi_teardown();
}

uint8_t flash_is_busy() {
    uint8_t busy;
    flash_spi_setup();
    spi0MasterSendByte(FLASH_RDSR);
    busy = 0 != (FLASH_WIP & spi0MasterSendByte(0xff));
    flash_spi_teardown();
    return busy;
}


void flash_write_enable() {
    while (flash_is_busy());
    flash_spi_setup();
    spi0MasterSendByte(FLASH_WREN);
    flash_spi_teardown();
}



void flash_write_disable(void) {
    while (flash_is_busy());
    flash_spi_setup();
    spi0MasterSendByte(FLASH_WRDI);
    flash_spi_teardown();
}


void flash_write(uint32_t address, const uint8_t XDATA *buffer, uint16_t length) {
    while (flash_is_busy());

    flash_spi_setup();
    spi0MasterSendByte(FLASH_PP);
    spi0MasterSendByte(address >> 16);
    spi0MasterSendByte(address >> 8);
    spi0MasterSendByte(address);

    while(length--) {
        spi0MasterSendByte(*buffer++);
    }

    flash_spi_teardown();
}


void flash_sector_erase(uint32_t address) {
    while (flash_is_busy());

    flash_spi_setup();
    spi0MasterSendByte(FLASH_SE);
    spi0MasterSendByte(address >> 16);
    spi0MasterSendByte(address >> 8);
    spi0MasterSendByte(address);
    flash_spi_teardown();
}
