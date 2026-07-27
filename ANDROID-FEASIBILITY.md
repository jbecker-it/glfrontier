# Android port feasibility — glfrontier

Assessment of what it would take to run this tree on Android with touch controls.

**Verdict: feasible, medium effort.** The recommended path is SDL2 + the *software*
renderer + a touch overlay that drives the input abstraction that already exists in
`src/joystick.c`. The GL renderer and the distribution model are the two hard parts,
and neither is on the critical path for a working build.

---

## 1. CPU core — already portable

The `--output-c` backend is architecture-neutral and is the path to use.

* 68k registers are a `union Reg` of `s32`/`u32` (`host.h:20`), not host pointers.
* 68k memory is a flat `s8 m68kram[0x110000]` array (`host.c:15`) addressed by `u32`
  offsets. The generated effective-address code emits only
  `(Regs[n]._s32 + displacement)` forms (`as68k/output_c.c:309-347`) — no pointer-width
  assumptions, so it is 64-bit clean and works on ARM.
* `as68k` itself is a host-side build tool. It builds cleanly here (warnings only; the
  one pointer-to-int cast is in an unused debug function, `as68k/dict.c:50`).

The i386 backend (`Makefile-i386`, `as68k/output_i386.c`, 3442 lines) is a dead end for
ARM. Ignore it entirely.

### Landmine: the endianness selector

`host.h:74-134` picks a byte-swap implementation with:

```c
#ifdef __i386__      /* bswap asm */
#elif LITTLE_ENDIAN  /* portable byte-by-byte swap */
#else                /* no-op — assumes big-endian host */
```

On ARM, `__i386__` is undefined, so correctness depends entirely on `LITTLE_ENDIAN`
being both defined *and* non-zero at that point. It happens to resolve to `1234` on
glibc via transitive includes from `stdlib.h` (verified), which is why the existing
MIPS handheld builds work. Bionic also defines it, but relying on a transitive include
for silent correctness is unacceptable: if it ever falls into the `#else` branch you get
the big-endian no-op path and the game corrupts itself with no diagnostic.

Fix before anything else — include `<endian.h>` explicitly and test
`__BYTE_ORDER == __LITTLE_ENDIAN`.

---

## 2. Renderer — use the software one

**Key structural fact:** the 3D rasterisation happens *inside the 68k code*, which draws
into ST video RAM. `src/soft.c` only converts a 320×200 8-bit indexed framebuffer to a
16-bit surface (`src/soft.c:309-322`) and flips it.

So the software path needs **no host 3D at all** — one 320×200 texture upload per frame.
`WITH_GL=n` is already a supported configuration (`Makefile-C:16-24`, commit `64fdc91`).
This is the fast path to a running build.

### The GL renderer does not port

`src/gl.c` (1883 lines) is desktop-GL-only and cannot run on GLES:

| Feature | Count | GLES status |
|---|---|---|
| `glBegin`/`glEnd` immediate mode | 19 blocks | removed |
| `glVertex*` calls | 51 | removed |
| `glVertex3dv` (double precision) | 7 | no GL_DOUBLE in GLES |
| `gluTess*` (concave tessellation) | 14 | GLU absent on Android |
| `gluPerspective` / `glOrtho` | 2 | removed |

Porting means vendoring a tessellator (libtess2), writing a batching vertex-buffer layer,
and writing shaders — a month-plus of work. And the upstream renderer is incomplete
anyway: the README calls it "very very experimental… most primitives aren't rendered yet",
and `TODO` still lists the planet renderer as missing plus Z-ordering bugs. Defer it.

---

## 3. SDL 1.2 → SDL 2

Modest surface: 12 distinct SDL-1.2-only symbols, ~37 call sites.

| Symbol | Sites | Notes |
|---|---|---|
| `SDL_keysym` | 9 | → `SDL_Keysym` |
| `SDL_EventState` | 8 | signature survives |
| `SDL_WM_GrabInput` | 4 | → `SDL_SetRelativeMouseMode` |
| `SDLK_LAST` | 4 | **no SDL2 equivalent** |
| `SDL_SetVideoMode` / `SDL_GetVideoInfo` | 4 | → window + renderer |
| `SDL_WM_SetCaption` | 2 | → `SDL_SetWindowTitle` |
| `SDL_Flip`, `SDL_GL_SwapBuffers`, `SDL_WarpMouse`, `SDL_HWSURFACE`, `SDL_ANYFORMAT` | 1 each | direct equivalents |

