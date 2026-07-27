/*
 * Minimal .ini reader. See ini.h.
 */

#include <SDL.h>
#include <ctype.h>

#include "main.h"
#include "platform.h"
#include "ini.h"

struct INI {
	char *buf;	/* whole file, NUL terminated */
	char *pos;	/* read cursor */
};

struct INI *ini_open (const char *name)
{
	struct INI *ini;
	char *buf;

	buf = Platform_LoadAssetText (name, NULL);
	if (!buf) return NULL;

	ini = SDL_malloc (sizeof (*ini));
	if (!ini) {
		SDL_free (buf);
		return NULL;
	}

	ini->buf = buf;
	ini->pos = buf;
	return ini;
}

void ini_close (struct INI *ini)
{
	if (!ini) return;
	SDL_free (ini->buf);
	SDL_free (ini);
}

/* Advance past the current line. */
static void skip_line (struct INI *ini)
{
	while (*ini->pos && *ini->pos != '\n') ini->pos++;
	if (*ini->pos == '\n') ini->pos++;
}

/* Position at the first line that is neither blank nor a comment.
 * Returns 0 at end of file. */
static int skip_blanks (struct INI *ini)
{
	for (;;) {
		char *p = ini->pos;

		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		ini->pos = p;

		if (!*p) return 0;
		if (*p != '#' && *p != ';') return 1;

		skip_line (ini);
	}
}

/* Length of the line at p with trailing whitespace removed. */
static size_t trimmed_line_len (const char *p)
{
	size_t len = 0;

	while (p[len] && p[len] != '\n') len++;
	while (len && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
				p[len - 1] == '\r')) len--;
	return len;
}

int ini_next_section (struct INI *ini, const char **name, size_t *len)
{
	if (!ini) return 0;

	while (skip_blanks (ini)) {
		char *p = ini->pos;
		size_t n;

		if (*p != '[') {
			/* A stray key outside any section. Ignore it. */
			skip_line (ini);
			continue;
		}

		p++;
		n = 0;
		while (p[n] && p[n] != ']' && p[n] != '\n') n++;

		/* Unterminated header - treat as malformed and skip. */
		if (p[n] != ']') {
			skip_line (ini);
			continue;
		}

		skip_line (ini);

		if (name) *name = p;
		if (len) *len = n;
		return 1;
	}

	return 0;
}

int ini_read_pair (struct INI *ini, const char **key, size_t *lkey,
		const char **val, size_t *lval)
{
	char *p, *eq;
	size_t line_len, n;

	if (!ini) return 0;
	if (!skip_blanks (ini)) return 0;

	p = ini->pos;

	/* The section ended. Leave the cursor on the header so the next
	 * ini_next_section picks it up. */
	if (*p == '[') return 0;

	line_len = trimmed_line_len (p);

	eq = memchr (p, '=', line_len);
	if (!eq) {
		/* Not a key=value line. Skip it rather than ending the
		 * section, so one malformed line does not hide the rest. */
		skip_line (ini);
		return ini_read_pair (ini, key, lkey, val, lval);
	}

	/* Key: from p up to '=', right-trimmed. */
	n = (size_t)(eq - p);
	while (n && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
	if (key) *key = p;
	if (lkey) *lkey = n;

	/* Value: after '=', left-trimmed, to the trimmed end of line. */
	eq++;
	while (eq < p + line_len && (*eq == ' ' || *eq == '\t')) eq++;
	if (val) *val = eq;
	if (lval) *lval = (size_t)((p + line_len) - eq);

	skip_line (ini);
	return 1;
}
