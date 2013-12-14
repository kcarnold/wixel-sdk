#include <wixel.h>

#include <usb.h>
#include <usb_com.h>

#include <radio_com.h>
#include <radio_link.h>

#include <gpio.h>
#include <spi0_master.h>

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


uint8 XDATA spiTxBuffer[10];
uint8 XDATA spiRxBuffer[10];

void main()
{
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

    while(1)
    {
        boardService();
        radioComTxService();
        usbComService();
        usbToRadioService();
        while (spi0MasterBusy());
        spi0MasterTransfer(spiTxBuffer, spiRxBuffer, 2);
    }
}
