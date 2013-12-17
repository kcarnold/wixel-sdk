#include <wixel.h>
#include "common.h"
#include "spi0_master.h"

#include "epd.h"

typedef enum {
    EPD_1_44,        // 128 x 96
    EPD_2_0,         // 200 x 96
    EPD_2_7          // 264 x 176
} EPD_size;

EPD_size CODE epd_size = EPD_2_7;

uint8_t epd_repeat_count = 2;

uint16_t CODE epd_stage_time = 630; // milliseconds
uint16_t CODE epd_lines_per_display = 176;
uint16_t CODE epd_dots_per_line = 264;
uint16_t CODE epd_bytes_per_line = 264 / 8;
uint16_t CODE epd_bytes_per_scan = 176 / 4;
uint16_t CODE epd_filler = 1;
uint8_t CODE epd_channel_select[] = {0x00, 0x00, 0x00, 0x7f, 0xff, 0xfe, 0x00, 0x00};
uint8_t CODE epd_channel_select_length = sizeof(epd_channel_select);

static void SPI_on();
static void SPI_off();
static void SPI_put(uint8_t c);
static void SPI_put_wait(uint8_t c);
static void SPI_send(const uint8_t *buffer, uint16_t length);
static void PWM_start();
static void PWM_stop();
static void epd_spi_regidx(uint8_t reg);
static void epd_spi_command(uint8_t reg, uint8_t data);
static void epd_spi_command_2(uint8_t reg, uint8_t data1, uint8_t data2);
static void epd_spi_command_long(uint8_t reg, uint8_t* data, uint8_t size);

static void epd_frame_fixed(uint8_t fixed_value, EPD_stage stage, uint16_t first_line_no, uint8_t line_count);
static void epd_line(uint16_t line, const uint8_t *data, uint8_t fixed_value, EPD_stage stage);

void epd_clear() {
    epd_frame_fixed(0xff, EPD_compensate, 0, 0);
    epd_frame_fixed(0xff, EPD_white, 0, 0);
    epd_frame_fixed(0xaa, EPD_inverse, 0, 0);
    epd_frame_fixed(0xaa, EPD_normal, 0, 0);
}

void epd_begin() {
    // power up sequence
    setDigitalOutput(PIN_RESET, LOW);
    setDigitalOutput(PIN_PWR, LOW);
    setDigitalOutput(PIN_DISCHARGE, LOW);
    setDigitalOutput(PIN_BORDER, LOW);
    setDigitalOutput(PIN_SSEL, LOW);

    SPI_on();

    PWM_start();
    delayMs(5);
    setDigitalOutput(PIN_PWR, HIGH);
    delayMs(10);

    setDigitalOutput(PIN_RESET, HIGH);
    setDigitalOutput(PIN_BORDER, HIGH);
    setDigitalOutput(PIN_SSEL, HIGH);
    delayMs(5);

    setDigitalOutput(PIN_RESET, LOW);
    delayMs(5);

    setDigitalOutput(PIN_RESET, HIGH);
    delayMs(5);

    // wait for COG to become ready
    while (isPinHigh(PIN_BUSY));

    // channel select
    epd_spi_command_long(0x01, epd_channel_select, epd_channel_select_length);

    // DC/DC frequency
    epd_spi_command(0x06, 0xff);

    // high power mode osc
    epd_spi_command(0x07, 0x9d);

    // disable ADC
    epd_spi_command(0x08, 0x00);

    // Vcom level
    epd_spi_command_2(0x09, 0xd0, 0x00);

    // gate and source voltage levels
    epd_spi_command(0x04, 0x00); // NOTE: different for different display sizes.

    delayMs(5);  //???

    // driver latch on
    epd_spi_command(0x03, 0x01);

    // driver latch off
    epd_spi_command(0x03, 0x00);

    delayMs(5);

    // charge pump positive voltage on
    epd_spi_command(0x05, 0x01);

    // final delay before PWM off
    delayMs(30);
    PWM_stop();

    // charge pump negative voltage on
    epd_spi_command(0x05, 0x03);

    delayMs(30);

    // Vcom driver on
    epd_spi_command(0x05, 0x0f);

    delayMs(30);

    // output enable to disable
    epd_spi_command(0x02, 0x24);

    SPI_off();
}

