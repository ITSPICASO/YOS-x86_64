#include "bmp.h"

void bmp_draw(const void *bmp_data, size_t size, uint32_t *target_buf, uint32_t buf_w, uint32_t buf_h, int dst_x, int dst_y) {
    if (!bmp_data || size < sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t)) return;

    const bmp_file_header_t *file_hdr = (const bmp_file_header_t *)bmp_data;
    if (file_hdr->bfType != 0x4D42) return; // Pas un fichier BMP 'BM'

    const bmp_info_header_t *info_hdr = (const bmp_info_header_t *)((const uint8_t *)bmp_data + sizeof(bmp_file_header_t));
    if (info_hdr->biCompression != 0) return; // Uniquement BI_RGB non compresse

    int width = info_hdr->biWidth;
    int height = info_hdr->biHeight;
    int bpp = info_hdr->biBitCount;
    if (bpp != 24 && bpp != 32) return;

    int is_bottom_up = (height > 0);
    if (height < 0) height = -height;

    const uint8_t *pixel_data = (const uint8_t *)bmp_data + file_hdr->bfOffBits;
    int row_stride = ((width * (bpp / 8) + 3) & ~3); // Aligne sur 4 octets

    for (int y = 0; y < height; y++) {
        int src_y = is_bottom_up ? (height - 1 - y) : y;
        int target_y = dst_y + y;
        if (target_y < 0 || (uint32_t)target_y >= buf_h) continue;

        const uint8_t *row = pixel_data + (src_y * row_stride);

        for (int x = 0; x < width; x++) {
            int target_x = dst_x + x;
            if (target_x < 0 || (uint32_t)target_x >= buf_w) continue;

            uint32_t color = 0;
            if (bpp == 24) {
                uint8_t b = row[x * 3 + 0];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];
                color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            } else if (bpp == 32) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }

            target_buf[target_y * buf_w + target_x] = color;
        }
    }
}
