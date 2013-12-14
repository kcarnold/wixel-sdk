#include <wixel.h>

#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

#include <radio_com.h>
#include <radio_link.h>

#include <spi0_master.h>

// More conventional type definitions
typedef uint8 uint8_t;
typedef uint16 uint16_t;

// Pin assignments
#define PIN_BORDER     0
#define PIN_BUSY       1
#define PIN_MISO       2 // these depend on the location of USART0.
#define PIN_MOSI       3
#define PIN_SCK        5
#define PIN_SSEL       4 // though, interestingly, this is movable.
#define PIN_RESET     12 // (port 1)
#define PIN_PWM       13 // Timer 3 channel 0
#define PIN_DISCHARGE 21
#define PIN_PWR       22


/**** FLASH ***/
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

// Supported chip (on the EA board)
#define FLASH_MFG (0xEF)
#define FLASH_ID  (0x4014)

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

/*** /FLASH */

void usbToRadioService()
{
    while(usbComRxAvailable() && radioComTxAvailable())
    {
        radioComTxSendByte(usbComRxReceiveByte());
    }

    while(radioComRxAvailable() && usbComTxAvailable())
    {
        usbComTxSendByte(radioComRxReceiveByte());
    }
}

// Called by printf.
void putchar(char c)
{
    while (!usbComTxAvailable()) usbComService();
    usbComTxSendByte(c);
}

uint8 XDATA spiTxBuffer[10];
uint8 XDATA spiRxBuffer[10];

void main()
{
    uint8_t flash_manufacturer;
    uint16_t flash_device;
    uint16_t timeOfLastMsg = 0;


    systemInit();
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

    radioComRxEnforceOrdering = 1;
    radioComInit();

    //delayMs(5000);
//
    flash_info(&flash_manufacturer, &flash_device);
    flash_info(&flash_manufacturer, &flash_device);
    if (flash_manufacturer == FLASH_MFG && flash_device == FLASH_ID) {
        printf("Flash good.\n");
    } else {
        printf("Flash what?\n");
    }
    printf("%x, %hx\n", flash_manufacturer, flash_device);


    while(1)
    {
        uint16 now;
        boardService();
        radioComTxService();
        usbComService();

        usbShowStatusWithGreenLed();

        now = (uint16)getMs();
        if ((now & 0x3FF) <= 20) {
            setDigitalOutput(PIN_PWR, 1);
        } else {
            setDigitalOutput(PIN_PWR, 0);
        }
        if (now - timeOfLastMsg > 1000) {
            printf("-\n");
            timeOfLastMsg = now;

        }

        //usbToRadioService();
        //while (spi0MasterBusy());
        //spi0MasterTransfer(spiTxBuffer, spiRxBuffer, 2);
    }
}