void epd_end() {

    // dummy frame
    epd_frame_fixed(0x55, EPD_normal, 0, 0);

    // dummy line and border
    if (EPD_1_44 == epd_size) {
        // only for 1.44" EPD
        epd_line(0x7fffu, 0, 0xaa, EPD_normal);

        delayMs(250);

    } else {
        // all other display sizes
        epd_line(0x7fffu, 0, 0x55, EPD_normal);

        delayMs(25);

        setDigitalOutput(PIN_BORDER, LOW);
        delayMs(250);
        setDigitalOutput(PIN_BORDER, HIGH);
    }

    SPI_on();

    // latch reset turn on
    epd_spi_command(0x03, 0x01);

    // output enable off
    epd_spi_command(0x02, 0x05);

    // Vcom power off
    epd_spi_command(0x05, 0x0e);

    // power off negative charge pump
    epd_spi_command(0x05, 0x02);

    // discharge
    epd_spi_command(0x04, 0x0C);

    delayMs(120);

    // all charge pumps off
    epd_spi_command(0x05, 0x00);

    // turn of osc
    epd_spi_command(0x07, 0x0D);

    // discharge internal - 1
    epd_spi_command(0x04, 0x50);

    delayMs(40);

    // discharge internal - 2
    epd_spi_command(0x04, 0xA0);

    delayMs(40);

    // discharge internal - 3
    epd_spi_command(0x04, 0x00);

    // turn of power and all signals
    setDigitalOutput(PIN_RESET, LOW);
    setDigitalOutput(PIN_PWR, LOW);
    setDigitalOutput(PIN_BORDER, LOW);

    // ensure SPI MOSI and CLOCK are Low before CS Low
    SPI_off();
    setDigitalOutput(PIN_SSEL, LOW);

    // discharge pulse
    setDigitalOutput(PIN_DISCHARGE, HIGH);
    delayMs(150);
    setDigitalOutput(PIN_DISCHARGE, LOW);
}


// One frame of data is the number of lines * rows. For example:
// The 1.44” frame of data is 96 lines * 128 dots.
// The 2” frame of data is 96 lines * 200 dots.
// The 2.7” frame of data is 176 lines * 264 dots.

// the image is arranged by line which matches the display size
// so smallest would have 96 * 32 bytes

static void epd_frame_fixed(uint8_t fixed_value, EPD_stage stage, uint16_t first_line_no, uint8_t line_count) {
    uint8_t line, i;
    if(line_count == 0) {
        line_count = epd_lines_per_display;
    }
    for (i=0; i<epd_repeat_count; i++) {
        for (line = first_line_no; line < line_count + first_line_no ; ++line) {
            epd_line(line, 0, fixed_value, stage);
        }
    }
}

void epd_frame_data(const uint8_t *image, EPD_stage stage, uint16_t first_line_no, uint8_t line_count) {
    uint8_t line, i;
    if(line_count == 0) {
        line_count = epd_lines_per_display;
    }
    for (i=0; i<epd_repeat_count; i++) {
        for (line = first_line_no; line < line_count + first_line_no ; ++line)
        {
            epd_line(line, &image[(line - first_line_no) * epd_bytes_per_line], 0, stage);
        }
    }
}

void epd_frame_cb(uint32_t address, EPD_reader *reader, EPD_stage stage, uint16_t first_line_no, uint8_t line_count) {
    uint8_t XDATA buffer[264 / 8];
    uint8_t line;
    uint8_t i;
    if(line_count == 0)
    {
        line_count = epd_lines_per_display;
    }
    for (i=0; i<epd_repeat_count; i++) {
        for (line = first_line_no; line < line_count + first_line_no ; ++line) {
            reader(buffer, address + (line - first_line_no) * epd_bytes_per_line, epd_bytes_per_line);
            epd_line(line, buffer, 0, stage);
        }
    }
}

