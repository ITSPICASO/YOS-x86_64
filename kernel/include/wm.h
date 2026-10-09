#ifndef WM_H
#define WM_H

#include <stdint.h>
#include <stdbool.h>
#include "mouse.h"

#define WM_MAX_WINDOWS    16

#define RESIZE_NONE         0x00
#define RESIZE_LEFT         0x01
#define RESIZE_RIGHT        0x02
#define RESIZE_TOP          0x04
#define RESIZE_BOTTOM       0x08

#define WM_MIN_WIDTH        120
#define WM_MIN_HEIGHT       80
#define WM_RESIZE_BORDER_PX 4
#define WM_TITLEBAR_HEIGHT 24
#define WM_BORDER_COLOR   0x002A475E
#define WM_TITLEBAR_COLOR 0x00007ACC
#define WM_TITLEBAR_TEXT  0x00FFFFFF
#define WM_BG_COLOR       0x00171A21


#define WINDOW_FLAG_NONE       0x00
#define WINDOW_FLAG_MINIMIZED  0x01
#define WINDOW_FLAG_MAXIMIZED  0x02
#define WINDOW_FLAG_FOCUSED    0x04
#define WINDOW_FLAG_RESIZING   0x08
#define WINDOW_FLAG_MOVING     0x10

typedef struct window {
    int id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    char title[64];
    uint32_t *buffer;
    bool is_dragging;
    int32_t drag_off_x;
    int32_t drag_off_y;
    bool visible;
    uint32_t flags;
    int32_t saved_x;
    int32_t saved_y;
    uint32_t saved_w;
    uint32_t saved_h;
} window_t;

void wm_destroy_window(window_t *win);
void wm_bring_to_front(window_t *win);
void wm_cycle_windows(void);
void wm_stress_test(void);
void wm_send_to_back(window_t *win);

void wm_init(void);
window_t *wm_create_window(int32_t x, int32_t y, uint32_t w, uint32_t h, const char *title);
void wm_compose(void);
void wm_update_clock(void);
void wm_handle_mouse(int32_t mx, int32_t my, bool left_down);
window_t *wm_get_window(int id);
void wm_handle_keyboard(char c);
window_t *wm_get_active_window(void);

void win_draw_string(window_t *win, uint32_t rx, uint32_t ry, const char *str, uint32_t fg);
void win_draw_rect(window_t *win, int32_t x, int32_t ry, uint32_t rw, uint32_t rh, uint32_t color);

#endif
