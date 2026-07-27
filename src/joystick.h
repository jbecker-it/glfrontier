#ifndef JOYSTICK_H
#define JOYSTICK_H

extern int currentTimeMode;

void Keymap_JoystickUpDown(unsigned int button, int pressed);
void joystick_motion(unsigned int axis, int value);
void joystick_read_config(const char *path);
const char *mode_name(void);
int in_mouse_mode(void);

/* Name of the action bound to a button in the current mode, or NULL if
 * the button is unbound. The touch overlay uses this to decide which
 * on-screen buttons to draw and what to call them. */
const char *joystick_action_for_button(unsigned int button);

#endif
