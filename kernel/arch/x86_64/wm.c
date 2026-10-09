#include "rtc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "wm.h"
#include "fb.h"
#include "font.h"
#include "heap.h"
#include "string.h"
#include "serial.h"

static char *k_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

static int32_t hover_mx = -1;
static int32_t hover_my = -1;

static window_t *windows[WM_MAX_WINDOWS];
static int window_count = 0;
static window_t *active_window = NULL;

static bool start_menu_open = false;

static void wm_draw_start_menu(void) {
    framebuffer_t *fb = fb_get_info();
    if (!fb || !start_menu_open) return;

    uint32_t menu_w = 180;
    uint32_t menu_h = 130;
    uint32_t menu_x = 4;
    uint32_t menu_y = fb->height - 32 - menu_h;

    /* Fond & bordure du menu */
    fb_draw_rect(menu_x - 1, menu_y - 1, menu_w + 2, menu_h + 2, 0x00007ACC);
    fb_draw_rect(menu_x, menu_y, menu_w, menu_h, 0x00171A21);

    /* En-tete YOS */
    fb_draw_rect(menu_x, menu_y, menu_w, 24, 0x00007ACC);
    fb_draw_string(menu_x + 8, menu_y + 4, "YOS Applications", 0x00FFFFFF, 0xFFFFFFFF);

    /* Item 0: Terminal (sh) [menu_y + 28 .. + 52] */
    bool h0 = (hover_mx >= (int32_t)menu_x && hover_mx < (int32_t)(menu_x + menu_w) &&
               hover_my >= (int32_t)(menu_y + 28) && hover_my < (int32_t)(menu_y + 52));
    if (h0) fb_draw_rect(menu_x + 2, menu_y + 28, menu_w - 4, 22, 0x002A475E);
    fb_draw_string(menu_x + 12, menu_y + 32, "> Terminal (sh)", h0 ? 0x0000FFCC : 0x00E5E5E5, 0xFFFFFFFF);

    /* Item 1: System Monitor [menu_y + 52 .. + 76] */
    bool h1 = (hover_mx >= (int32_t)menu_x && hover_mx < (int32_t)(menu_x + menu_w) &&
               hover_my >= (int32_t)(menu_y + 52) && hover_my < (int32_t)(menu_y + 76));
    if (h1) fb_draw_rect(menu_x + 2, menu_y + 52, menu_w - 4, 22, 0x002A475E);
    fb_draw_string(menu_x + 12, menu_y + 56, "> System Monitor", h1 ? 0x0000FFCC : 0x00E5E5E5, 0xFFFFFFFF);

    /* Item 2: About YOS [menu_y + 76 .. + 100] */
    bool h2 = (hover_mx >= (int32_t)menu_x && hover_mx < (int32_t)(menu_x + menu_w) &&
               hover_my >= (int32_t)(menu_y + 76) && hover_my < (int32_t)(menu_y + 100));
    if (h2) fb_draw_rect(menu_x + 2, menu_y + 76, menu_w - 4, 22, 0x002A475E);
    fb_draw_string(menu_x + 12, menu_y + 80, "> About YOS", h2 ? 0x0000FFCC : 0x00E5E5E5, 0xFFFFFFFF);

    /* Item 3: Redemarrer [menu_y + 100 .. + 126] */
    bool h3 = (hover_mx >= (int32_t)menu_x && hover_mx < (int32_t)(menu_x + menu_w) &&
               hover_my >= (int32_t)(menu_y + 100) && hover_my < (int32_t)(menu_y + 126));
    if (h3) fb_draw_rect(menu_x + 2, menu_y + 100, menu_w - 4, 24, 0x00801818);
    fb_draw_string(menu_x + 12, menu_y + 104, "> Redemarrer", h3 ? 0x00FFFFFF : 0x00FF6666, 0xFFFFFFFF);
}

static bool is_resizing = false;
static uint8_t active_resize_edge = RESIZE_NONE;
static int32_t resize_orig_mx = 0;
static int32_t resize_orig_my = 0;
static int32_t resize_orig_win_x = 0;
static int32_t resize_orig_win_y = 0;
static uint32_t resize_orig_win_w = 0;
static uint32_t resize_orig_win_h = 0;

static uint8_t wm_get_resize_edge(window_t *w, int32_t mx, int32_t my) {
    if (!w || !w->visible || (w->flags & WINDOW_FLAG_MAXIMIZED) || (w->flags & WINDOW_FLAG_MINIMIZED)) {
        return RESIZE_NONE;
    }

    int32_t wx = w->x;
    int32_t wy = w->y;
    int32_t ww = w->width;
    int32_t wh = w->height + WM_TITLEBAR_HEIGHT;
    int32_t m = WM_RESIZE_BORDER_PX;

    /* Verifier si mx, my dans la boite elargie */
    if (mx < wx - m || mx > wx + ww + m || my < wy - m || my > wy + wh + m) {
        return RESIZE_NONE;
    }

    uint8_t edge = RESIZE_NONE;
    if (mx >= wx - m && mx <= wx + m) edge |= RESIZE_LEFT;
    if (mx >= wx + ww - m && mx <= wx + ww + m) edge |= RESIZE_RIGHT;
    if (my >= wy - m && my <= wy + m) edge |= RESIZE_TOP;
    if (my >= wy + wh - m && my <= wy + wh + m) edge |= RESIZE_BOTTOM;

    return edge;
}

