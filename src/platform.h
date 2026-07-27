/*
 * Platform abstraction for asset and savegame locations.
 *
 * Frontier opens everything with a bare relative path from the working
 * directory. That works on a desktop and nowhere else: Android has no
 * meaningful working directory, read-only assets live inside the APK, and
 * writable storage is a separate app-private directory.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>
#include <SDL.h>

/* Resolve the asset and save locations. Call once, after SDL_Init. */
void Platform_Init (void);
void Platform_UnInit (void);

/* Open a read-only game asset (sfx/, music/, joystick.ini).
 * On Android SDL_RWFromFile reads these straight out of the APK asset
 * bundle; elsewhere they are relative to the working directory.
 * Returns NULL if missing. The caller closes the handle. */
SDL_RWops *Platform_OpenAsset (const char *name);

/* Read a whole asset into a NUL-terminated buffer. Returns NULL if
 * missing. The caller frees it with SDL_free. If len_out is non-NULL it
 * receives the length excluding the terminator. */
char *Platform_LoadAssetText (const char *name, size_t *len_out);

/* Map a savegame filename to a writable absolute path, written into buf.
 * The name comes from the 68k side, which takes it from in-game text
 * entry, so it is sanitised here: any directory component is stripped so
 * a savegame can never be made to escape the save directory. Returns buf,
 * or NULL if the name is unusable. */
const char *Platform_SavePath (const char *name, char *buf, size_t len);

/* The directory Platform_SavePath writes into, for opendir(). */
const char *Platform_SaveDir (void);

/* Non-zero when touch is the primary input and the on-screen controls
 * should be enabled by default. */
int Platform_IsTouchDevice (void);

#endif /* PLATFORM_H */
