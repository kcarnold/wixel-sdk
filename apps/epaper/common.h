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


// More conventional type definitions
typedef uint8 uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;