static void wm_resize_window_buffer(window_t *win, uint32_t new_w, uint32_t new_h) {
    if (!win || new_w == 0 || new_h == 0) return;
    if (new_w == win->width && new_h == win->height) return;

    uint32_t *new_buf = (uint32_t *)kmalloc(new_w * new_h * sizeof(uint32_t));
    if (!new_buf) return;

    /* Initialiser avec couleur de fond */
    for (uint32_t i = 0; i < new_w * new_h; i++) {
        new_buf[i] = WM_BG_COLOR;
    }

    /* Copier lancien contenu clipse */
    if (win->buffer) {
        uint32_t copy_w = (win->width < new_w) ? win->width : new_w;
        uint32_t copy_h = (win->height < new_h) ? win->height : new_h;
        for (uint32_t y = 0; y < copy_h; y++) {
            for (uint32_t x = 0; x < copy_w; x++) {
                new_buf[y * new_w + x] = win->buffer[y * win->width + x];
            }
        }
        kfree(win->buffer);
    }

    win->buffer = new_buf;
    win->width = new_w;
    win->height = new_h;
}

static bool is_dragging_outline = false;
static int32_t drag_orig_x = 0;
static int32_t drag_orig_y = 0;
static int32_t drag_curr_x = 0;
static int32_t drag_curr_y = 0;
static int32_t drag_last_x = 0;
static int32_t drag_last_y = 0;
static uint32_t drag_w = 0;
static uint32_t drag_h = 0;

static void draw_wireframe_rect(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    /* Rsem cadre khfif (4 khtout b 1px) direct f VRAM w BackBuffer */
    for (uint32_t i = 0; i < w; i++) {
        fb_putpixel(x + i, y, 0x00FFFFFF);
        fb_putpixel(x + i, y + h - 1, 0x00FFFFFF);
    }
    for (uint32_t j = 0; j < h; j++) {
        fb_putpixel(x, y + j, 0x00FFFFFF);
        fb_putpixel(x + w - 1, y + j, 0x00FFFFFF);
    }
}


void win_draw_string(window_t *win, uint32_t rx, uint32_t ry, const char *str, uint32_t fg) {
    if (!win || !win->buffer) return;
    uint32_t cur_x = rx;
    uint32_t cur_y = ry;

    while (*str) {
        if (*str == '\n') {
            cur_x = rx;
            cur_y += 16;
        } else {
            uint8_t ch = (uint8_t)*str;
            if (ch < 128) {
                const uint8_t *glyph = font8x16[ch];
                for (int row = 0; row < 16; row++) {
                    uint8_t line = glyph[row];
                    int py = cur_y + row;
                    if (py < 0 || py >= (int)win->height) continue;
                    for (int col = 0; col < 8; col++) {
                        int px = cur_x + col;
                        if (px < 0 || px >= (int)win->width) continue;
                        if (line & (0x80 >> col)) {
                            win->buffer[py * win->width + px] = fg;
                        }
                    }
                }
            }
            cur_x += 8;
        }
        str++;
    }
}


static void wm_draw_desktop(void) {
    fb_clear(0x001B2838);

    framebuffer_t *fb = fb_get_info();
    if (fb) {
        uint32_t bar_h = 32;
        uint32_t bar_y = fb->height - bar_h;
        fb_draw_rect(0, bar_y, fb->width, bar_h, 0x0010161D);
        fb_draw_rect(0, bar_y, fb->width, 1, 0x002A475E);

        /* Bouton Start Menu */
        uint32_t btn_col = start_menu_open ? 0x00005A9E : 0x00007ACC;
        fb_draw_rect(4, bar_y + 4, 80, 24, btn_col);
        fb_draw_string(14, bar_y + 8, "YOS 64", 0x00FFFFFF, 0xFFFFFFFF);

        /* Dimensions horloge RTC */
        uint32_t clock_box_w = 80;
        uint32_t clock_box_x = fb->width - clock_box_w - 8;

        /* Etape 21: Boutons de taches pour les fenetres ouvertes */
        uint32_t tab_x = 92;
        uint32_t tab_w = 110;
        uint32_t tab_h = 24;

        for (int i = 0; i < window_count; i++) {
            window_t *w = windows[i];
            if (!w) continue;
            if (tab_x + tab_w >= clock_box_x - 8) break;

            uint32_t tab_bg;
            uint32_t tab_txt_col = 0x00FFFFFF;

            if (w == active_window && !(w->flags & WINDOW_FLAG_MINIMIZED)) {
                tab_bg = 0x00007ACC;
            } else if (w->flags & WINDOW_FLAG_MINIMIZED) {
                tab_bg = 0x001E222B;
                tab_txt_col = 0x008892B0;
            } else {
                tab_bg = 0x002B303C;
            }

            fb_draw_rect(tab_x, bar_y + 4, tab_w, tab_h, tab_bg);
            fb_draw_rect(tab_x, bar_y + 4, tab_w, 1, 0x003A4354);

            char tab_title[12];
            int ci = 0;
            while (w->title[ci] != 0 && ci < 10) {
                tab_title[ci] = w->title[ci];
                ci++;
            }
            tab_title[ci] = 0;

            fb_draw_string(tab_x + 8, bar_y + 8, tab_title, tab_txt_col, 0xFFFFFFFF);
            tab_x += tab_w + 4;
        }

        /* Horloge RTC */
        rtc_time_t t;
        rtc_read_time(&t);

        char clock_str[9];
        clock_str[0] = 0x30 + ((t.hour / 10) % 10);
        clock_str[1] = 0x30 + (t.hour % 10);
        clock_str[2] = 0x3A;
        clock_str[3] = 0x30 + ((t.minute / 10) % 10);
        clock_str[4] = 0x30 + (t.minute % 10);
        clock_str[5] = 0x3A;
        clock_str[6] = 0x30 + ((t.second / 10) % 10);
        clock_str[7] = 0x30 + (t.second % 10);
        clock_str[8] = 0;

        fb_draw_rect(clock_box_x, bar_y + 4, clock_box_w, 24, 0x002A475E);
        fb_draw_rect(clock_box_x + 1, bar_y + 5, clock_box_w - 2, 22, 0x00171A21);
        fb_draw_string(clock_box_x + 8, bar_y + 8, clock_str, 0x0000FFCC, 0xFFFFFFFF);
    }
}

