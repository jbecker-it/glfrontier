/*
 * Minimal .ini reader, API-compatible with the subset of libini that
 * joystick.c uses.
 *
 * libini is a separate small library that would otherwise have to be
 * cross-compiled for every Android ABI just to parse joystick.ini. This
 * replaces it, and loads through the platform layer so the file can live
 * inside the APK asset bundle.
 *
 * Returned key/value/section pointers are not NUL-terminated; they point
 * into the loaded file image and are valid until ini_close. Each comes
 * with an explicit length, which is how libini behaves too.
 */

#ifndef GLF_INI_H
#define GLF_INI_H

#include <stddef.h>

struct INI;

/* Open by asset name. Returns NULL if the file is missing. */
struct INI *ini_open (const char *name);
void ini_close (struct INI *ini);

/* Advance to the next [section]. Returns 1 on success, 0 at end of file. */
int ini_next_section (struct INI *ini, const char **name, size_t *len);

/* Read the next key=value inside the current section. Returns 1 on
 * success, 0 when the section ends (at EOF or the next section header). */
int ini_read_pair (struct INI *ini, const char **key, size_t *lkey,
		const char **val, size_t *lval);

#endif /* GLF_INI_H */
