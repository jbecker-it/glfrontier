# Tests

Host-side checks for the parts of the Android port that can be verified
without a device. They build against the stub SDL in `stub/`, so they need
no SDL2, no window and no GPU.

```sh
make -C tests run
```

## test_touch

Drives the touch layer (`src/touch.c`) with synthetic finger events and a
fake `joystick.ini` binding table, checking:

- **Foldable geometry** - that a folded 22:9 window leaves no band below
  the game image (so controls must overlay it), and an unfolded near-square
  window leaves a deep one (so they dock into it instead).
- **Tap vs drag** - a short, still touch clicks; a long or moving one does
  not, and moves the cursor with acceleration instead.
- **Press-and-hold auto-repeat** - nothing before the delay, then repeated
  clicks, which is what makes hull repair bearable on a touchscreen.
- **Virtual stick** - centred at rest, saturating at full deflection,
  suppressing movement inside the deadzone, and recentring on release.

## test_ini

Parses the real `joystick.ini` with `src/ini.c`, the in-tree replacement
for libini, and prints every section and key/value pair with its parsed
button number.