void wm_init(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i] = NULL;
    }
    window_count = 0;
    active_window = NULL;

    wm_draw_desktop();
    serial_print("[+] Window Manager (Bloc 13) initialise!\n");
}

void wm_send_to_back(window_t *win) {
    if (!win || window_count <= 1) return;

    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            idx = i;
            break;
        }
    }
    if (idx == -1 || idx == 0) return;

    /* Decalage vers la droite */
    for (int i = idx; i > 0; i--) {
        windows[i] = windows[i - 1];
    }
    windows[0] = win;

    /* Mise a jour focus vers la derniere fenetre visible */
    for (int i = 0; i < window_count; i++) {
        if (windows[i]) windows[i]->flags &= ~WINDOW_FLAG_FOCUSED;
    }
    for (int i = window_count - 1; i >= 0; i--) {
        if (windows[i] && windows[i]->visible && !(windows[i]->flags & WINDOW_FLAG_MINIMIZED)) {
            windows[i]->flags |= WINDOW_FLAG_FOCUSED;
            active_window = windows[i];
            break;
        }
    }
}

void wm_bring_to_front(window_t *win) {
    if (!win || window_count <= 1) return;

    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            idx = i;
            break;
        }
    }
    if (idx == -1 || idx == window_count - 1) {
        active_window = win;
        return;
    }

    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    windows[window_count - 1] = win;

    for (int i = 0; i < window_count; i++) {
        if (windows[i]) windows[i]->flags &= ~WINDOW_FLAG_FOCUSED;
    }
    win->flags |= WINDOW_FLAG_FOCUSED;
    active_window = win;
}

void wm_destroy_window(window_t *win) {
    if (!win) return;

    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return;

    if (win->buffer) {
        kfree(win->buffer);
        win->buffer = NULL;
    }

    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    windows[window_count - 1] = NULL;
    window_count--;

    kfree(win);

    if (window_count > 0) {
        active_window = windows[window_count - 1];
        if (active_window) active_window->flags |= WINDOW_FLAG_FOCUSED;
    } else {
        active_window = NULL;
    }
}

window_t *wm_create_window(int32_t x, int32_t y, uint32_t w, uint32_t h, const char *title) {
    if (window_count >= WM_MAX_WINDOWS) return NULL;

    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) return NULL;

    win->id = window_count + 1;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->saved_x = x;
    win->saved_y = y;
    win->saved_w = w;
    win->saved_h = h;
    win->visible = true;
    win->is_dragging = false;
    win->drag_off_x = 0;
    win->drag_off_y = 0;
    win->flags = WINDOW_FLAG_FOCUSED;

    k_strncpy(win->title, title, 63);
    win->title[63] = 0;

    win->buffer = (uint32_t *)kmalloc(w * h * sizeof(uint32_t));
    if (win->buffer) {
        for (uint32_t i = 0; i < w * h; i++) {
            win->buffer[i] = WM_BG_COLOR;
        }
    }

    for (int i = 0; i < window_count; i++) {
        if (windows[i]) windows[i]->flags &= ~WINDOW_FLAG_FOCUSED;
    }

    windows[window_count++] = win;
    active_window = win;

    return win;
}

static void wm_render_window(window_t *win) {
    if (!win || !win->visible) return;
    if (win->flags & WINDOW_FLAG_MINIMIZED) return;

    framebuffer_t *fb = fb_get_info();
    if (!fb) return;

    int32_t wx = win->x;
    int32_t wy = win->y;
    uint32_t total_h = win->height + WM_TITLEBAR_HEIGHT;

    /* Etape 10: Drop Shadow pour la fenetre active (si non-maximisee) */
    if (win == active_window && !(win->flags & WINDOW_FLAG_MAXIMIZED)) {
        int32_t shadow_offset = 4;
        fb_draw_rect(wx + shadow_offset, wy + shadow_offset, win->width + 4, total_h + 4, 0x000A0D12);
    }

    /* Bordure & Barre de Titre (Accent si actif, Gris si inactif) */
    uint32_t b_color = (win == active_window) ? 0x00007ACC : WM_BORDER_COLOR;
    fb_draw_rect(wx - 2, wy - 2, win->width + 4, total_h + 4, b_color);
    uint32_t t_color = (win == active_window) ? WM_TITLEBAR_COLOR : 0x002B2D30;
    fb_draw_rect(wx, wy, win->width, WM_TITLEBAR_HEIGHT, t_color);

    /* Coordonnees des 3 boutons */
    int32_t close_x = wx + win->width - 20;
    int32_t max_x   = wx + win->width - 38;
    int32_t min_x   = wx + win->width - 56;
    int32_t btn_y   = wy + 4;
    int32_t btn_sz  = 16;

    /* Etape 8: Detection Hover par bouton */
    bool hover_close = (hover_mx >= close_x && hover_mx < close_x + btn_sz &&
                        hover_my >= btn_y && hover_my < btn_y + btn_sz);
    bool hover_max   = (hover_mx >= max_x && hover_mx < max_x + btn_sz &&
                        hover_my >= btn_y && hover_my < btn_y + btn_sz);
    bool hover_min   = (hover_mx >= min_x && hover_mx < min_x + btn_sz &&
                        hover_my >= btn_y && hover_my < btn_y + btn_sz);

    /* Bouton 1: Minimize [-] */
    uint32_t min_bg = hover_min ? 0x004A505C : 0x003A3F47;
    fb_draw_rect(min_x, btn_y, btn_sz, btn_sz, min_bg);
    fb_draw_char(min_x + 4, wy + 2, 0x2D, 0x00FFFFFF, 0xFFFFFFFF);

    /* Bouton 2: Maximize [+] / Restore [r] */
    uint32_t max_bg = hover_max ? 0x004A505C : 0x003A3F47;
    fb_draw_rect(max_x, btn_y, btn_sz, btn_sz, max_bg);
    char max_char = (win->flags & WINDOW_FLAG_MAXIMIZED) ? 0x72 : 0x2B;
    fb_draw_char(max_x + 4, btn_y, max_char, 0x00FFFFFF, 0xFFFFFFFF);

    /* Bouton 3: Close [x] */
    uint32_t close_bg = hover_close ? 0x00E81123 : 0x00C42B1C;
    fb_draw_rect(close_x, btn_y, btn_sz, btn_sz, close_bg);
    fb_draw_char(close_x + 4, btn_y, 0x78, 0x00FFFFFF, 0xFFFFFFFF);

    /* Titre de la fenetre */
    fb_draw_string(wx + 8, wy + 4, win->title, WM_TITLEBAR_TEXT, 0xFFFFFFFF);

    int32_t client_y = wy + WM_TITLEBAR_HEIGHT;
    if (win->buffer) {
        for (uint32_t j = 0; j < win->height; j++) {
            int32_t py = client_y + j;
            if (py < 0 || py >= (int32_t)fb->height) continue;

            for (uint32_t i = 0; i < win->width; i++) {
                int32_t px = wx + i;
                if (px < 0 || px >= (int32_t)fb->width) continue;

                fb_putpixel(px, py, win->buffer[j * win->width + i]);
            }
        }
    }
}


