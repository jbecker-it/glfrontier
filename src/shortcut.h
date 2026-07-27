/*
  Hatari
*/

#ifndef HATARI_SHORTCUT_H
#define HATARI_SHORTCUT_H

#include <SDL_keycode.h>

typedef void (*ShortCutFunction_t)(void);

enum {
  SHORTCUT_NOTASSIGNED,
  SHORTCUT_FULLSCREEN,
  SHORTCUT_MOUSEMODE,

  NUM_SHORTCUTS
};

typedef struct {
  /* Must be a full SDL_Keycode: SDL2 encodes non-ASCII keys as
   * SDLK_SCANCODE_MASK|scancode, so SDLK_F11 and friends do not fit in a
   * short and would never match their case labels. */
  SDL_Keycode Key;
  BOOL bShiftPressed;
  BOOL bCtrlPressed;
} SHORTCUT_KEY;

extern char *pszShortCutTextStrings[NUM_SHORTCUTS+1];
extern char *pszShortCutF11TextString[];
extern char *pszShortCutF12TextString[];
extern SHORTCUT_KEY ShortCutKey;

extern void ShortCut_ClearKeys(void);
extern void ShortCut_CheckKeys(void);
extern void ShortCut_FullScreen(void);
extern void ShortCut_MouseMode(void);
extern void ShortCut_ColdReset(void);

#endif /* HATARI_SHORTCUT_H */
