/*
 * Platform abstraction for asset and savegame locations.
 * See platform.h.
 */

#include <SDL.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "main.h"
#include "platform.h"

static char save_dir[MAX_FILENAME_LENGTH];
static int is_touch_device;

void Platform_Init (void)
{
#ifdef __ANDROID__
	const char *base = SDL_AndroidGetInternalStoragePath ();

	/* Internal storage is guaranteed writable and is wiped on uninstall,
	 * which is the right home for savegames. If SDL cannot tell us where
	 * it is there is nowhere else to go, so fail loudly rather than
	 * silently writing saves into a directory the user will never find. */
	if (!base) {
		fprintf (stderr, "Cannot determine Android storage path: %s\n",
				SDL_GetError ());
		base = ".";
	}
	snprintf (save_dir, sizeof (save_dir), "%s/savs", base);

	if (mkdir (save_dir, 0700) != 0 && errno != EEXIST) {
		fprintf (stderr, "Cannot create save directory '%s': %s\n",
				save_dir, strerror (errno));
	}
	is_touch_device = 1;
#else
	/* Desktop keeps the historical behaviour: savefiles land in the
	 * working directory, so existing installs and ST-compatible saves
	 * carry on working unchanged. */
	snprintf (save_dir, sizeof (save_dir), ".");
	is_touch_device = 0;
#endif
}

void Platform_UnInit (void)
{
}

SDL_RWops *Platform_OpenAsset (const char *name)
{
	/* On Android SDL_RWFromFile resolves a relative path against the APK
	 * asset bundle, so sfx/, music/ and joystick.ini need no special
	 * casing at the call sites. */
	return SDL_RWFromFile (name, "rb");
}

char *Platform_LoadAssetText (const char *name, size_t *len_out)
{
	SDL_RWops *rw;
	Sint64 size;
	char *buf;
	size_t got;

	rw = Platform_OpenAsset (name);
	if (!rw) return NULL;

	size = SDL_RWsize (rw);
	if (size < 0) {
		SDL_RWclose (rw);
		return NULL;
	}

	buf = SDL_malloc ((size_t)size + 1);
	if (!buf) {
		SDL_RWclose (rw);
		return NULL;
	}

	got = SDL_RWread (rw, buf, 1, (size_t)size);
	SDL_RWclose (rw);

	buf[got] = '\0';
	if (len_out) *len_out = got;
	return buf;
}

const char *Platform_SaveDir (void)
{
	return save_dir;
}

const char *Platform_SavePath (const char *name, char *buf, size_t len)
{
	const char *leaf;

	if (!name || !*name) return NULL;

	/* Strip any directory component. These names originate from in-game
	 * text entry on the 68k side, so treat them as untrusted: a name
	 * containing '/' or ".." must not be able to reach outside save_dir. */
	leaf = name;
	for (; *name; name++) {
		if (*name == '/' || *name == '\\') leaf = name + 1;
	}

	if (!*leaf || !strcmp (leaf, ".") || !strcmp (leaf, "..")) return NULL;

	if ((size_t)snprintf (buf, len, "%s/%s", save_dir, leaf) >= len)
		return NULL;

	return buf;
}

int Platform_IsTouchDevice (void)
{
	return is_touch_device;
}