static int32_t cur_prev_x = -1;
static int32_t cur_prev_y = -1;
static bool cur_saved = false;

static void wm_render_cursor(int32_t mx, int32_t my);

void wm_compose(void) {
    wm_draw_desktop();
    /* Invalidation du curseur pour eviter les artefacts */
    cur_saved = false;
    cur_prev_x = -1;
    cur_prev_y = -1;

    for (int i = 0; i < window_count; i++) {
        wm_render_window(windows[i]);
    }

    wm_draw_start_menu();
    fb_flip();
    cur_saved = false;
    cur_prev_x = -1;
    cur_prev_y = -1;
    /* Dessin direct du curseur sans enregistrer de zone a restaurer */
    framebuffer_t *fb = fb_get_info();
    if (fb && fb->virt_addr) {
        for (int y = 0; y < 12; y++) {
            for (int x = 0; x <= y && x < 8; x++) {
                int px = hover_mx + x;
                int py = hover_my + y;
                if (px >= 0 && px < (int)fb->width && py >= 0 && py < (int)fb->height) {
                    uint32_t col = (x == 0 || x == y || y == 11) ? 0x00000000 : 0x00FFFFFF;
                    fb->virt_addr[py * fb->width + px] = col;
                }
            }
        }
    }
}


static uint32_t click_counter = 0;
static uint32_t last_click_time = 0;
static window_t *last_clicked_win = NULL;


static void wm_erase_cursor(void) {
    if (!cur_saved || cur_prev_x < 0 || cur_prev_y < 0) return;
    framebuffer_t *fb = fb_get_info();
    if (!fb || !fb->virt_addr || !fb->back_buffer) return;

    /* Restauration depuis back_buffer propre directement vers l ecran physique */
    for (int y = 0; y < 12; y++) {
        int py = cur_prev_y + y;
        if (py < 0 || py >= (int)fb->height) continue;
        for (int x = 0; x < 12; x++) {
            int px = cur_prev_x + x;
            if (px < 0 || px >= (int)fb->width) continue;
            uint32_t orig_pixel = fb->back_buffer[py * fb->width + px];
            fb->virt_addr[py * fb->width + px] = orig_pixel;
        }
    }
    cur_saved = false;
}

static void wm_render_cursor(int32_t mx, int32_t my) {
    framebuffer_t *fb = fb_get_info();
    if (!fb || !fb->virt_addr) return;

    cur_prev_x = mx;
    cur_prev_y = my;
    cur_saved = true;

    /* Dessin direct sur virt_addr (l ecran) sans toucher a back_buffer ! */
    for (int y = 0; y < 12; y++) {
        for (int x = 0; x <= y && x < 8; x++) {
            int px = mx + x;
            int py = my + y;
            if (px >= 0 && px < (int)fb->width && py >= 0 && py < (int)fb->height) {
                uint32_t col = (x == 0 || x == y || y == 11) ? 0x00000000 : 0x00FFFFFF;
                fb->virt_addr[py * fb->width + px] = col;
            }
        }
    }
}

static void wm_realloc_win_buffer(window_t *w, uint32_t new_w, uint32_t new_h) {
    if (!w || (w->width == new_w && w->height == new_h)) return;
    uint32_t *new_buf = (uint32_t*)kmalloc(new_w * new_h * sizeof(uint32_t));
    if (!new_buf) return;

    /* Nettoyer avec la couleur de fond de la fenetre */
    for (uint32_t i = 0; i < new_w * new_h; i++) {
        new_buf[i] = WM_BG_COLOR;
    }

    if (w->buffer) {
        /* Copier le contenu existant */
        uint32_t min_w = (w->width < new_w) ? w->width : new_w;
        uint32_t min_h = (w->height < new_h) ? w->height : new_h;
        for (uint32_t y = 0; y < min_h; y++) {
            for (uint32_t x = 0; x < min_w; x++) {
                new_buf[y * new_w + x] = w->buffer[y * w->width + x];
            }
        }
        kfree(w->buffer);
    }

    w->buffer = new_buf;
    w->width = new_w;
    w->height = new_h;
}

