/*
 * Touchscreen control layer. See touch.h.
 *
 * Everything here funnels into joystick.c's abstract input entry points,
 * so the on-screen control set is whatever joystick.ini binds for the
 * current mode - bind a new action there and a button for it appears.
 */

#include <SDL.h>

#include "main.h"
#include "input.h"
#include "joystick.h"
#include "platform.h"
#include "screen.h"
#include "touch.h"
#include "font5x7.h"

#define MAX_FINGERS		8
#define MAX_BUTTONS		16

/* A tap is a touch that lifts quickly without travelling far. Anything
 * longer or further is a drag of the cursor. */
#define TAP_MAX_MS		280
#define TAP_MAX_TRAVEL		18.0f

/* Holding still on the trackpad starts auto-repeating clicks. Frontier
 * makes you click the same icon dozens of times to repair a hull, which
 * is merely tedious with a mouse and intolerable with a finger. */
#define HOLD_REPEAT_DELAY_MS	500
#define HOLD_REPEAT_MS		110

/* A synthesised click must stay down long enough for the 68k side to
 * actually poll it. The game reads the mouse once per VBL (20ms), so
 * hold for a few frames' worth. */
#define CLICK_HOLD_MS		70

/* Virtual stick travel, as a fraction of the smaller window dimension. */
#define STICK_RADIUS_FRAC	0.13f
#define STICK_DEADZONE_FRAC	0.18f

/* Cursor speed multiplier for trackpad drags. Above 1.0 so crossing the
 * whole cockpit does not need several swipes. */
#define TRACKPAD_ACCEL		1.6f

enum finger_role {
	ROLE_NONE = 0,
	ROLE_BUTTON,
	ROLE_STICK,
	ROLE_TRACKPAD,
	ROLE_OVERLAY_TOGGLE
};

typedef struct {
	SDL_FingerID id;
	int in_use;
	enum finger_role role;
	int button;		/* ROLE_BUTTON: joystick button index */
	float anchor_x, anchor_y;
	float x, y;
	Uint32 down_ms;
	float travel;
	int repeating;
	Uint32 next_repeat_ms;
} FINGER;

typedef struct {
	int idx;		/* abstract joystick button index */
	int cx, cy, r;
	int shown;
	int held;
	const char *label;
} TBUTTON;

static int enabled;
static int overlay_visible = 1;

static int win_w = 1, win_h = 1;
static TOUCH_VIEWPORT viewport;

static FINGER fingers[MAX_FINGERS];
static TBUTTON buttons[MAX_BUTTONS];

/* Stick hint ring, and the region a touch must start in to grab the
 * stick rather than anything else. */
static int stick_cx, stick_cy, stick_r;
static SDL_Rect stick_zone;
static int stick_active;
static float stick_dx, stick_dy;

/* Overlay show/hide chip. */
static SDL_Rect toggle_rect;

/* Pending synthesised click. */
static int click_down;
static Uint32 click_release_ms;

/* Which mode the layout was built for, so we can rebuild when it
 * changes - the control set differs between mouse and flight modes. */
static int layout_mouse_mode = -1;

/* ------------------------------------------------------------------ */
/* Drawing helpers                                                      */
/* ------------------------------------------------------------------ */

/* Span batching. Circles are drawn as one-pixel-tall rects, and a large
 * screen makes for a lot of them, so flush whenever the batch fills
 * rather than silently drawing a partial shape. */
#define SPAN_BATCH	128

typedef struct {
	SDL_Renderer *r;
	SDL_Rect rects[SPAN_BATCH];
	int n;
} SPANS;

static void spans_begin (SPANS *s, SDL_Renderer *r)
{
	s->r = r;
	s->n = 0;
}

static void spans_flush (SPANS *s)
{
	if (s->n) SDL_RenderFillRects (s->r, s->rects, s->n);
	s->n = 0;
}

static void spans_add (SPANS *s, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0) return;
	if (s->n == SPAN_BATCH) spans_flush (s);

	s->rects[s->n].x = x;
	s->rects[s->n].y = y;
	s->rects[s->n].w = w;
	s->rects[s->n].h = h;
	s->n++;
}

