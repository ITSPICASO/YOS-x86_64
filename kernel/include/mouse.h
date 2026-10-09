#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

#define MOUSE_IRQ_VECTOR 0x2C

typedef struct {
    int32_t x;
    int32_t y;
    bool    btn_left;
    bool    btn_right;
    bool    btn_middle;
} mouse_state_t;

void mouse_init(void);
void mouse_handler(void);
mouse_state_t mouse_get_state(void);
#define CURSOR_WIDTH  12
#define CURSOR_HEIGHT 19

void mouse_init_cursor(void);
void mouse_draw_cursor(void);
void mouse_update(void);


#endif