void wm_handle_mouse(int32_t mx, int32_t my, bool left_down) {
    bool mouse_moved = (mx != hover_mx || my != hover_my);
    hover_mx = mx;
    hover_my = my;

    if (mouse_moved) {
        wm_erase_cursor();
        wm_render_cursor(mx, my);
    }
    static bool was_down = false;
    bool just_pressed = left_down && !was_down;
    bool just_released = !left_down && was_down;
    was_down = left_down;

    if (just_pressed) {
        framebuffer_t *fb = fb_get_info();
        if (fb && my >= (int32_t)(fb->height - 32) && mx >= 4 && mx <= 84) {
            start_menu_open = !start_menu_open;
            wm_compose();
            return;
        }

        /* Clic a l interieur du Start Menu ouvert */
        if (start_menu_open && fb) {
            uint32_t menu_w = 180;
            uint32_t menu_h = 130;
            uint32_t menu_x = 4;
            uint32_t menu_y = fb->height - 32 - menu_h;

            if (mx >= (int32_t)menu_x && mx < (int32_t)(menu_x + menu_w) &&
                my >= (int32_t)menu_y && my < (int32_t)(menu_y + menu_h)) {

                /* 1. Terminal (sh) */
                if (my >= (int32_t)(menu_y + 28) && my < (int32_t)(menu_y + 52)) {
                    start_menu_open = false;
                    window_t *found = NULL;
                    for (int i = 0; i < window_count; i++) {
                        if (windows[i] && windows[i]->title[0] == 'T') {
                            found = windows[i]; break;
                        }
                    }
                    if (found) {
                        found->flags &= ~WINDOW_FLAG_MINIMIZED;
                        wm_bring_to_front(found);
                    } else {
                        window_t *w = wm_create_window(60, 60, 420, 240, "Terminal (sh)");
                        if (w) {
                            win_draw_string(w, 10, 10, "root@yos-kernel:~# uname -a", 0x0000FF00);
                            win_draw_string(w, 10, 30, "YOS 64-bit Kernel v1.0 SMP x86_64", 0x00CCCCCC);
                            win_draw_string(w, 10, 60, "root@yos-kernel:~# ls -l /", 0x0000FF00);
                            win_draw_string(w, 10, 80, "drwx------  devfs", 0x0000AAFF);
                            win_draw_string(w, 10, 100, "drwx------  procfs", 0x0000AAFF);
                            win_draw_string(w, 10, 120, "-rwxr-xr-x  hello.elf", 0x00FFFFFF);
                        }
                    }
                    wm_compose();
                    return;
                }

                /* 2. System Monitor */
                if (my >= (int32_t)(menu_y + 52) && my < (int32_t)(menu_y + 76)) {
                    start_menu_open = false;
                    window_t *found = NULL;
                    for (int i = 0; i < window_count; i++) {
                        if (windows[i] && windows[i]->title[0] == 'S') {
                            found = windows[i]; break;
                        }
                    }
                    if (found) {
                        found->flags &= ~WINDOW_FLAG_MINIMIZED;
                        wm_bring_to_front(found);
                    } else {
                        window_t *w = wm_create_window(520, 140, 360, 200, "System Monitor");
                        if (w) {
                            win_draw_string(w, 10, 10, "CPU  : AMD/Intel x86_64 @ Ring 0/3", 0x00FFAA00);
                            win_draw_string(w, 10, 30, "ARCH : Higher-Half PML4 4-Level", 0x00FFFFFF);
                            win_draw_string(w, 10, 50, "DISP : UEFI/GOP Linear FB", 0x0000FFFF);
                            win_draw_string(w, 10, 70, "MEM  : PMM Page Frame Allocator", 0x0055FF55);
                            win_draw_string(w, 10, 90, "SCHED: MLFQ Priority Queues", 0x00FF55AA);
                        }
                    }
                    wm_compose();
                    return;
                }

                /* 3. About YOS */
                if (my >= (int32_t)(menu_y + 76) && my < (int32_t)(menu_y + 100)) {
                    start_menu_open = false;
                    window_t *w = wm_create_window(260, 180, 380, 190, "About YOS 64");
                    if (w) {
                        win_draw_rect(w, 10, 10, 360, 170, 0x00171A21);
                        win_draw_string(w, 20, 25, "YOS Operating System x86_64", 0x0000FFCC);
                        win_draw_string(w, 20, 50, "Architecture: 64-bit Long Mode", 0x00FFFFFF);
                        win_draw_string(w, 20, 75, "Compositing Window Manager OK", 0x0055FF55);
                        win_draw_string(w, 20, 100, "Kernel Heap: 32MB Dynamic", 0x00FFAA00);
                        win_draw_string(w, 20, 130, "(c) 2026 YOS - Open Source Project", 0x00888888);
                    }
                    wm_compose();
                    return;
                }

                /* 4. Redemarrer */
                if (my >= (int32_t)(menu_y + 100) && my < (int32_t)(menu_y + 126)) {
                    /* Pulse reset CPU via 8042 Keyboard Controller */
                    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
                    return;
                }
            } else {
                /* Clic en dehors du menu : le fermer */
                start_menu_open = false;
                wm_compose();
            }
        }

            /* Etape 22: Clic sur les boutons de taches */
            if (fb && my >= (int32_t)(fb->height - 32) && mx >= 92) {
                uint32_t tab_x = 92;
                uint32_t tab_w = 110;
                for (int i = 0; i < window_count; i++) {
                    window_t *w = windows[i];
                    if (!w) continue;
                    if (mx >= (int32_t)tab_x && mx < (int32_t)(tab_x + tab_w)) {
                        if (w->flags & WINDOW_FLAG_MINIMIZED) {
                            w->flags &= ~WINDOW_FLAG_MINIMIZED;
                            wm_bring_to_front(w);
                        } else if (w == active_window) {
                            w->flags |= WINDOW_FLAG_MINIMIZED;
                            active_window = NULL;
                            for (int k = window_count - 1; k >= 0; k--) {
                                if (windows[k] && windows[k]->visible && !(windows[k]->flags & WINDOW_FLAG_MINIMIZED)) {
                                    wm_bring_to_front(windows[k]);
                                    break;
                                }
                            }
                        } else {
                            wm_bring_to_front(w);
                        }
                        wm_compose();
                        return;
                    }
                    tab_x += tab_w + 4;
                }
            }

        for (int i = window_count - 1; i >= 0; i--) {
            window_t *w = windows[i];
            if (!w || !w->visible || (w->flags & WINDOW_FLAG_MINIMIZED)) continue;

            /* Etape 16: Verification de la bordure de redimensionnement */
            uint8_t edge = wm_get_resize_edge(w, mx, my);
            if (edge != RESIZE_NONE) {
                wm_bring_to_front(w);
                w->flags |= WINDOW_FLAG_RESIZING;
                is_resizing = true;
                active_resize_edge = edge;
                resize_orig_mx = mx;
                resize_orig_my = my;
                resize_orig_win_x = w->x;
                resize_orig_win_y = w->y;
                resize_orig_win_w = w->width;
                resize_orig_win_h = w->height;

                drag_curr_x = w->x;
                drag_curr_y = w->y;
                drag_w = w->width;
                drag_h = w->height + WM_TITLEBAR_HEIGHT;
                wm_compose();
                return;
            }

            int32_t close_btn_x = w->x + (int32_t)w->width - 20;
            int32_t max_btn_x   = w->x + (int32_t)w->width - 38;
            int32_t min_btn_x   = w->x + (int32_t)w->width - 56;
            int32_t tb_bottom   = w->y + WM_TITLEBAR_HEIGHT;

            /* 1. Hitbox Close [x] */
            if (mx >= close_btn_x && mx <= (int32_t)(w->x + w->width) &&
                my >= w->y && my <= tb_bottom) {
                cur_saved = false;
                wm_destroy_window(w);
                wm_compose();
                return;
            }

            /* 2. Hitbox Maximize [+] */
            if (mx >= max_btn_x && mx < close_btn_x &&
                my >= w->y && my <= tb_bottom) {
                framebuffer_t *fb = fb_get_info();
                if (fb) {
                    if (w->flags & WINDOW_FLAG_MAXIMIZED) {
                        /* Restore */
                        w->x = w->saved_x;
                        w->y = w->saved_y;
                        wm_realloc_win_buffer(w, w->saved_w, w->saved_h);
                        w->flags &= ~WINDOW_FLAG_MAXIMIZED;
                    } else {
                        /* Maximize */
                        w->saved_x = w->x;
                        w->saved_y = w->y;
                        w->saved_w = w->width;
                        w->saved_h = w->height;
                        w->x = 0;
                        w->y = 0;
                        uint32_t target_w = fb->width;
                        uint32_t target_h = fb->height - 32 - WM_TITLEBAR_HEIGHT;
                        wm_realloc_win_buffer(w, target_w, target_h);
                        w->flags |= WINDOW_FLAG_MAXIMIZED;
                    }
                    wm_bring_to_front(w);
                    wm_compose();
                }
                return;
            }

            /* 3. Hitbox Minimize [-] */
            if (mx >= min_btn_x && mx < max_btn_x &&
                my >= w->y && my <= tb_bottom) {
                w->flags |= WINDOW_FLAG_MINIMIZED;
                if (active_window == w) {
                    active_window = NULL;
                    for (int k = window_count - 1; k >= 0; k--) {
                        if (windows[k] && windows[k]->visible && !(windows[k]->flags & WINDOW_FLAG_MINIMIZED)) {
                            wm_bring_to_front(windows[k]);
                            break;
                        }
                    }
                }
                wm_compose();
                return;
            }

            /* 4. Click 3la Titlebar (Drag w Focus) */
            if (mx >= w->x && mx < min_btn_x &&
                my >= w->y && my <= tb_bottom) {

                wm_bring_to_front(w);

                /* Etape 14: Double-clic detection */
                click_counter++;
                if (last_clicked_win == w && (click_counter - last_click_time) < 30) {
                    framebuffer_t *fb_d = fb_get_info();
                    if (fb_d) {
                        if (w->flags & WINDOW_FLAG_MAXIMIZED) {
                            w->x = w->saved_x;
                            w->y = w->saved_y;
                            wm_realloc_win_buffer(w, w->saved_w, w->saved_h);
                            w->flags &= ~WINDOW_FLAG_MAXIMIZED;
                        } else {
                            w->saved_x = w->x;
                            w->saved_y = w->y;
                            w->saved_w = w->width;
                            w->saved_h = w->height;
                            w->x = 0;
                            w->y = 0;
                            uint32_t target_w = fb_d->width;
                            uint32_t target_h = fb_d->height - 32 - WM_TITLEBAR_HEIGHT;
                            wm_realloc_win_buffer(w, target_w, target_h);
                            w->flags |= WINDOW_FLAG_MAXIMIZED;
                        }
                        last_clicked_win = NULL;
                        is_dragging_outline = false;
                        wm_compose();
                        return;
                    }
                }
                last_clicked_win = w;
                last_click_time = click_counter;

                wm_bring_to_front(w);

                /* Ne deplacer que si non maximise */
                if (!(w->flags & WINDOW_FLAG_MAXIMIZED)) {
                    is_dragging_outline = true;
                    drag_orig_x = mx - w->x;
                    drag_orig_y = my - w->y;
                    drag_curr_x = w->x;
                    drag_curr_y = w->y;
                    drag_last_x = w->x;
                    drag_last_y = w->y;
                    drag_w = w->width;
                    drag_h = w->height + WM_TITLEBAR_HEIGHT;
                }

                wm_compose();
                return;
            }

            /* 5. Click 3la Client Area dial la fenetre */
            if (mx >= w->x && mx <= (int32_t)(w->x + w->width) &&
                my > tb_bottom && my <= (int32_t)(w->y + w->height + WM_TITLEBAR_HEIGHT)) {
                wm_bring_to_front(w);
                wm_compose();
                return;
            }
        }
    }

    /* Gestion interactive du redimensionnement (Bloc 4: Etape 18 & 20) */
    if (left_down && is_resizing && active_window) {
        int32_t dx = mx - resize_orig_mx;
        int32_t dy = my - resize_orig_my;

        int32_t new_x = resize_orig_win_x;
        int32_t new_y = resize_orig_win_y;
        int32_t new_w = (int32_t)resize_orig_win_w;
        int32_t new_h = (int32_t)resize_orig_win_h;

        if (active_resize_edge & RESIZE_RIGHT) {
            new_w += dx;
            if (new_w < WM_MIN_WIDTH) new_w = WM_MIN_WIDTH;
        }
        if (active_resize_edge & RESIZE_BOTTOM) {
            new_h += dy;
            if (new_h < WM_MIN_HEIGHT) new_h = WM_MIN_HEIGHT;
        }
        if (active_resize_edge & RESIZE_LEFT) {
            int32_t cand_w = (int32_t)resize_orig_win_w - dx;
            if (cand_w >= WM_MIN_WIDTH) {
                new_w = cand_w;
                new_x = resize_orig_win_x + dx;
            } else {
                new_w = WM_MIN_WIDTH;
                new_x = resize_orig_win_x + (resize_orig_win_w - WM_MIN_WIDTH);
            }
        }
        if (active_resize_edge & RESIZE_TOP) {
            int32_t cand_h = (int32_t)resize_orig_win_h - dy;
            if (cand_h >= WM_MIN_HEIGHT) {
                new_h = cand_h;
                new_y = resize_orig_win_y + dy;
            } else {
                new_h = WM_MIN_HEIGHT;
                new_y = resize_orig_win_y + (resize_orig_win_h - WM_MIN_HEIGHT);
            }
        }

        drag_curr_x = new_x;
        drag_curr_y = new_y;
        drag_w = new_w;
        drag_h = new_h + WM_TITLEBAR_HEIGHT;

        wm_compose();
        draw_wireframe_rect(drag_curr_x, drag_curr_y, drag_w, drag_h);
        fb_flip();
        return;
    } else if (just_released && is_resizing && active_window) {
        /* Etape 19: Fin du resize et reallocation du buffer */
        active_window->x = drag_curr_x;
        active_window->y = drag_curr_y;
        wm_resize_window_buffer(active_window, drag_w, drag_h - WM_TITLEBAR_HEIGHT);
        active_window->flags &= ~WINDOW_FLAG_RESIZING;
        is_resizing = false;
        active_resize_edge = RESIZE_NONE;
        wm_compose();
        return;
    }

    framebuffer_t *fb_dim = fb_get_info();
    if (left_down && is_dragging_outline && active_window && fb_dim) {
        int32_t nx = mx - drag_orig_x;
        int32_t ny = my - drag_orig_y;

        /* Etape 15: Screen bounds clamping */
        int32_t min_x = -((int32_t)drag_w - 40);
        int32_t max_x = (int32_t)fb_dim->width - 40;
        int32_t min_y = 0;
        int32_t max_y = (int32_t)fb_dim->height - 32 - WM_TITLEBAR_HEIGHT;

        if (nx < min_x) nx = min_x;
        if (nx > max_x) nx = max_x;
        if (ny < min_y) ny = min_y;
        if (ny > max_y) ny = max_y;

        if (nx != drag_curr_x || ny != drag_curr_y) {
            drag_curr_x = nx;
            drag_curr_y = ny;
            wm_compose();
            draw_wireframe_rect(drag_curr_x, drag_curr_y, drag_w, drag_h);
            fb_flip();
        }
    } else if (just_released && is_dragging_outline && active_window) {
        active_window->x = drag_curr_x;
        active_window->y = drag_curr_y;
        is_dragging_outline = false;
        wm_compose();
    }
}

