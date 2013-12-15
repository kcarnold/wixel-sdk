void flash_spi_setup();
void flash_spi_teardown();
void flash_info(uint8 *maufacturer, uint16 *device);

// Supported chip (on the EA board)
#define FLASH_MFG (0xEF)
#define FLASH_ID  (0x4014)

