/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --font assets\fonts\ProggyCleanSZ-1.ttf --range 0x20-0x7E --size 16 --bpp 1 --format lvgl --no-compress --lv-font-name proggy_16 --lv-include lvgl.h --output assets\fonts\proggy_16.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef PROGGY_16
#define PROGGY_16 1
#endif

#if PROGGY_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xfd,

    /* U+0022 "\"" */
    0xb6, 0x80,

    /* U+0023 "#" */
    0x14, 0x29, 0xf9, 0x42, 0x9f, 0x94, 0x28,

    /* U+0024 "$" */
    0x23, 0xe9, 0x47, 0x14, 0xbe, 0x20,

    /* U+0025 "%" */
    0x45, 0x52, 0xa2, 0x81, 0x45, 0x4a, 0xa2,

    /* U+0026 "&" */
    0x62, 0x49, 0x19, 0x96, 0x28, 0x9d,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x29, 0x49, 0x24, 0x48, 0x80,

    /* U+0029 ")" */
    0x89, 0x12, 0x49, 0x4a, 0x0,

    /* U+002A "*" */
    0x25, 0x5d, 0x52, 0x0,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0x56,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x8, 0x44, 0x22, 0x11, 0x8, 0x84, 0x0,

    /* U+0030 "0" */
    0x74, 0x67, 0x5a, 0xe6, 0x2e,

    /* U+0031 "1" */
    0x23, 0x28, 0x42, 0x10, 0x9f,

    /* U+0032 "2" */
    0x74, 0x42, 0x22, 0x22, 0x1f,

    /* U+0033 "3" */
    0x74, 0x42, 0x60, 0x86, 0x2e,

    /* U+0034 "4" */
    0x8, 0x62, 0x92, 0x8b, 0xf0, 0x82,

    /* U+0035 "5" */
    0xfc, 0x21, 0xe0, 0x86, 0x2e,

    /* U+0036 "6" */
    0x32, 0x21, 0xe8, 0xc6, 0x2e,

    /* U+0037 "7" */
    0xf8, 0x44, 0x22, 0x11, 0x8,

    /* U+0038 "8" */
    0x74, 0x62, 0xe8, 0xc6, 0x2e,

    /* U+0039 "9" */
    0x74, 0x63, 0x17, 0x84, 0x4c,

    /* U+003A ":" */
    0xcc,

    /* U+003B ";" */
    0x50, 0x56,

    /* U+003C "<" */
    0xc, 0xcc, 0xc, 0xc,

    /* U+003D "=" */
    0xfc, 0xf, 0xc0,

    /* U+003E ">" */
    0xc0, 0xc0, 0xcc, 0xc0,

    /* U+003F "?" */
    0x74, 0x42, 0x22, 0x10, 0x4,

    /* U+0040 "@" */
    0x38, 0x8a, 0x6d, 0x5a, 0xb3, 0x90, 0x1e,

    /* U+0041 "A" */
    0x30, 0xc4, 0x92, 0x7a, 0x18, 0x61,

    /* U+0042 "B" */
    0xf2, 0x28, 0xbe, 0x86, 0x18, 0x7e,

    /* U+0043 "C" */
    0x39, 0x18, 0x20, 0x82, 0x4, 0x4e,

    /* U+0044 "D" */
    0xf2, 0x28, 0x61, 0x86, 0x18, 0xbc,

    /* U+0045 "E" */
    0xfc, 0x21, 0xe8, 0x42, 0x1f,

    /* U+0046 "F" */
    0xfc, 0x21, 0xe8, 0x42, 0x10,

    /* U+0047 "G" */
    0x39, 0x18, 0x20, 0x9e, 0x14, 0x4e,

    /* U+0048 "H" */
    0x86, 0x18, 0x7f, 0x86, 0x18, 0x61,

    /* U+0049 "I" */
    0xe9, 0x24, 0x97,

    /* U+004A "J" */
    0x71, 0x11, 0x11, 0x1e,

    /* U+004B "K" */
    0x86, 0x29, 0x28, 0xe2, 0x48, 0xa1,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x1f,

    /* U+004D "M" */
    0xc7, 0x8e, 0xad, 0x59, 0x32, 0x60, 0xc1,

    /* U+004E "N" */
    0xc7, 0x1a, 0x69, 0x96, 0x58, 0xe3,

    /* U+004F "O" */
    0x31, 0x28, 0x61, 0x86, 0x14, 0x8c,

    /* U+0050 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x10,

    /* U+0051 "Q" */
    0x31, 0x28, 0x61, 0x86, 0x14, 0x8d, 0x4,

    /* U+0052 "R" */
    0xf2, 0x28, 0xa2, 0xf2, 0x48, 0xa1,

    /* U+0053 "S" */
    0x7a, 0x18, 0x18, 0x18, 0x18, 0x5e,

    /* U+0054 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8,

    /* U+0055 "U" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x5e,

    /* U+0056 "V" */
    0x83, 0x5, 0x12, 0x22, 0x85, 0x4, 0x8,

    /* U+0057 "W" */
    0x83, 0x26, 0x4d, 0x5a, 0xad, 0x91, 0x22,

    /* U+0058 "X" */
    0x86, 0x14, 0x8c, 0x31, 0x28, 0x61,

    /* U+0059 "Y" */
    0x83, 0x5, 0x11, 0x41, 0x2, 0x4, 0x8,

    /* U+005A "Z" */
    0xfc, 0x10, 0x84, 0x21, 0x8, 0x3f,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x93, 0x80,

    /* U+005C "\\" */
    0x84, 0x10, 0x82, 0x10, 0x42, 0x8, 0x40,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x27, 0x80,

    /* U+005E "^" */
    0x21, 0x14, 0xa8, 0xc4,

    /* U+005F "_" */
    0xfe,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x70, 0x5f, 0x18, 0xbc,

    /* U+0062 "b" */
    0x84, 0x21, 0xe8, 0xc6, 0x31, 0xf0,

    /* U+0063 "c" */
    0x74, 0x61, 0x8, 0xb8,

    /* U+0064 "d" */
    0x8, 0x42, 0xf8, 0xc6, 0x31, 0x78,

    /* U+0065 "e" */
    0x74, 0x7f, 0x8, 0xb8,

    /* U+0066 "f" */
    0x3a, 0x11, 0xe4, 0x21, 0x8, 0x40,

    /* U+0067 "g" */
    0x7c, 0x63, 0x18, 0xbc, 0x21, 0x70,

    /* U+0068 "h" */
    0x84, 0x21, 0xe8, 0xc6, 0x31, 0x88,

    /* U+0069 "i" */
    0x43, 0x55, 0x40,

    /* U+006A "j" */
    0x10, 0x3, 0x11, 0x11, 0x11, 0xe0,

    /* U+006B "k" */
    0x84, 0x21, 0x19, 0x53, 0x92, 0x88,

    /* U+006C "l" */
    0xd5, 0x55, 0x40,

    /* U+006D "m" */
    0xed, 0x26, 0x4c, 0x99, 0x32, 0x40,

    /* U+006E "n" */
    0xf4, 0x63, 0x18, 0xc4,

    /* U+006F "o" */
    0x74, 0x63, 0x18, 0xb8,

    /* U+0070 "p" */
    0xf4, 0x63, 0x18, 0xfa, 0x10, 0x80,

    /* U+0071 "q" */
    0x7c, 0x63, 0x18, 0xbc, 0x21, 0x8,

    /* U+0072 "r" */
    0xb6, 0x61, 0x8, 0x40,

    /* U+0073 "s" */
    0x7c, 0x18, 0x20, 0xf8,

    /* U+0074 "t" */
    0x88, 0xf8, 0x88, 0x87,

    /* U+0075 "u" */
    0x8c, 0x63, 0x18, 0xbc,

    /* U+0076 "v" */
    0x8c, 0x54, 0xa2, 0x10,

    /* U+0077 "w" */
    0x83, 0x26, 0x4d, 0x56, 0xc8, 0x80,

    /* U+0078 "x" */
    0x8a, 0x88, 0x45, 0x44,

    /* U+0079 "y" */
    0x8c, 0x63, 0x18, 0xbc, 0x21, 0x70,

    /* U+007A "z" */
    0xf8, 0x44, 0x44, 0x7c,

    /* U+007B "{" */
    0x19, 0x8, 0x42, 0x60, 0x84, 0x21, 0x6,

    /* U+007C "|" */
    0xff, 0xe0,

    /* U+007D "}" */
    0xc1, 0x8, 0x42, 0xc, 0x84, 0x21, 0x30,

    /* U+007E "~" */
    0x73, 0x38
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 112, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 112, .box_w = 1, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 112, .box_w = 3, .box_h = 3, .ofs_x = 2, .ofs_y = 6},
    {.bitmap_index = 4, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 11, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 17, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 24, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 30, .adv_w = 112, .box_w = 1, .box_h = 3, .ofs_x = 3, .ofs_y = 6},
    {.bitmap_index = 31, .adv_w = 112, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 36, .adv_w = 112, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 41, .adv_w = 112, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 45, .adv_w = 112, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 49, .adv_w = 112, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 50, .adv_w = 112, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 51, .adv_w = 112, .box_w = 1, .box_h = 2, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 52, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 59, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 79, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 90, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 100, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 105, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 112, .box_w = 1, .box_h = 6, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 112, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 113, .adv_w = 112, .box_w = 6, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 117, .adv_w = 112, .box_w = 6, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 120, .adv_w = 112, .box_w = 6, .box_h = 5, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 124, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 136, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 154, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 176, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 112, .box_w = 3, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 112, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 189, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 195, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 219, .adv_w = 112, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 112, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 231, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 263, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 276, .adv_w = 112, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 283, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 289, .adv_w = 112, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 294, .adv_w = 112, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 301, .adv_w = 112, .box_w = 3, .box_h = 11, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 306, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 310, .adv_w = 112, .box_w = 7, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 311, .adv_w = 112, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 7},
    {.bitmap_index = 312, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 316, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 322, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 326, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 332, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 342, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 348, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 354, .adv_w = 112, .box_w = 2, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 357, .adv_w = 112, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 363, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 112, .box_w = 2, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 112, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 382, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 386, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 392, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 398, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 402, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 406, .adv_w = 112, .box_w = 4, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 414, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 418, .adv_w = 112, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 424, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 428, .adv_w = 112, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 434, .adv_w = 112, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 438, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 445, .adv_w = 112, .box_w = 1, .box_h = 11, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 447, .adv_w = 112, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 454, .adv_w = 112, .box_w = 7, .box_h = 2, .ofs_x = 0, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t proggy_16 = {
#else
lv_font_t proggy_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 12,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if PROGGY_16*/