static char term_buffer[128];
static int term_buf_len = 0;
static uint32_t term_cursor_x = 10;
static uint32_t term_cursor_y = 145;

window_t *wm_get_active_window(void) {
    return active_window;
}

void wm_cycle_windows(void) {
    if (window_count <= 1) return;

    /* Trouver l index dial la fenetre active */
    int active_idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == active_window) {
            active_idx = i;
            break;
        }
    }

    /* Trouver la fenetre suivante */
    int next_idx = (active_idx - 1 + window_count) % window_count;
    window_t *next_win = windows[next_idx];
    if (next_win) {
        if (next_win->flags & WINDOW_FLAG_MINIMIZED) {
            next_win->flags &= ~WINDOW_FLAG_MINIMIZED;
        }
        wm_bring_to_front(next_win);
        wm_compose();
    }
}


void wm_refresh_window_content(window_t *win) {
    if (!win || !win->visible || !win->buffer) return;
    framebuffer_t *fb = fb_get_info();
    if (!fb || !fb->back_buffer || !fb->virt_addr) return;

    int32_t client_y = win->y + WM_TITLEBAR_HEIGHT;
    for (uint32_t j = 0; j < win->height; j++) {
        int32_t py = client_y + j;
        if (py < 0 || py >= (int32_t)fb->height) continue;

        for (uint32_t i = 0; i < win->width; i++) {
            int32_t px = win->x + i;
            if (px < 0 || px >= (int32_t)fb->width) continue;

            uint32_t color = win->buffer[j * win->width + i];
            uint32_t offset = py * fb->width + px;
            fb->back_buffer[offset] = color;
            fb->virt_addr[offset]   = color;
        }
    }
}

