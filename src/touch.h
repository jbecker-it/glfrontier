/*
 * Touchscreen control layer.
 *
 * This drives the existing abstract input layer in joystick.c rather than
 * talking to the game directly: a touch on a virtual button goes through
 * Keymap_JoystickUpDown, and the virtual stick goes through
 * joystick_motion. That means the touch controls are remapped by editing
 * joystick.ini, exactly like a real gamepad, and the 68k side never learns
 * that touch exists.
 */

#ifndef TOUCH_H
#define TOUCH_H

#include <SDL.h>

/* Where the game's 320x200 image is drawn inside the window, in window
 * pixels. The touch layer needs this both to place controls outside the
 * image when there is room, and to convert touches to game coordinates. */
typedef struct {
	int x, y, w, h;
} TOUCH_VIEWPORT;

void Touch_Init (void);
void Touch_UnInit (void);

/* Recompute the control layout. Call at startup and on every window
 * resize - on a foldable, unfolding is a resize, and the layout has to
 * move because the aspect ratio changes drastically. */
void Touch_Layout (int window_w, int window_h, const TOUCH_VIEWPORT *view);

/* Feed an SDL_FINGERDOWN / FINGERMOTION / FINGERUP event. Returns
 * non-zero if the touch layer consumed it. */
int Touch_HandleEvent (const SDL_Event *event);

/* Per-frame housekeeping: tap-click release timing and press-and-hold
 * auto-repeat. Call once per rendered frame. */
void Touch_Update (void);

/* Draw the overlay. Call after the game image, before RenderPresent. */
void Touch_Render (SDL_Renderer *renderer);

/* Show or hide the on-screen controls. Hidden still leaves the trackpad
 * and stick zones live, so the game stays playable with a clean view. */
void Touch_SetOverlayVisible (int visible);
int Touch_OverlayVisible (void);

/* Enable the whole layer. Off by default on desktop, on for touch
 * devices; --touch / --no-touch override it. */
void Touch_SetEnabled (int enabled);
int Touch_Enabled (void);

#endif /* TOUCH_H */
