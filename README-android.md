# Android port

Frontier: Elite II on Android, with touch controls.

The 68k core, the game logic and the renderer are unchanged in substance -
what changed is everything around them: SDL 1.2 became SDL2, the software
renderer now presents through a GPU texture, file access goes through a
platform layer, and there is a touch control layer that drives the same
abstract input path a gamepad uses.

## What you need to supply

`fe2.s`, a disassembly of the original Frontier: Elite II Atari ST binary.
It is not in this repository and cannot be, because it is a derived work
of copyrighted code. See `README`.

This matters more on Android than elsewhere. The build statically
recompiles the 68k binary into the native library, so the resulting `.so`
is itself a derived work of the original game - unlike an emulator, where
the game data stays a separate file the user provides at runtime. **A
built APK is for your own device only.** It is not redistributable, on a
store or otherwise.

## Building

### 1. Recompile the 68k binary

On your build machine, with `fe2.s` in the repository root:

```sh
make -f Makefile-C fe2.s.c
```

This runs `as68k` and writes `fe2.s.c`, `fixups.h`, and several thousand
files under `gen/`. It only has to be done once, and the output is
architecture independent - the same generated C compiles for x86-64 and
for both Android ABIs.

### 2. Provide SDL2

Unpack or symlink an SDL2 release (2.24 or newer) at `android/app/jni/SDL`:

```sh
ln -s /path/to/SDL2-2.30.9 android/app/jni/SDL
```

### 3. Build the APK

```sh
cd android
./gradlew assembleDebug
```

The first build compiles the whole recompiled core and takes a while.
Trim `abiFilters` in `app/build.gradle` to a single ABI to halve it.

Sound effects and `joystick.ini` are copied into the APK assets
automatically by the `stageAssets` task. Savegames are written to app
private storage and stay ST-compatible.

## Touch controls

The touch layer does not talk to the game. It calls the same three
functions in `src/joystick.c` that a physical gamepad does, so **the
on-screen controls are configured by `joystick.ini`** - bind an action to
a button number there and a labelled on-screen button for it appears.

There are three modes, cycled with the `MODE` button, exactly as on the
handheld builds this port inherits from:

### Mouse mode - the cockpit interface

Frontier's UI is icon clicking at 320x200. The icons are about 16x16
native, which is roughly 3mm on a phone, so tapping them directly is
hopeless. Instead the screen works as a **trackpad**:

| Gesture | Effect |
| --- | --- |
| Drag anywhere | Moves the on-screen cursor, with acceleration |
| Quick tap | Clicks at the cursor |
| Touch and hold | Auto-repeats clicks |

Touch-and-hold exists because of a specific misery noted in `TODO`:
repairing a hull means clicking the same icon dozens of times. That is
tedious with a mouse and unbearable with a fingertip.

### Adventure and Battle modes - flying

The left half of the control band is a **floating virtual stick**: it
anchors wherever your thumb lands rather than at a fixed point. Moving it
engages Frontier's right-button flight grab automatically, because that is
what `joystick_motion` already did for an analog stick.

The right half holds the action buttons for that mode - thrust and time
controls in Adventure, weapons in Battle.

`HIDE` in the top right corner collapses the overlay for a clean view of
the cockpit; the trackpad and stick keep working.

## Foldables

Unfolding is a window resize, not an activity restart - the manifest
declares `resizeableActivity` and lists every relevant value in
`configChanges`, so the running game survives it. SDL reports the change
as `SDL_WINDOWEVENT_SIZE_CHANGED`, and both the letterbox and the control
layout are recomputed from it.

The layout deliberately behaves differently in the two states. The image
keeps its 1.6 aspect ratio, and vertical slack is pushed to the bottom
rather than being split evenly:

- **Folded** - the screen is long and thin, the image is height-limited,
  there is no spare band, so the controls overlay the image
  semi-transparently.
- **Unfolded** - the screen is nearly square, so a 1.6-aspect image leaves
  a deep band underneath. The controls drop into that band and stop
  covering the cockpit entirely.

## Known limitations

- **No GL renderer.** `src/gl.c` uses immediate mode (19 `glBegin` blocks,
  7 of them with double-precision vertices) and the GLU tessellator. None
  of that exists in OpenGL ES and GLU is not available on Android at all.
  The software renderer is used instead, which is authentic and needs no
  host 3D - the game rasterises into ST video RAM itself and only the
  320x200 result is uploaded.
- **No music.** Ogg playback would need libvorbis cross-compiled, and per
  the README there are no music files anyway.
- **No hardware keyboard mapping beyond the standard layout.** Physical
  keyboards work through the scancode table in `src/keymap.c`.