void wm_handle_keyboard(char ch) {
    if (ch == 0x09) {
        wm_cycle_windows();
        return;
    }

    if (!active_window || !active_window->visible) return;

    if (active_window->title[0] == 'T') {
        if (ch == '\n') {
            term_buffer[term_buf_len] = '\0';
            term_cursor_y += 18;
            term_cursor_x = 10;

            if (strcmp(term_buffer, "help") == 0) {
                win_draw_string(active_window, 10, term_cursor_y, "Commandes: help, clear, uname, ls, about, reboot", 0x0000FFCC);
                term_cursor_y += 18;
            } else if (strcmp(term_buffer, "clear") == 0) {
                for (uint32_t p = 0; p < active_window->width * active_window->height; p++) {
                    active_window->buffer[p] = WM_BG_COLOR;
                }
                term_cursor_y = 10;
            } else if (strcmp(term_buffer, "uname") == 0 || strcmp(term_buffer, "uname -a") == 0) {
                win_draw_string(active_window, 10, term_cursor_y, "YOS 64-bit Kernel v1.0 SMP x86_64 [Build 2026]", 0x00CCCCCC);
                term_cursor_y += 18;
            } else if (strcmp(term_buffer, "ls") == 0 || strcmp(term_buffer, "ls -l /") == 0) {
                win_draw_string(active_window, 10, term_cursor_y, "drwx------  devfs      drwx------ procfs", 0x0000AAFF);
                term_cursor_y += 18;
                win_draw_string(active_window, 10, term_cursor_y, "-rwxr-xr-x  hello.elf  -rw-r--r-- notes.txt", 0x00FFFFFF);
                term_cursor_y += 18;
            } else if (strcmp(term_buffer, "about") == 0) {
                win_draw_string(active_window, 10, term_cursor_y, "YOS 64: OS modulaire x86_64 avec fenetrage complet", 0x00FFAA00);
                term_cursor_y += 18;
            } else if (strcmp(term_buffer, "reboot") == 0) {
                __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
            } else if (term_buf_len > 0) {
                win_draw_string(active_window, 10, term_cursor_y, "Commande introuvable. Tapez help", 0x00FF5555);
                term_cursor_y += 18;
            }

            term_buf_len = 0;

            if (term_cursor_y + 36 >= active_window->height) {
                for (uint32_t p = 0; p < active_window->width * active_window->height; p++) {
                    active_window->buffer[p] = WM_BG_COLOR;
                }
                term_cursor_y = 10;
            }

            win_draw_string(active_window, 10, term_cursor_y, "root@yos-kernel:~# ", 0x0000FF00);
            term_cursor_x = 10 + 19 * 8;
            wm_refresh_window_content(active_window);
        } else if (ch == '\b') {
            if (term_buf_len > 0) {
                term_buf_len--;
                term_cursor_x -= 8;
                for (int r = 0; r < 16; r++) {
                    for (int col = 0; col < 8; col++) {
                        active_window->buffer[(term_cursor_y + r) * active_window->width + (term_cursor_x + col)] = WM_BG_COLOR;
                    }
                }
                wm_refresh_window_content(active_window);
            }
        } else if (ch >= 32 && ch <= 126) {
            if (term_buf_len < 120 && term_cursor_x + 8 < active_window->width) {
                term_buffer[term_buf_len++] = ch;
                char str[2] = {ch, '\0'};
                win_draw_string(active_window, term_cursor_x, term_cursor_y, str, 0x00FFFFFF);
                term_cursor_x += 8;
                wm_refresh_window_content(active_window);
            }
        }
    }
}

