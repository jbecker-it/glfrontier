/*
 * Embedded mouse cursor, 11x11, generated from cursor.png.
 *
 * Inlined so the port does not have to cross-compile SDL2_image for a
 * single 158-byte image. Values index CURSOR_PALETTE below.
 */

#ifndef CURSOR_DATA_H
#define CURSOR_DATA_H

#define CURSOR_W	11
#define CURSOR_H	11

/* RGBA8888, entry 0 is fully transparent. */
static const unsigned char CURSOR_PALETTE[4][4] = {
	{   0,   0,   0,   0 },
	{   0,   0,   0, 255 },
	{ 212,  28,  59, 255 },
	{ 234, 208, 128, 255 },
};

static const unsigned char CURSOR_PIXELS[CURSOR_W * CURSOR_H] = {
	1,1,1,1,1,1,0,0,0,0,0,
	1,3,3,3,3,3,1,0,0,0,0,
	1,2,2,2,2,3,1,0,0,0,0,
	1,2,2,2,3,1,0,0,0,0,0,
	1,2,2,2,2,3,1,0,0,0,0,
	1,2,2,1,2,2,3,1,0,0,0,
	0,1,1,0,1,2,2,3,1,0,0,
	0,0,0,0,0,1,2,2,3,1,0,
	0,0,0,0,0,0,1,2,2,3,1,
	0,0,0,0,0,0,0,1,2,1,0,
	0,0,0,0,0,0,0,0,1,0,0,
};

#endif /* CURSOR_DATA_H */
