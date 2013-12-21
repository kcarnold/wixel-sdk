#define MMA8452_ADDRESS 0x1D
#define MMA_PULSE_SRC 0x22


// Read a single byte from address and return it as a byte
uint8_t mmaReadRegister(uint8_t address);
void mmaWriteRegister(uint8_t address, uint8_t data);
void MMA8452Standby();
void MMA8452Active();
void initMMA8452(uint8_t fsr, uint8_t dataRate);