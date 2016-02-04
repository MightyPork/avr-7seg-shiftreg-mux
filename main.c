#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdint.h>
#include <stdbool.h>

#include "lib/iopins.h"
#include "lib/uart.h"

// Configure what pins to use
#define IO_DATA D2
#define IO_CLK D3
#define IO_STR D4

/*
| LED display driver
|
| Two chained 70hc4094 are connected.
|
| One drives common cathodes, the other
| the segments.
|
| Display is controlled over UART at 115200 baud.
|
| Commands:
|
| R         - reset (clear display)
|
| Aaaaaaaaa - set value using ASCII. Supported are digits and letters A-F.
|               Period ('.') adds a decimal point to the last-entered symbol
|             (does not move "cursor").  It is not possible to add DP to the
|             last symbol using ASCII mode.
|
| Bbbbbbbbb - Set segments using binary mode (no conversion).
|             The bytes have the following format: 0bHGFEDCBA
|
| Ll        - Set brightness. `l` is a byte (0-255) determining the level.
|
*/

// ---- Segment definitions -----------------------

#define SEG_A 0x01
#define SEG_B 0x02
#define SEG_C 0x04
#define SEG_D 0x08
#define SEG_E 0x10
#define SEG_F 0x20
#define SEG_G 0x40
#define SEG_DP 0x80

/** Digits */
enum {
	SYM_BLANK = 0,
	SYM_0 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
	SYM_1 = SEG_B | SEG_C,
	SYM_2 = SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,
	SYM_3 = SEG_B | SEG_C | SEG_A | SEG_D | SEG_G,
	SYM_4 = SEG_F | SEG_G | SEG_B | SEG_C,
	SYM_5 = SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,
	SYM_6 = SEG_A | SEG_F | SEG_E | SEG_D | SEG_C | SEG_G,
	SYM_7 = SEG_A | SEG_B | SEG_C,
	SYM_8 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
	SYM_9 = SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,
	SYM_A = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,
	SYM_B = SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
	SYM_C = SEG_A | SEG_D | SEG_E | SEG_F,
	SYM_D = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,
	SYM_E = SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,
	SYM_F = SEG_A | SEG_E | SEG_F | SEG_G,
};


/** Conversion table */
const uint8_t num2seg[16] PROGMEM = {
	SYM_0, SYM_1, SYM_2, SYM_3,
	SYM_4, SYM_5, SYM_6, SYM_7,
	SYM_8, SYM_9, SYM_A, SYM_B,
	SYM_C, SYM_D, SYM_E, SYM_F
};

// ---- Routines for loading data to the display registers ----------

void disp_load(uint8_t place, uint8_t segments)
{
	uint16_t w;

	if (segments == 0) {
		w = 0;
	} else {
		w = (1 << (place + 8)) | segments;
	}

	for (uint8_t i = 0; i < 16; i++) {

		// bit value to data line
		set_pin(IO_DATA, (bool)(w & 0x8000));

		// pulse clock
		pin_high(IO_CLK);
		pin_low(IO_CLK);

		w <<= 1;
	}

	// pulse STR
	pin_high(IO_STR);
	pin_low(IO_STR);
}

/** Brightness table (for smoother brightness adjustment) */
const uint8_t BRIGHT_128[128] PROGMEM = {
	0,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,
	5,  5,  6,  6,  6,  7,  7,  8,  8,  8,  9,  10, 10, 10, 11, 12, 13, 14,
	14, 15, 16, 17, 18, 20, 21, 22, 24, 26, 27, 28, 30, 31, 32, 34, 35, 36,
	38, 39, 40, 41, 42, 44, 45, 46, 48, 49, 50, 52, 54, 56, 58, 59, 61, 63,
	65, 67, 68, 69, 71, 72, 74, 76, 78, 80, 82, 85, 88, 90, 92, 95, 98, 100,
	103, 106, 109, 112, 116, 119, 122, 125, 129, 134, 138, 142, 147, 151,
	153, 156, 160, 163, 165, 170, 175, 180, 185, 190, 195, 200, 207, 214, 218,
	221, 225, 228, 232, 234, 241, 248, 254, 255
};

/** Buffer for input */
uint8_t screen_buf[8];
uint8_t scr_buf_i;

/** Array of currently displayed segments */
uint8_t screen[8];

uint8_t level = 255;
uint8_t delay_l = 255;

enum {
	STATE_NONE,
	STATE_ASCII,
	STATE_BINARY,
	STATE_BRIGHTNESS
} state;


inline void cpy_buf2scr(void)
{
	for (uint8_t i = 0; i < 8; i++) {
		screen[i] = screen_buf[i];
	}
}


// Incoming data IRQ
ISR(USART_RX_vect)
{
	uint8_t b = uart_rx();
	uart_tx(b); // send back

	switch (state) {
		case STATE_NONE:
			// Choose what to do

			if (b >= 'a' && b <= 'z') {
				b -= 'a' - 'A'; // make uppercase
			}

			switch (b) {
				case 'A':
					state = STATE_ASCII;
					scr_buf_i = 0;
					break;

				case 'B':
					state = STATE_BINARY;
					scr_buf_i = 0;
					break;

				case 'R':
					for (uint8_t i = 0; i < 8; i++) {
						screen[i] = SYM_BLANK;
					}
					break;

				case 'L':
					state = STATE_BRIGHTNESS;
					break;

				default:
					break;
			}

			break;

		case STATE_BRIGHTNESS:
			level = b;
			delay_l = pgm_read_byte(&BRIGHT_128[level >> 1]);
			state = STATE_NONE;
			break;

		case STATE_ASCII:

			if (b >= 'a' && b <= 'z') {
				b -= 'a' - 'A'; // make uppercase
			}

			if (b >= '0' && b <= '9') {

				// numbers
				screen_buf[scr_buf_i++] = pgm_read_byte(&num2seg[b - '0']);

			} else if (b >= 'A' && b <= 'F') {

				// hex
				screen_buf[scr_buf_i++] = pgm_read_byte(&num2seg[10 + (b - 'A')]);

			} else if (b == '-') {

				// numbers
				screen_buf[scr_buf_i++] = SEG_G; // minus

			} else if (b == '.') {

				// add period to previous symbol
				if (scr_buf_i > 0) {
					screen_buf[scr_buf_i - 1] |= SEG_DP;
				}

			} else {
				// default - blank
				screen_buf[scr_buf_i++] = SYM_BLANK;
			}

			if (scr_buf_i == 8) {
				cpy_buf2scr();
				state = STATE_NONE;
			}

			break;

		case STATE_BINARY:
			screen_buf[scr_buf_i++] = b; // no processing

			if (scr_buf_i == 8) {
				cpy_buf2scr();
				state = STATE_NONE;
			}
			break;
	}

}


void main()
{
	_uart_init(8); // set usart @ 115200

	as_output(IO_DATA);
	as_output(IO_CLK);
	as_output(IO_STR);

	uart_isr_rx(true);

	sei();

	while (1) {
		// display loop
		for (register uint8_t i = 0; i < 8; i++) {
			disp_load(i, screen[i]);

			for (register uint8_t j = 0; j < 255; j++) {
				if (j == delay_l) disp_load(0, 0);

				__builtin_avr_delay_cycles(5);
			}
		}
	}
}
