/*
  Hatari - keymap.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Map host key presses to Atari ST scan codes.

  Under SDL 1.2 this was done twice over: a 300-entry table indexed by
  SDL keycode (which was dead code - nothing ever read it), and a
  heuristic that guessed the offset between the host's raw PC scancodes
  and the ST's, because SDL 1.2 exposed platform-dependent scancodes.

  SDL2 reports layout-independent USB HID scancodes, which describe the
  physical key - exactly what an ST keyboard matrix needs. So the guessing
  is gone and what is left is one explicit table.
*/

#include <SDL.h>

#include "main.h"
#include "keymap.h"
#include "input.h"
#include "joystick.h"
#include "shortcut.h"
#include "screen.h"

static SDL_Joystick *joystick;

/*-----------------------------------------------------------------------*/
/*
  SDL physical scancode -> Atari ST scan code.

  ST keyboard layout, for reference:

    Esc  F1..F10                                    ($01, $3b..$44)
    ` 1 2 3 4 5 6 7 8 9 0 - = BkSp                  ($29,$02..$0e)
    Tab Q W E R T Y U I O P [ ] Return              ($0f..$1c)
    Ctrl A S D F G H J K L ; ' #                    ($1d..$29)
    LShift \ Z X C V B N M , . / RShift             ($2a..$36)
    Alt Space CapsLock                              ($38,$39,$3a)
    Help Undo, cursor cluster, numeric keypad       ($47..$72)

  Zero means "no ST equivalent"; such keys are dropped.
*/
static const unsigned char ScancodeToST[SDL_NUM_SCANCODES] = {
	[SDL_SCANCODE_ESCAPE]		= 0x01,
	[SDL_SCANCODE_1]		= 0x02,
	[SDL_SCANCODE_2]		= 0x03,
	[SDL_SCANCODE_3]		= 0x04,
	[SDL_SCANCODE_4]		= 0x05,
	[SDL_SCANCODE_5]		= 0x06,
	[SDL_SCANCODE_6]		= 0x07,
	[SDL_SCANCODE_7]		= 0x08,
	[SDL_SCANCODE_8]		= 0x09,
	[SDL_SCANCODE_9]		= 0x0a,
	[SDL_SCANCODE_0]		= 0x0b,
	[SDL_SCANCODE_MINUS]		= 0x0c,
	[SDL_SCANCODE_EQUALS]		= 0x0d,
	[SDL_SCANCODE_BACKSPACE]	= 0x0e,

	[SDL_SCANCODE_TAB]		= 0x0f,
	[SDL_SCANCODE_Q]		= 0x10,
	[SDL_SCANCODE_W]		= 0x11,
	[SDL_SCANCODE_E]		= 0x12,
	[SDL_SCANCODE_R]		= 0x13,
	[SDL_SCANCODE_T]		= 0x14,
	[SDL_SCANCODE_Y]		= 0x15,
	[SDL_SCANCODE_U]		= 0x16,
	[SDL_SCANCODE_I]		= 0x17,
	[SDL_SCANCODE_O]		= 0x18,
	[SDL_SCANCODE_P]		= 0x19,
	[SDL_SCANCODE_LEFTBRACKET]	= 0x1a,
	[SDL_SCANCODE_RIGHTBRACKET]	= 0x1b,
	[SDL_SCANCODE_RETURN]		= 0x1c,

	[SDL_SCANCODE_LCTRL]		= 0x1d,
	[SDL_SCANCODE_RCTRL]		= 0x1d,
	[SDL_SCANCODE_A]		= 0x1e,
	[SDL_SCANCODE_S]		= 0x1f,
	[SDL_SCANCODE_D]		= 0x20,
	[SDL_SCANCODE_F]		= 0x21,
	[SDL_SCANCODE_G]		= 0x22,
	[SDL_SCANCODE_H]		= 0x23,
	[SDL_SCANCODE_J]		= 0x24,
	[SDL_SCANCODE_K]		= 0x25,
	[SDL_SCANCODE_L]		= 0x26,
	[SDL_SCANCODE_SEMICOLON]	= 0x27,
	[SDL_SCANCODE_APOSTROPHE]	= 0x28,
	[SDL_SCANCODE_GRAVE]		= 0x29,
	[SDL_SCANCODE_BACKSLASH]	= 0x2b,

	[SDL_SCANCODE_LSHIFT]		= 0x2a,
	[SDL_SCANCODE_Z]		= 0x2c,
	[SDL_SCANCODE_X]		= 0x2d,
	[SDL_SCANCODE_C]		= 0x2e,
	[SDL_SCANCODE_V]		= 0x2f,
	[SDL_SCANCODE_B]		= 0x30,
	[SDL_SCANCODE_N]		= 0x31,
	[SDL_SCANCODE_M]		= 0x32,
	[SDL_SCANCODE_COMMA]		= 0x33,
	[SDL_SCANCODE_PERIOD]		= 0x34,
	[SDL_SCANCODE_SLASH]		= 0x35,
	[SDL_SCANCODE_RSHIFT]		= 0x36,

	[SDL_SCANCODE_LALT]		= 0x38,
	[SDL_SCANCODE_RALT]		= 0x38,
	[SDL_SCANCODE_SPACE]		= 0x39,
	[SDL_SCANCODE_CAPSLOCK]		= 0x3a,

	[SDL_SCANCODE_F1]		= 0x3b,
	[SDL_SCANCODE_F2]		= 0x3c,
	[SDL_SCANCODE_F3]		= 0x3d,
	[SDL_SCANCODE_F4]		= 0x3e,
	[SDL_SCANCODE_F5]		= 0x3f,
	[SDL_SCANCODE_F6]		= 0x40,
	[SDL_SCANCODE_F7]		= 0x41,
	[SDL_SCANCODE_F8]		= 0x42,
	[SDL_SCANCODE_F9]		= 0x43,
	[SDL_SCANCODE_F10]		= 0x44,

	/* Cursor cluster. Frontier uses these to pan the external view. */
	[SDL_SCANCODE_HOME]		= 0x47,
	[SDL_SCANCODE_UP]		= 0x48,
	[SDL_SCANCODE_LEFT]		= 0x4b,
	[SDL_SCANCODE_RIGHT]		= 0x4d,
	[SDL_SCANCODE_DOWN]		= 0x50,
	[SDL_SCANCODE_INSERT]		= 0x52,
	[SDL_SCANCODE_DELETE]		= 0x53,

	/* The ST's "<>" key, Undo and Help. */
	[SDL_SCANCODE_NONUSBACKSLASH]	= 0x60,
	[SDL_SCANCODE_END]		= 0x60,
	[SDL_SCANCODE_PAGEUP]		= 0x63,
	[SDL_SCANCODE_PAGEDOWN]		= 0x64,

	/* Numeric keypad. Keypad +/- are Frontier's zoom controls. */
	[SDL_SCANCODE_KP_MINUS]		= 0x4a,
	[SDL_SCANCODE_KP_PLUS]		= 0x4e,
	[SDL_SCANCODE_KP_DIVIDE]	= 0x65,
	[SDL_SCANCODE_KP_MULTIPLY]	= 0x66,
	[SDL_SCANCODE_KP_7]		= 0x67,
	[SDL_SCANCODE_KP_8]		= 0x68,
	[SDL_SCANCODE_KP_9]		= 0x69,
	[SDL_SCANCODE_KP_4]		= 0x6a,
	[SDL_SCANCODE_KP_5]		= 0x6b,
	[SDL_SCANCODE_KP_6]		= 0x6c,
	[SDL_SCANCODE_KP_1]		= 0x6d,
	[SDL_SCANCODE_KP_2]		= 0x6e,
	[SDL_SCANCODE_KP_3]		= 0x6f,
	[SDL_SCANCODE_KP_0]		= 0x70,
	[SDL_SCANCODE_KP_PERIOD]	= 0x71,
	[SDL_SCANCODE_KP_ENTER]		= 0x72,
};


