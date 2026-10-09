#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* Police bitmap standard 8x16 pour ASCII 0..127 */
extern const uint8_t font8x16[128][16];

#endif