`SDLK_LAST` is the awkward one: `unsigned char key_states[SDLK_LAST]` (`src/input.h:11`)
must become scancode-indexed at `SDL_NUM_SCANCODES`, and `src/keymap.c` (702 lines, 238
`SDLK_` references) needs a remap table. Mechanical but tedious.

The audio layer (`SDL_OpenAudio`, `SDL_LoadWAV`, `src/audio.c`) survives SDL2 unchanged.

SDL2 ships an official Android backend — Java activity, EGL context, touch events — which
is what makes this whole exercise tractable.

### App lifecycle

`Start680x0()` never returns; it is an infinite dispatch loop, and the SDL event pump is
called *from inside* the 68k code via `Call_HostUpdate` (`src/hostcall.c:648`). SDL2's
`android_main` runs `main()` on its own thread, so a blocking loop is fine — but there is
currently no lifecycle handling at all. `SDL_APP_WILLENTERBACKGROUND` / `DIDENTERFOREGROUND`
must be handled in `Main_EventHandler` (`src/main.c:103`) or Android will ANR or kill the
process on backgrounding.

Also `Call_Idle` is `SDL_Delay(0)` (`src/hostcall.c:643`) — a busy-wait. On a phone that
is a battery and thermal problem; it needs a real sleep.

---

## 4. Touch controls — the strong point of this tree

Every commit here is by Paul Cercueil, who ported this to OpenDingux handhelds. The
result is a **complete abstract input layer already in place** (`src/joystick.c`, 287
lines) that a touch overlay can drive directly, without touching the game or the 68k core:

* **Three input modes** toggled by one button — Adventure / Battle / Mouse
  (`src/joystick.c:12`), each with its own action→ST-scancode map loaded at runtime from
  `joystick.ini`. Mouse mode is the default (commit `599ab53`).
* **`Keymap_JoystickUpDown(button, pressed)`** (`src/joystick.c:186`) — maps an abstract
  button index to an ST scancode and injects it into the same ring buffer the real
  keyboard feeds.
* **`joystick_motion(axis, value)`** (`src/joystick.c:250`) — in Mouse mode drives the
  absolute cursor; in flight modes it *auto-holds the right mouse button* and feeds
  relative deltas. That is exactly how FE2 flight control works (`src/input.c:85` — right
  button held = grab = pitch/yaw).
* **`inject_mouse_event(x, y, pressed)`** (`src/joystick.c:170`) — synthesises a click at
  an arbitrary screen coordinate; already used to hit the cockpit time-acceleration
  buttons (`src/joystick.c:236`).
* An on-screen mode indicator is already drawn by the software renderer (`src/soft.c:345`).

A touch layer therefore only has to render an overlay and call these three functions.

### Proposed touch scheme

**Flight (Adventure / Battle modes)**
* Left half: virtual analog stick → `joystick_motion(0/1, v)`. This already engages the
  right-button flight grab automatically, so pitch/yaw works with no game-side change.
* Right half: a ring of touch buttons bound through `joystick.ini` to
  `THRUST` / `RTHRUST` / `LASER` / `MISSILE` / `HYPERSPACE`. No code change — it is a
  config file.
* Two-finger vertical drag → `SPECIAL_TIME_INCREASE` / `SPECIAL_TIME_DECREASE`, which
  already exist as actions.

**Cockpit UI (Mouse mode) — the real design problem**

FE2's entire interface is icon-clicking at 320×200. Icons are ~16×16 native, roughly 3 mm
on a phone. Direct 1:1 tap-through will be unusable.

Use **indirect touch** instead: drag anywhere to move the existing on-screen cursor, tap
to click at the cursor position. This reuses `input.abs_x/abs_y` and the cursor sprite
already blitted at `src/soft.c:353`. It is the trackpad model that every ScummVM and
DOSBox Android port converged on, and it is the right answer here too.

Add touch-and-hold auto-repeat in the overlay while you are at it — `TODO` notes
"repairing your hull involves RSI mouse clicking", which is intolerable on a touchscreen.

**Screen geometry:** the game is a hard 320×200 at 1.6 aspect (`src/main.c:222`); phones
are ~20:9, so expect pillarboxing. Mouse coordinates are already scaled by
`screen_w`/`screen_h` (`src/input.c:38`), so this is parameterised — the letterbox
margins are natural real estate for the touch overlay.

---

## 5. Distribution — the actual blocker

`fe2.s`, the disassembly of the original Frontier: Elite II ST binary, is **not in this
repo and is copyrighted** (`README:1-8`, commit `133dbd4`).