void wm_update_clock(void) {
    framebuffer_t *fb = fb_get_info();
    if (!fb) return;

    uint32_t bar_h = 32;
    uint32_t bar_y = fb->height - bar_h;
    uint32_t clock_box_w = 80;
    uint32_t clock_box_x = fb->width - clock_box_w - 8;

    rtc_time_t t;
    rtc_read_time(&t);

    char clock_str[9];
    clock_str[0] = '0' + ((t.hour / 10) % 10);
    clock_str[1] = '0' + (t.hour % 10);
    clock_str[2] = ':';
    clock_str[3] = '0' + ((t.minute / 10) % 10);
    clock_str[4] = '0' + (t.minute % 10);
    clock_str[5] = ':';
    clock_str[6] = '0' + ((t.second / 10) % 10);
    clock_str[7] = '0' + (t.second % 10);
    clock_str[8] = '\0';

    /* Cadre & fond direct */
    fb_draw_rect(clock_box_x, bar_y + 4, clock_box_w, 24, 0x002A475E);
    fb_draw_rect(clock_box_x + 1, bar_y + 5, clock_box_w - 2, 22, 0x00171A21);
    fb_draw_string(clock_box_x + 8, bar_y + 8, clock_str, 0x0000FFCC, 0xFFFFFFFF);

    /* Flip khfif dial zone clock safi */
    fb_flip_rect(clock_box_x, bar_y + 4, clock_box_w, 24);
}



void win_draw_rect(window_t *win, int32_t rx, int32_t ry, uint32_t rw, uint32_t rh, uint32_t color) {
    if (!win || !win->buffer) return;
    for (uint32_t dy = 0; dy < rh; dy++) {
        int32_t py = ry + (int32_t)dy;
        if (py < 0 || (uint32_t)py >= win->height) continue;
        for (uint32_t dx = 0; dx < rw; dx++) {
            int32_t px = rx + (int32_t)dx;
            if (px < 0 || (uint32_t)px >= win->width) continue;
            win->buffer[(uint32_t)py * win->width + (uint32_t)px] = color;
        }
    }
}


void wm_stress_test(void) {
    window_t *test_wins[8];
    char tbuf[16];

    /* 1. Allocation & Ouverture en cascade */
    for (int i = 0; i < 6; i++) {
        tbuf[0] = 'W';
        tbuf[1] = 'i';
        tbuf[2] = 'n';
        tbuf[3] = 0x30 + i;
        tbuf[4] = 0;
        test_wins[i] = wm_create_window(50 + (i * 30), 50 + (i * 25), 180, 100, tbuf);
    }
    wm_compose();

    /* 2. Fermeture propre de toutes les fenetres de test */
    for (int i = 0; i < 6; i++) {
        if (test_wins[i]) {
            wm_destroy_window(test_wins[i]);
        }
    }
    wm_compose();
}
