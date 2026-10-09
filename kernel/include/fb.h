#ifndef FB_H
#define FB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t phys_addr;
    uint32_t *virt_addr;
    uint32_t *back_buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
} framebuffer_t;

void fb_init(void);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_clear(uint32_t color);
void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_flip(void);
void fb_flip_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
framebuffer_t *fb_get_info(void);
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);


#endif