/*-----------------------------------------------------------------------*/
/*
  Initialization / Deinitialization.
*/
void Keymap_Init(void)
{
	/* Read the button bindings unconditionally. This used to happen only
	 * when a physical joystick was present, which would leave the touch
	 * overlay with nothing bound on a phone - it reads the same table. */
	joystick_read_config("joystick.ini");

	if (SDL_NumJoysticks())
		joystick = SDL_JoystickOpen(0);
}

void Keymap_UnInit(void)
{
	if (joystick) {
		SDL_JoystickClose(joystick);
		joystick = NULL;
	}
}


/*-----------------------------------------------------------------------*/
/*
  Remap an SDL key event to an ST scan code, or 0 if there is none.
*/
char Keymap_RemapKeyToSTScanCode(SDL_Keysym *keysym)
{
	if (keysym->scancode <= 0 || keysym->scancode >= SDL_NUM_SCANCODES)
		return 0;

	return (char)ScancodeToST[keysym->scancode];
}


/*-----------------------------------------------------------------------*/
/*
  Debounce any key held down if running with key repeat disabled.
  This is called each ST frame.
*/
void Keymap_DebounceAllKeys(void)
{
}


/*-----------------------------------------------------------------------*/
/*
  User pressed a key.
*/
void Keymap_KeyDown(SDL_Keysym *sdlkey)
{
	SDL_Scancode scan = sdlkey->scancode;
	char STScanCode;

	if (scan <= 0 || scan >= SDL_NUM_SCANCODES)
		return;

	/* F11/F12 are host shortcuts, not ST keys. */
	if (sdlkey->sym == SDLK_F11 || sdlkey->sym == SDLK_F12) {
		ShortCutKey.Key = sdlkey->sym;
		return;
	}

	/* Ctrl-<key> combinations are host shortcuts too, and are held back
	 * until the start of the next VBL so they run at a safe point. */
	if (sdlkey->mod & KMOD_CTRL) {
		ShortCutKey.Key = sdlkey->sym;
		ShortCutKey.bCtrlPressed = TRUE;
		if (sdlkey->mod & KMOD_SHIFT)
			ShortCutKey.bShiftPressed = TRUE;
		return;
	}

	STScanCode = Keymap_RemapKeyToSTScanCode(sdlkey);
	if (STScanCode && !input.key_states[scan])
		Input_PressSTKey(STScanCode, TRUE);

	input.key_states[scan] = TRUE;
}


/*-----------------------------------------------------------------------*/
/*
  User released a key.
*/
void Keymap_KeyUp(SDL_Keysym *sdlkey)
{
	SDL_Scancode scan = sdlkey->scancode;
	char STScanCode;

	if (scan <= 0 || scan >= SDL_NUM_SCANCODES)
		return;

	if (sdlkey->sym == SDLK_F11 || sdlkey->sym == SDLK_F12)
		return;

	/* Caps Lock latches on the host but the ST expects a full press and
	 * release, so emit the missing press here. */
	if (sdlkey->sym == SDLK_CAPSLOCK)
		Input_PressSTKey(0x3a, TRUE);

	if (input.key_states[scan]) {
		STScanCode = Keymap_RemapKeyToSTScanCode(sdlkey);
		if (STScanCode)
			Input_PressSTKey(STScanCode, FALSE);
	}

	input.key_states[scan] = FALSE;
}
