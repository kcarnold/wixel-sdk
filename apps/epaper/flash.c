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
}

void flash_spi_teardown() {
    delayMicroseconds(50);
    setDigitalOutput(PIN_SSEL, HIGH);
}

void flash_info(uint8_t *maufacturer, uint16_t *device) {
    uint8_t id_high, id_low;
    flash_spi_setup();
    setDigitalOutput(PIN_SSEL, LOW);
    delayMicroseconds(10);
    spi0MasterSendByte(FLASH_RDID);
    *maufacturer = spi0MasterReceiveByte();
    id_high = spi0MasterReceiveByte();
    id_low = spi0MasterReceiveByte();
    *device = (id_high << 8) | id_low;
    flash_spi_teardown();
}