static void epd_line(uint16_t line, const uint8_t *data, uint8_t fixed_value, EPD_stage stage) {
    uint16_t b;

    SPI_on();

    // charge pump voltage levels
    epd_spi_command(0x04, 0x00); // NOTE: again, differs between sizes.

    // send data
    epd_spi_regidx(0x0a);
    delayMicroseconds(10);

    // CS low
    setDigitalOutput(PIN_SSEL, LOW);
    SPI_put_wait(0x72);

    // border byte only necessary for 1.44" EPD
    if (EPD_1_44 == epd_size) {
        SPI_put_wait(0x00);
        //SPI_send(PIN_SSEL, CU8_2(0x00), 1);
    }

    // even pixels
    for (b = epd_bytes_per_line; b > 0; --b) {
        if (0 != data) {
            uint8_t pixels = data[b - 1] & 0xaa;
            switch(stage) {
            case EPD_compensate:  // B -> W, W -> B (Current Image)
                pixels = 0xaa | ((pixels ^ 0xaa) >> 1);
                break;
            case EPD_white:       // B -> N, W -> W (Current Image)
                pixels = 0x55 + ((pixels ^ 0xaa) >> 1);
                break;
            case EPD_inverse:     // B -> N, W -> B (New Image)
                pixels = 0x55 | (pixels ^ 0xaa);
                break;
            case EPD_normal:       // B -> B, W -> W (New Image)
                pixels = 0xaa | (pixels >> 1);
                break;
            }
            SPI_put_wait(pixels);
        } else {
            SPI_put_wait(fixed_value);
        }
    }

    // scan line
    for (b = 0; b < epd_bytes_per_scan; ++b) {
        if (line / 4 == b) {
            SPI_put_wait(0xc0 >> (2 * (line & 0x03)));
        } else {
            SPI_put_wait(0x00);
        }
    }

    // odd pixels
    for (b = 0; b < epd_bytes_per_line; ++b) {
        if (0 != data) {
            uint8_t pixels = data[b] & 0x55;
            uint8_t p1, p2, p3, p4;
            switch(stage) {
            case EPD_compensate:  // B -> W, W -> B (Current Image)
                pixels = 0xaa | (pixels ^ 0x55);
                break;
            case EPD_white:       // B -> N, W -> W (Current Image)
                pixels = 0x55 + (pixels ^ 0x55);
                break;
            case EPD_inverse:     // B -> N, W -> B (New Image)
                pixels = 0x55 | ((pixels ^ 0x55) << 1);
                break;
            case EPD_normal:       // B -> B, W -> W (New Image)
                pixels = 0xaa | pixels;
                break;
            }
            p1 = (pixels >> 6) & 0x03;
            p2 = (pixels >> 4) & 0x03;
            p3 = (pixels >> 2) & 0x03;
            p4 = (pixels >> 0) & 0x03;
            pixels = (p1 << 0) | (p2 << 2) | (p3 << 4) | (p4 << 6);
            SPI_put_wait(pixels);
        } else {
            SPI_put_wait(fixed_value);
        }
    }

    if (epd_filler) {
        SPI_put_wait(0x00);
    }

    // CS high
    setDigitalOutput(PIN_SSEL, HIGH);

    // output data to panel
    epd_spi_command(0x02, 0x2f);

    SPI_off();
}


static void SPI_on() {
    SPI_put(0x00);
    SPI_put(0x00);
    delayMicroseconds(10);
}


static void SPI_off() {
    SPI_put(0x00);
    SPI_put(0x00);
    delayMicroseconds(10);
}


static void SPI_put(uint8_t c) {
    spi0MasterSendByte(c);
}


static void SPI_put_wait(uint8_t c) {

    // wait for COG ready
    while (isPinHigh(PIN_BUSY));

    SPI_put(c);
}


static void SPI_send(const uint8_t *buffer, uint16_t length) {
    // CS low
    setDigitalOutput(PIN_SSEL, LOW);

    // send all data
    while (length--) {
        SPI_put(*buffer++);
    }

    // CS high
    setDigitalOutput(PIN_SSEL, HIGH);
}

static void epd_spi_regidx(uint8_t reg) {
    delayMicroseconds(10);
    setDigitalOutput(PIN_SSEL, LOW);
    SPI_put(0x70);
    SPI_put(reg);
    setDigitalOutput(PIN_SSEL, HIGH);
}

static void epd_spi_command(uint8_t reg, uint8_t data) {
    epd_spi_regidx(reg);
    delayMicroseconds(10);
    setDigitalOutput(PIN_SSEL, LOW);
    SPI_put(0x72);
    SPI_put(data);
    setDigitalOutput(PIN_SSEL, HIGH);
}

static void epd_spi_command_2(uint8_t reg, uint8_t data1, uint8_t data2) {
    epd_spi_regidx(reg);
    delayMicroseconds(10);
    setDigitalOutput(PIN_SSEL, LOW);
    SPI_put(0x72);
    SPI_put(data1);
    SPI_put(data2);
    setDigitalOutput(PIN_SSEL, HIGH);
}

static void epd_spi_command_long(uint8_t reg, uint8_t* data, uint8_t size) {
    epd_spi_regidx(reg);
    delayMicroseconds(10);
    setDigitalOutput(PIN_SSEL, LOW);
    SPI_put(0x72);
    while (size) {
        SPI_put(*data++);
        size--;
    }
    setDigitalOutput(PIN_SSEL, HIGH);
}


static void PWM_start() {
    T3CTL   = 0b00000110; // no prescalar (000), don't start (0), no interrupt (0), clear (1), modulo mode (10)
    T3CCTL0 = 0b00010100; // unused (0), no interrupt (0), toggle on compare (010), enable (1), reserved (00)
    T3CC0 = 100; // frequency divisor: 24 MHz / 100 = 240 kHz.
    // Timer 3 use location Alternative 1 (P1_3 and P1_4).
    PERCFG &= ~(1<<5);  // PERCFG.T3CFG = 0;
    P1SEL |= (1<<3); // P1_3 is a peripheral (i.e., Timer 3 channel 0).

    // Start the timer.
    T3CTL |= (1<<4);
}

static void PWM_stop() {
    // Stop the timer.
    T3CTL &= ~(1<<4);

    // Drive P1_3 low.
    P1SEL &= ~(1<<3);
    P1 &= ~(1<<3);
}
