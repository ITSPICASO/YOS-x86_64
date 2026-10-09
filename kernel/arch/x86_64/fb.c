#include "font.h"
#include "pmm.h"
#include "fb.h"
#include "multiboot2.h"
#include "vmm.h"
#include "heap.h"
#include "serial.h"
#include "string.h"

#define FB_VIRT_BASE 0xFFFFFFFF90000000ULL
#define BACKBUFFER_VIRT_BASE 0xFFFFFFFF92000000ULL

static framebuffer_t g_fb = {0};

void fb_init(void) {
    fb_info_t *info = multiboot_get_fb();
    if (!info || info->addr == 0 || info->width == 0 || info->height == 0) {
        serial_print("[-] Framebuffer GOP non detecte via Multiboot2!\n");
        return;
    }

    g_fb.phys_addr = info->addr;
    g_fb.width     = info->width;
    g_fb.height    = info->height;
    g_fb.pitch     = info->pitch;
    g_fb.bpp       = info->bpp;
    g_fb.virt_addr = (uint32_t *)FB_VIRT_BASE;

    /* Etape 75: Mapping de la VRAM en memoire virtuelle */
    uint64_t total_bytes = (uint64_t)g_fb.pitch * g_fb.height;
    uint64_t num_pages   = (total_bytes + 4095) / 4096;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t vaddr = FB_VIRT_BASE + (i * 4096);
        uint64_t paddr = g_fb.phys_addr + (i * 4096);
        vmm_map_page(kernel_pml4, vaddr, paddr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    /* Etape 76: Allocation et mapping du Back Buffer via PMM & VMM */
    g_fb.back_buffer = (uint32_t *)BACKBUFFER_VIRT_BASE;
    for (uint64_t i = 0; i < num_pages; i++) {
        void *pframe = pmm_alloc_page();
        if (!pframe) {
            serial_print("[-] Echec allocation page RAM pour Back Buffer!\n");
            return;
        }
        uint64_t vaddr = BACKBUFFER_VIRT_BASE + (i * 4096);
        vmm_map_page(kernel_pml4, vaddr, (uint64_t)pframe, PAGE_PRESENT | PAGE_WRITABLE);
    }

    serial_print("[+] Framebuffer GOP initialise avec succes!\n");
    serial_print("  -> Resolution: ");
    serial_print_dec(g_fb.width);
    serial_print("x");
    serial_print_dec(g_fb.height);
    serial_print("x");
    serial_print_dec(g_fb.bpp);
    serial_print(" | BackBuffer: pret\n");

    /* Etape 77 & 78: Rendu Graphique + Moteur de texte Bitmap */
    fb_clear(0x001B2838); /* Fond Dark Blue */

    /* Fenetre de test */
    fb_draw_rect(80, 80, 480, 260, 0x002A475E);  /* Bordure */
    fb_draw_rect(82, 82, 476, 30,  0x0000AAFF);  /* Title bar */
    fb_draw_rect(82, 112, 476, 226, 0x00171A21); /* Zone contenu */

    /* Titre et texte */
    fb_draw_string(92, 90, "YOS 64-bit GUI Environment - GOP Display", 0x00FFFFFF, 0xFFFFFFFF);
    fb_draw_string(100, 130, "[+] Bloc 12: GOP & Double Buffering OK!", 0x0055FF55, 0xFFFFFFFF);
    fb_draw_string(100, 160, "[+] Etape 78: Bitmap Font Engine 8x16 OK!", 0x0055FFFF, 0xFFFFFFFF);
    fb_draw_string(100, 190, "Kernel Mode: Higher-Half x86_64", 0x00E5E5E5, 0xFFFFFFFF);
    fb_draw_string(100, 220, "Resolution : 1024x768 (32 bpp)", 0x00FFAA00, 0xFFFFFFFF);
    fb_draw_string(100, 260, "System Ready. Initializing Compositor...", 0x00FFFFFF, 0xFFFFFFFF);

    fb_flip();
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_fb.width || y >= g_fb.height || !g_fb.back_buffer) return;
    uint32_t pixels_per_pitch = g_fb.pitch / 4;
    g_fb.back_buffer[y * pixels_per_pitch + x] = color;
}

void fb_clear(uint32_t color) {
    if (!g_fb.back_buffer) return;
    uint32_t total_pixels = (g_fb.pitch / 4) * g_fb.height;
    for (uint32_t i = 0; i < total_pixels; i++) {
        g_fb.back_buffer[i] = color;
    }
}

void fb_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_fb.back_buffer) return;
    for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
            fb_putpixel(x + i, y + j, color);
        }
    }
}

void fb_flip(void) {
    if (!g_fb.back_buffer || !g_fb.virt_addr) return;
    uint64_t total_bytes = (uint64_t)g_fb.pitch * g_fb.height;
    memcpy(g_fb.virt_addr, g_fb.back_buffer, total_bytes);
}

framebuffer_t *fb_get_info(void) {
    return &g_fb;
}

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if ((uint8_t)c >= 128) return;
    const uint8_t *glyph = font8x16[(uint8_t)c];

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (line & (0x80 >> col)) {
                fb_putpixel(x + col, y + row, fg);
            } else if (bg != 0xFFFFFFFF) { /* Si bg == 0xFFFFFFFF -> Fond transparent */
                fb_putpixel(x + col, y + row, bg);
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg) {
    uint32_t cur_x = x;
    uint32_t cur_y = y;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += FONT_HEIGHT;
        } else {
            fb_draw_char(cur_x, cur_y, *str, fg, bg);
            cur_x += FONT_WIDTH;
        }
        str++;
    }
}

void fb_flip_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!g_fb.back_buffer || !g_fb.virt_addr) return;
    if (x >= g_fb.width || y >= g_fb.height) return;

    if (x + w > g_fb.width)  w = g_fb.width - x;
    if (y + h > g_fb.height) h = g_fb.height - y;

    uint32_t pitch_pixels = g_fb.pitch / 4;
    for (uint32_t row = 0; row < h; row++) {
        uint32_t offset = (y + row) * pitch_pixels + x;
        memcpy(&g_fb.virt_addr[offset], &g_fb.back_buffer[offset], w * sizeof(uint32_t));
    }
}