static void fill_circle (SDL_Renderer *r, int cx, int cy, int rad)
{
	SPANS s;
	int dy;

	if (rad <= 0) return;

	spans_begin (&s, r);
	for (dy = -rad; dy <= rad; dy++) {
		int dx = (int)SDL_sqrt ((double)rad * rad - (double)dy * dy);

		spans_add (&s, cx - dx, cy + dy, 2 * dx + 1, 1);
	}
	spans_flush (&s);
}

static void fill_ring (SDL_Renderer *r, int cx, int cy, int rad, int thick)
{
	SPANS s;
	int dy, inner = rad - thick;

	if (rad <= 0 || thick <= 0) return;
	if (inner < 0) inner = 0;

	spans_begin (&s, r);
	for (dy = -rad; dy <= rad; dy++) {
		int dx = (int)SDL_sqrt ((double)rad * rad - (double)dy * dy);
		int ix = 0;

		if (dy > -inner && dy < inner)
			ix = (int)SDL_sqrt ((double)inner * inner - (double)dy * dy);

		if (ix == 0) {
			spans_add (&s, cx - dx, cy + dy, 2 * dx + 1, 1);
		} else {
			spans_add (&s, cx - dx, cy + dy, dx - ix + 1, 1);
			spans_add (&s, cx + ix, cy + dy, dx - ix + 1, 1);
		}
	}
	spans_flush (&s);
}

static int font_index (char c)
{
	const char *p;

	if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
	p = SDL_strchr (FONT_CHARS, c);
	return p ? (int)(p - FONT_CHARS) : 0;	/* unknown -> space */
}

static int text_width (const char *s, int scale)
{
	int n = (int)SDL_strlen (s);

	if (n <= 0) return 0;
	return (n * (FONT_W + 1) - 1) * scale;
}

static void draw_text (SDL_Renderer *r, int x, int y, int scale, const char *s)
{
	SPANS sp;

	spans_begin (&sp, r);

	for (; *s; s++) {
		const unsigned char *g = FONT_GLYPHS[font_index (*s)];
		int col;

		for (col = 0; col < FONT_W; col++) {
			int row;

			for (row = 0; row < FONT_H; row++) {
				if (!(g[col] & (1 << row))) continue;
				spans_add (&sp, x + col * scale, y + row * scale,
						scale, scale);
			}
		}
		x += (FONT_W + 1) * scale;
	}

	spans_flush (&sp);
}

static void draw_text_centered (SDL_Renderer *r, int cx, int cy, int maxw,
		const char *s)
{
	int scale = 2;

	while (scale > 1 && text_width (s, scale) > maxw) scale--;
	draw_text (r, cx - text_width (s, scale) / 2,
			cy - (FONT_H * scale) / 2, scale, s);
}

/* ------------------------------------------------------------------ */
/* Labels                                                              */
/* ------------------------------------------------------------------ */

/* Action names from joystick.ini are descriptive but too long to fit in
 * a touch target, so shorten the ones we know about. Anything else falls
 * through and is drawn as-is, which keeps user-added bindings working. */
static const struct {
	const char *action;
	const char *label;
} label_table[] = {
	{ "THRUST",			"THR+" },
	{ "RTHRUST",			"THR-" },
	{ "LASER",			"FIRE" },
	{ "MISSILE",			"MSL" },
	{ "BOMB",			"BOMB" },
	{ "ECM",			"ECM" },
	{ "EJECT",			"EJECT" },
	{ "HYPERSPACE",			"HYP" },
	{ "RADAR",			"RADAR" },
	{ "ZOOM_IN",			"Z+" },
	{ "ZOOM_OUT",			"Z-" },
	{ "MAP_CENTER",			"CENTR" },
	{ "MB4_PHOTO",			"PHOTO" },
	{ "PAUSE",			"PAUSE" },
	{ "LOOK_UP",			"UP" },
	{ "LOOK_DOWN",			"DOWN" },
	{ "LOOK_LEFT",			"LEFT" },
	{ "LOOK_RIGHT",			"RIGHT" },
	{ "MOVE_UP",			"UP" },
	{ "MOVE_DOWN",			"DOWN" },
	{ "SPECIAL_TIME_INCREASE",	"TIME+" },
	{ "SPECIAL_TIME_DECREASE",	"TIME-" },
	{ "SPECIAL_SWITCH_MODE",	"MODE" },
	{ "SPECIAL_MOUSE_BTN_LEFT",	"CLICK" },
	{ "SPECIAL_MOUSE_BTN_RIGHT",	"RCLK" },
	{ "SPECIAL_MOUSE_BTN_MIDDLE",	"MCLK" },
};