The critical difference from shipping an emulator: this project *statically recompiles*
the 68k binary into the executable. The generated C is a derived work of Braben's binary,
baked into the shipped artifact — unlike DOSBox or ScummVM, where the game data stays a
separate, user-supplied file. So:

* An APK containing the built binary cannot go on Google Play or F-Droid.
* "Let the user supply `fe2.s`" does not rescue it either, because the recompilation
  happens at *build* time — you would have to ship a compiler.

Two honest options:

1. **Personal/sideload build only.** Anyone who owns the original builds it themselves.
   Zero extra engineering; no distribution.
2. **Restructure to runtime ROM loading** — load `frontier.prg` at startup and run it on
   an interpreting or JIT 68k core, i.e. go back to the UAE-core approach this fork
   explicitly abandoned. That makes the game a user-supplied asset and is the only clean
   distribution model. The cost is the 8 fps → 94 fps speedup this fork exists for
   (`benchmarks.txt`) — but that benchmark is an Athlon XP 1700+ from 2006. A current
   phone SoC runs a 68000 interpreter at hundreds of times original ST speed, so the
   performance argument that justified static recompilation in 2006 simply does not apply
   on modern ARM. If distribution matters at all, this is the right architecture.

Licensing: GPL v2-or-later (Hatari-derived). v2 is compatible with Google Play's terms;
GPLv3 would not be.

---

## 6. Remaining port work

**Asset paths.** All relative to cwd, which is meaningless on Android:
* `sfx/sfx_%02d.wav` (`src/audio.c:333`)
* `music/%02d.ogg` (`src/audio.c:58`)
* `cursor.png` (`src/soft.c:71`)
* `joystick.ini` (`src/joystick.c`)

Needs asset extraction to `SDL_AndroidGetInternalStoragePath()`. Note `FORCE_WORKING_DIR`
(`src/main.c:29`).

**Save games.** `Call_Fwrite` / `Call_Fread` / `Call_Fopendir` (`src/hostcall.c:741-868`)
call bare `fopen`/`dirent` with game-supplied filenames. Must be redirected to
app-private storage. Worth preserving ST savefile compatibility — the README advertises it.

**Dependencies to cross-compile:**
| Library | Needed for | Note |
|---|---|---|
| SDL2 | everything | official Android support |
| SDL2_image | `cursor.png` only | drop it — inline the 158-byte cursor |
| libvorbis / libogg | `-DOGG_MUSIC` | drop it — README says music doesn't exist yet |
| **libini** | `src/joystick.c:1` | small; vendor it or write a 50-line parser |

**Build system.** `Makefile-C` generates `gen/*.c`, one file per 68k function split by a
6-character name prefix (`as68k/output_c.c:576`) — thousands of translation units. NDK
CMake with a glob works, but expect a long first build. The README warns about hundreds
of MB of RAM with gcc-3.x; clang handles it better, but the giant `switch` dispatch in
`Start680x0()` over thousands of case labels (`as68k/output_c.c:172`) is a known compiler
stress point. `benchmarks.txt` suggests `-O1` on the dispatch part.

**Debugging.** Bounds checking is compiled out by default (`M68K_DEBUG` off,
`host.h:53`), so any 68k address outside `0x110000` walks off the array silently. Enable
it during bring-up. The `SIGSEGV` handler (`src/main.c:284`) is not useful on Android.

---

## 7. Effort estimate

| Phase | Scope | Estimate | Risk |
|---|---|---|---|
| 1 | SDL2 migration + software renderer + ARM/NDK build | 1–2 weeks | Medium — endian macro, `gen/*.c` build volume |
| 2 | Touch overlay on the existing joystick layer | 1–2 weeks | **Low** — the abstraction already exists |
| 3 | GLES 2 renderer | 1–2 months | High, and upstream GL is incomplete anyway |
| 4 | Runtime ROM loading (for distribution) | Substantial | Different architecture |

Phases 1+2 give a genuinely playable Android build in roughly a month. Phase 3 is
optional — the software renderer is what the game shipped with and it is authentic.
Phase 4 is only needed if the build is to be distributed.

## Bottom line

Technically this is one of the more favourable ports of its kind: the CPU core is already
architecture-neutral C, the software renderer needs no host 3D, and — unusually — a
gamepad-style input abstraction with a virtual-cursor mode is *already implemented*,
which is precisely the layer a touch UI needs. The engineering risk is low and
concentrated in the SDL2 migration.

The binding constraint is legal, not technical: static recompilation makes the shipped
binary a derived work of the original game, so a distributable build requires switching
to runtime ROM loading with an interpreted 68k core — which modern ARM performance makes
entirely practical.
