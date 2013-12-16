void flash_spi_setup();
void flash_spi_teardown();
void flash_info(uint8 *maufacturer, uint16 *device);
void flash_read(uint8_t XDATA *buffer, uint32_t address, uint16_t length);
void flash_write_enable();
void flash_write_disable(void);
void flash_write(uint32_t address, const uint8_t XDATA *buffer, uint16_t length);
void flash_sector_erase(uint32_t address);

// Supported chip (on the EA board)
#define FLASH_MFG (0xEF)
#define FLASH_ID  (0x4014)