static const char *short_label (const char *action)
{
	unsigned int i;

	if (!action) return NULL;

	for (i = 0; i < SDL_arraysize (label_table); i++)
		if (!SDL_strcmp (label_table[i].action, action))
			return label_table[i].label;

	return action;
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

/* Slot positions, as a fraction of window width and of the control band
 * height. Only slots whose button is bound in the current mode appear. */
static const struct {
	int idx;
	float fx, fy;
	float size;	/* relative to the base radius */
} slot_table[] = {
	/* shoulder row */
	{ 4,  0.07f, 0.15f, 0.78f },
	{ 6,  0.21f, 0.15f, 0.72f },
	{ 7,  0.79f, 0.15f, 0.72f },
	{ 5,  0.93f, 0.15f, 0.78f },

	/* right-hand face cluster, arranged as a diamond */
	{ 3,  0.84f, 0.42f, 1.00f },
	{ 2,  0.94f, 0.64f, 1.00f },
	{ 1,  0.84f, 0.86f, 1.00f },
	{ 0,  0.74f, 0.64f, 1.00f },

	/* secondary column between the clusters */
	{ 8,  0.62f, 0.40f, 0.72f },
	{ 10, 0.62f, 0.66f, 0.72f },
	{ 11, 0.62f, 0.92f, 0.72f },

	/* d-pad, only placed in mouse mode - the flight modes put the
	 * virtual stick here instead */
	{ 12, 0.16f, 0.40f, 0.68f },
	{ 13, 0.16f, 0.88f, 0.68f },
	{ 14, 0.06f, 0.64f, 0.68f },
	{ 15, 0.26f, 0.64f, 0.68f },

	/* mode switch, kept away from both clusters */
	{ 9,  0.42f, 0.90f, 0.72f },
};

void Touch_Layout (int window_w, int window_h, const TOUCH_VIEWPORT *view)
{
	int base_r, band_y, band_h, margin_below, mouse_mode;
	unsigned int i;

	win_w = window_w > 0 ? window_w : 1;
	win_h = window_h > 0 ? window_h : 1;
	if (view) viewport = *view;

	mouse_mode = in_mouse_mode ();
	layout_mouse_mode = mouse_mode;

	base_r = (int)(SDL_min (win_w, win_h) * 0.058f);
	if (base_r < 20) base_r = 20;
	if (base_r > 68) base_r = 68;

	/* Prefer to sit in the letterbox margin below the game image rather
	 * than on top of it. On a foldable this is the whole point: folded,
	 * the screen is long and thin and the margin is too small, so the
	 * controls overlay the image; unfolded, the screen is nearly square,
	 * the 1.6-aspect image leaves a deep band underneath, and the
	 * controls move down into it so nothing covers the cockpit. */
	margin_below = win_h - (viewport.y + viewport.h);
	if (margin_below >= base_r * 4) {
		band_y = viewport.y + viewport.h;
		band_h = margin_below;
	} else {
		band_h = (int)(win_h * 0.42f);
		if (band_h < base_r * 4) band_h = base_r * 4;
		band_y = win_h - band_h;
	}

	for (i = 0; i < MAX_BUTTONS; i++) {
		buttons[i].idx = (int)i;
		buttons[i].shown = 0;
		buttons[i].held = 0;
		buttons[i].label = NULL;
	}

	for (i = 0; i < SDL_arraysize (slot_table); i++) {
		int idx = slot_table[i].idx;
		const char *action;

		/* In the flight modes the left of the band belongs to the
		 * stick, so the d-pad slots are not placed. */
		if (!mouse_mode && idx >= 12 && idx <= 15) continue;

		action = joystick_action_for_button (idx);
		if (!action) continue;

		buttons[idx].shown = 1;
		buttons[idx].label = short_label (action);
		buttons[idx].cx = (int)(win_w * slot_table[i].fx);
		buttons[idx].cy = band_y + (int)(band_h * slot_table[i].fy);
		buttons[idx].r = (int)(base_r * slot_table[i].size);
	}

	/* Virtual stick occupies the left of the control band in flight
	 * modes. Touching anywhere in the zone anchors the stick there
	 * rather than snapping to a fixed centre, so it does not matter
	 * exactly where your thumb lands. */
	/* Clamped: on a large unfolded screen an unclamped fraction gives a
	 * stick wider than a thumb can comfortably reach. */
	stick_r = (int)(SDL_min (win_w, win_h) * STICK_RADIUS_FRAC);
	if (stick_r < 56) stick_r = 56;
	if (stick_r > 150) stick_r = 150;
	stick_cx = (int)(win_w * 0.16f);
	stick_cy = band_y + band_h / 2;
	stick_zone.x = 0;
	stick_zone.y = band_y;
	stick_zone.w = win_w / 2;
	stick_zone.h = win_h - band_y;

	toggle_rect.w = base_r * 2;
	toggle_rect.h = base_r;
	toggle_rect.x = win_w - toggle_rect.w - base_r / 2;
	toggle_rect.y = base_r / 2;
}

/* ------------------------------------------------------------------ */
/* Event handling                                                      */
/* ------------------------------------------------------------------ */

static FINGER *find_finger (SDL_FingerID id)
{
	int i;

	for (i = 0; i < MAX_FINGERS; i++)
		if (fingers[i].in_use && fingers[i].id == id)
			return &fingers[i];
	return NULL;
}

static FINGER *alloc_finger (SDL_FingerID id)
{
	int i;

	for (i = 0; i < MAX_FINGERS; i++) {
		if (fingers[i].in_use) continue;
		SDL_memset (&fingers[i], 0, sizeof (fingers[i]));
		fingers[i].in_use = 1;
		fingers[i].id = id;
		return &fingers[i];
	}
	return NULL;
}

static int hit_button (float x, float y)
{
	int i;

	if (!overlay_visible) return -1;

	for (i = 0; i < MAX_BUTTONS; i++) {
		float dx, dy, r;

		if (!buttons[i].shown) continue;

		dx = x - buttons[i].cx;
		dy = y - buttons[i].cy;
		/* Touch targets are forgiving: the hit radius is larger than
		 * the drawn one, because a fingertip is much bigger than the
		 * point the digitiser reports. */
		r = buttons[i].r * 1.25f;
		if (dx * dx + dy * dy <= r * r) return i;
	}
	return -1;
}

static int point_in_rect (float x, float y, const SDL_Rect *r)
{
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

/* Move the game's cursor by a window-pixel delta. input.abs_* is in
 * viewport pixels, which is the same scale, so no conversion is needed -
 * only clamping to the image. */
static void move_cursor (float dx, float dy)
{
	int nx = input.abs_x + (int)(dx * TRACKPAD_ACCEL);
	int ny = input.abs_y + (int)(dy * TRACKPAD_ACCEL);

	if (nx < 0) nx = 0;
	if (ny < 0) ny = 0;
	if (nx > screen_w - 1) nx = screen_w - 1;
	if (ny > screen_h - 1) ny = screen_h - 1;

	input.abs_x = nx;
	input.abs_y = ny;
}

static void begin_click (void)
{
	if (click_down) return;
	Input_MousePress (SDL_BUTTON_LEFT);
	click_down = 1;
	click_release_ms = SDL_GetTicks () + CLICK_HOLD_MS;
}

static void update_stick (const FINGER *f)
{
	float dx = f->x - f->anchor_x;
	float dy = f->y - f->anchor_y;
	float len = SDL_sqrtf (dx * dx + dy * dy);
	float dead = stick_r * STICK_DEADZONE_FRAC;

	if (len <= dead) {
		dx = dy = 0.0f;
	} else {
		/* Rescale so the axis reaches full deflection at the edge of
		 * the ring rather than jumping as soon as the deadzone ends. */
		float scale = (len - dead) / (stick_r - dead);

		if (scale > 1.0f) scale = 1.0f;
		dx = dx / len * scale;
		dy = dy / len * scale;
	}

	stick_dx = dx;
	stick_dy = dy;

	/* joystick_motion expects raw SDL joystick axis values. */
	joystick_motion (0, (int)(dx * 32767.0f));
	joystick_motion (1, (int)(dy * 32767.0f));
}

static void release_finger (FINGER *f)
{
	switch (f->role) {
	case ROLE_BUTTON:
		Keymap_JoystickUpDown ((unsigned int)f->button, 0);
		break;

	case ROLE_STICK:
		joystick_motion (0, 0);
		joystick_motion (1, 0);
		stick_active = 0;
		stick_dx = stick_dy = 0.0f;
		break;

	case ROLE_TRACKPAD:
		/* A quick touch that barely moved is a tap: click where the
		 * cursor now is. A drag just leaves the cursor where it was
		 * put, and a hold has already been auto-repeating. */
		if (!f->repeating &&
		    SDL_GetTicks () - f->down_ms <= TAP_MAX_MS &&
		    f->travel <= TAP_MAX_TRAVEL)
			begin_click ();
		break;

	case ROLE_OVERLAY_TOGGLE:
		Touch_SetOverlayVisible (!overlay_visible);
		break;

	default:
		break;
	}

	if (f->role == ROLE_BUTTON && f->button >= 0 && f->button < MAX_BUTTONS)
		buttons[f->button].held = 0;

	f->in_use = 0;
	f->role = ROLE_NONE;
}

int Touch_HandleEvent (const SDL_Event *event)
{
	FINGER *f;
	float x, y;

	if (!enabled) return 0;

	switch (event->type) {
	case SDL_FINGERDOWN:
		x = event->tfinger.x * win_w;
		y = event->tfinger.y * win_h;

		f = alloc_finger (event->tfinger.fingerId);
		if (!f) return 1;

		f->x = f->anchor_x = x;
		f->y = f->anchor_y = y;
		f->down_ms = SDL_GetTicks ();

		if (point_in_rect (x, y, &toggle_rect)) {
			f->role = ROLE_OVERLAY_TOGGLE;
			return 1;
		}

		f->button = hit_button (x, y);
		if (f->button >= 0) {
			f->role = ROLE_BUTTON;
			buttons[f->button].held = 1;
			Keymap_JoystickUpDown ((unsigned int)f->button, 1);
			return 1;
		}

		if (!in_mouse_mode () && !stick_active &&
		    point_in_rect (x, y, &stick_zone)) {
			f->role = ROLE_STICK;
			stick_active = 1;
			return 1;
		}

		if (in_mouse_mode ()) {
			f->role = ROLE_TRACKPAD;
			return 1;
		}

		f->role = ROLE_NONE;
		return 1;

	case SDL_FINGERMOTION:
		f = find_finger (event->tfinger.fingerId);
		if (!f) return 0;

		x = event->tfinger.x * win_w;
		y = event->tfinger.y * win_h;

		if (f->role == ROLE_TRACKPAD) {
			move_cursor (x - f->x, y - f->y);
			f->travel += SDL_fabsf (x - f->x) + SDL_fabsf (y - f->y);
			/* Moving cancels a pending auto-repeat, so dragging
			 * across the cockpit does not fire clicks. */
			if (f->travel > TAP_MAX_TRAVEL && !f->repeating)
				f->down_ms = SDL_GetTicks ();
		}

		f->x = x;
		f->y = y;

		if (f->role == ROLE_STICK) update_stick (f);
		return 1;

	case SDL_FINGERUP:
		f = find_finger (event->tfinger.fingerId);
		if (!f) return 0;

		f->x = event->tfinger.x * win_w;
		f->y = event->tfinger.y * win_h;
		release_finger (f);
		return 1;

	default:
		return 0;
	}
}

void Touch_Update (void)
{
	Uint32 now;
	int i;

	if (!enabled) return;

	now = SDL_GetTicks ();

	if (click_down && (Sint32)(now - click_release_ms) >= 0) {
		Input_MouseRelease (SDL_BUTTON_LEFT);
		click_down = 0;
	}

	for (i = 0; i < MAX_FINGERS; i++) {
		FINGER *f = &fingers[i];

		if (!f->in_use || f->role != ROLE_TRACKPAD) continue;
		if (f->travel > TAP_MAX_TRAVEL) continue;

		if (!f->repeating) {
			if (now - f->down_ms < HOLD_REPEAT_DELAY_MS) continue;
			f->repeating = 1;
			f->next_repeat_ms = now;
		}

		if ((Sint32)(now - f->next_repeat_ms) >= 0 && !click_down) {
			begin_click ();
			f->next_repeat_ms = now + HOLD_REPEAT_MS;
		}
	}

	/* The control set differs between mouse and flight modes, and the
	 * mode can change from a button press, so rebuild when it flips. */
	if (layout_mouse_mode != in_mouse_mode ())
		Touch_Layout (win_w, win_h, &viewport);
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

void Touch_Render (SDL_Renderer *renderer)
{
	int i;

	if (!enabled) return;

	SDL_SetRenderDrawBlendMode (renderer, SDL_BLENDMODE_BLEND);

	/* Show/hide chip is always drawn, otherwise a hidden overlay could
	 * not be brought back. */
	SDL_SetRenderDrawColor (renderer, 255, 255, 255, overlay_visible ? 40 : 70);
	SDL_RenderFillRect (renderer, &toggle_rect);
	SDL_SetRenderDrawColor (renderer, 255, 255, 255, 150);
	draw_text_centered (renderer, toggle_rect.x + toggle_rect.w / 2,
			toggle_rect.y + toggle_rect.h / 2,
			toggle_rect.w - 8, overlay_visible ? "HIDE" : "SHOW");

	if (!overlay_visible) return;

	for (i = 0; i < MAX_BUTTONS; i++) {
		const TBUTTON *b = &buttons[i];

		if (!b->shown) continue;

		if (b->held) {
			SDL_SetRenderDrawColor (renderer, 120, 200, 255, 110);
			fill_circle (renderer, b->cx, b->cy, b->r);
			SDL_SetRenderDrawColor (renderer, 170, 225, 255, 230);
		} else {
			SDL_SetRenderDrawColor (renderer, 255, 255, 255, 28);
			fill_circle (renderer, b->cx, b->cy, b->r);
			SDL_SetRenderDrawColor (renderer, 255, 255, 255, 95);
		}
		fill_ring (renderer, b->cx, b->cy, b->r, 2);

		if (b->label) {
			SDL_SetRenderDrawColor (renderer, 255, 255, 255,
					b->held ? 255 : 170);
			draw_text_centered (renderer, b->cx, b->cy,
					(int)(b->r * 1.7f), b->label);
		}
	}

	if (!in_mouse_mode ()) {
		int kx, ky;
		const FINGER *sf = NULL;
		int cx = stick_cx, cy = stick_cy;

		for (i = 0; i < MAX_FINGERS; i++) {
			if (fingers[i].in_use && fingers[i].role == ROLE_STICK) {
				sf = &fingers[i];
				break;
			}
		}

		/* While held, the ring follows the anchor so the stick appears
		 * exactly where the thumb landed. */
		if (sf) {
			cx = (int)sf->anchor_x;
			cy = (int)sf->anchor_y;
		}

		kx = cx + (int)(stick_dx * stick_r);
		ky = cy + (int)(stick_dy * stick_r);

		SDL_SetRenderDrawColor (renderer, 255, 255, 255, sf ? 70 : 40);
		fill_ring (renderer, cx, cy, stick_r, 2);

		SDL_SetRenderDrawColor (renderer, 160, 210, 255, sf ? 130 : 55);
		fill_circle (renderer, kx, ky, stick_r / 3);
	}
}

/* ------------------------------------------------------------------ */

void Touch_Init (void)
{
	/* SDL turns touches into synthetic mouse events by default. We
	 * handle SDL_FINGER* ourselves, so leaving that on would double up
	 * every gesture as a phantom mouse click. */
	SDL_SetHint (SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

	SDL_memset (fingers, 0, sizeof (fingers));
	enabled = Platform_IsTouchDevice ();
}

void Touch_UnInit (void)
{
	if (click_down) {
		Input_MouseRelease (SDL_BUTTON_LEFT);
		click_down = 0;
	}
}

void Touch_SetOverlayVisible (int visible)
{
	overlay_visible = visible ? 1 : 0;
}

int Touch_OverlayVisible (void)
{
	return overlay_visible;
}

void Touch_SetEnabled (int on)
{
	int i;

	if (enabled && !on) {
		for (i = 0; i < MAX_FINGERS; i++)
			if (fingers[i].in_use) release_finger (&fingers[i]);
	}
	enabled = on ? 1 : 0;
}

int Touch_Enabled (void)
{
	return enabled;
}
