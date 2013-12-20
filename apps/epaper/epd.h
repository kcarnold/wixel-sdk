typedef void EPD_reader(uint8_t XDATA *buffer, uint32_t address, uint16_t length) __reentrant;

typedef enum {           // Image pixel -> Display pixel
    EPD_compensate,  // B -> W, W -> B (Current Image)
    EPD_white,       // B -> N, W -> W (Current Image)
    EPD_inverse,     // B -> N, W -> B (New Image)
    EPD_normal       // B -> B, W -> W (New Image)
} EPD_stage;

void epd_begin();
void epd_end();
void epd_clear();

void epd_frame_cb(uint32_t address, EPD_reader *reader, EPD_stage stage, uint8_t repeat_count, uint16_t first_line_no, uint8_t line_count);
