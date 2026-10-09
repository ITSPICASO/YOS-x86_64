#ifndef _BMP_H
#define _BMP_H

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // 'BM' -> 0x4D42
    uint32_t bfSize;      // Taille totale du fichier
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;   // Offset vers les pixels
} bmp_file_header_t;

typedef struct {
    uint32_t biSize;          // Taille du header (40)
    int32_t  biWidth;         // Largeur
    int32_t  biHeight;        // Hauteur (positive: bas en haut)
    uint16_t biPlanes;        // 1
    uint16_t biBitCount;      // 24 ou 32 bits
    uint32_t biCompression;   // 0 = BI_RGB (sans compression)
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmp_info_header_t;
#pragma pack(pop)

void bmp_draw(const void *bmp_data, size_t size, uint32_t *target_buf, uint32_t buf_w, uint32_t buf_h, int dst_x, int dst_y);

#endif
