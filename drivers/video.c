#include <common.h>
#include <linux/errno.h>
#include <bmp.h>


uint32_t a_pixs[64] = {
    0x3c00, 0x3c00, 0x7e000, 0x7e000,
    0xf7800, 0xf7800, 0x1e3c0, 0x1e3c0, 0x3c1e00, 0x3c1e00,
    0x780e00, 0x780e00, 0xf00780, 0xf00780, 0x1e003c0, 0x1e003c0,
    0x3c001e0, 0x3c001e0, 0x78000f0, 0x78000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xffffff0,
    0xffffff0, 0xffffff0, 0xffffff0, 0xf0000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0,
    0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0, 0xf0000f0
 };


static int width = 1920;
static int height = 1080;
static int pitch = 7680;
static int pixelwidth = 4;

void putpixel(unsigned char* screen, int x,int y, int color) {
    uint8_t *fb = screen + x*pixelwidth + y*pitch;

    *fb++ = color & 0xff;              // BLUE
    *fb++ = (color >> 8) & 0xff;   // GREEN
    *fb++ = (color >> 16) & 0xff;  // RED
}

void fillrect(unsigned char* screen, int x, int y, int w, int h, int color) {
    uint8_t *fb = screen + x*pixelwidth + y*pitch;

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            *fb++ = color & 0xff;              // BLUE
            *fb++ = (color >> 8) & 0xff;   // GREEN
            *fb++ = (color >> 16) & 0xff;  // RED
            *fb++ = 0;
        }
        fb+=pitch-w*pixelwidth;
    }
}


/*
void draw_a(unsigned char* screen, int x, int y) {
    for (l = j = 0; l < h; l++) {
        for (i = 0; i < w; i++, j++) {
            putpixel(FB, x + i, y + l, a_pixs[j]);
        }
    }
}
*/

void video_bmp_display(unsigned char* screen, void *img_buff, int x, int y) {
    struct bmp_image *bmp = (struct bmp_image *)img_buff;
    uint8_t *bmap;
    uint8_t *start, *fb;
    unsigned long width, height, byte_width;
    unsigned colours, bpix, bmp_bpix;

    	if (!bmp || !(bmp->header.signature[0] == 'B' &&
	    bmp->header.signature[1] == 'M')) {
		printf("Error: no valid bmp image at %lx\n", img_buff);

		return -EINVAL;
	}

    bpix = 32;

    width = bmp->header.width;
    height = bmp->header.height;
    bmp_bpix = bmp->header.bit_count;

    printf("Image width %d height %d bpix %d\n", width, height, bmp_bpix);

    bmap = (uint8_t *)bmp + bmp->header.data_offset;
	start = (uint8_t *)(screen +
		(y + height) * 1920*4/*priv->line_length*/ + x * bpix / 8);

	/* Move back to the final line to be drawn */
	fb = start - 1920*4 /*priv->line_length*/;
//    printf("fb 0x%lX bmap 0x%lX\n", fb, bmap);
			for (int i = 0; i < height; ++i) {
				for (int j = 0; j < width; j++) {
						*fb++ = *bmap++;
						*fb++ = *bmap++;
						*fb++ = *bmap++;
						*fb++ = *bmap++;
				}
				fb -= 1920*4/*priv->line_length*/ + width * (bpix / 8);
			}
    flush_dcache_range(FB, FB+FB_SIZE);
}
