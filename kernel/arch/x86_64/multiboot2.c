#include "multiboot2.h"
#include "serial.h"

fb_info_t fb_info = {0};

fb_info_t *multiboot_get_fb(void) {
    return &fb_info;
}


uint64_t module_start_addr = 0;
uint64_t module_total_size = 0;

uint64_t multiboot_get_module_start(void) {
    return module_start_addr;
}

uint64_t multiboot_get_module_size(void) {
    return module_total_size;
}

void multiboot_parse_mmap(uint64_t addr) {
    uint32_t total_size = *(uint32_t *)addr;
    uint64_t current_addr = addr + 8;

    while (current_addr < addr + total_size) {
        struct multiboot_tag *tag = (struct multiboot_tag *)current_addr;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) break;

                if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer_common *fb = (struct multiboot_tag_framebuffer_common *)tag;
            fb_info.addr   = fb->framebuffer_addr;
            fb_info.pitch  = fb->framebuffer_pitch;
            fb_info.width  = fb->framebuffer_width;
            fb_info.height = fb->framebuffer_height;
            fb_info.bpp    = fb->framebuffer_bpp;

            serial_print("[+] Multiboot2: Linear Framebuffer GOP detecte!\n");
            serial_print("  -> VRAM Physique: 0x");
            serial_print_hex(fb_info.addr);
            serial_print(" | Resolution: ");
            serial_print_dec(fb_info.width);
            serial_print("x");
            serial_print_dec(fb_info.height);
            serial_print("x");
            serial_print_dec(fb_info.bpp);
            serial_print(" | Pitch: ");
            serial_print_dec(fb_info.pitch);
            serial_print(" octets\n");
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
            module_start_addr = (uint64_t)mod->mod_start;
            module_total_size = (uint64_t)(mod->mod_end - mod->mod_start);

            serial_print("[+] Multiboot2: Module Initrd detecte!\n");
            serial_print("  -> Base Virtuelle : 0x");
            serial_print_hex(module_start_addr);
            serial_print(" | Taille: ");
            serial_print_dec(module_total_size);
            serial_print(" octets\n");
        }

        current_addr += (tag->size + 7) & ~7ULL;
    }
}
