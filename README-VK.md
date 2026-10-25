# Chocolate Doom VK — libretro slang shaders in DOOM, via Vulkan

This fork of [Chocolate Doom](https://www.chocolate-doom.org) runs every
frame of the game through a **libretro slang shader preset** (`.slangp` /
`.slang` — the shader format used by RetroArch and the
[slang-shaders](https://github.com/libretro/slang-shaders) corpus) using
[VTSTech/libslang-vk](https://github.com/VTSTech/libslang-vk), an embeddable
C++23 library with a pure-C binding. CRT scanlines, LCD grids, phosphor
decay, bezels — the same post-processing ecosystem emulators use, applied
to the original 1993 engine.

```
I_VideoBuffer (320x200, PLAYPAL indices, CPU-rasterized by the engine)
    |  palette LUT expand (+ 6:5 nearest stretch when 4:3-corrected)
    v
RGBA8 320x240 / 320x200  --slangvk_image_update-->  Vulkan input image
    |  slangvk_chain_render_swapchain  (preset pass chain; the library
    |  handles history rings, feedback, per-pass FBOs, letterboxing)
    v
vkQueuePresentKHR  (swapchain; render() is synchronous/fenced, so the
                     present needs no wait semaphore)
```

The integration lives in `src/i_video_vk.c` (~1,000 lines of C99) plus a
set of small, reviewable hooks in `src/i_video.c`, `src/m_config.c`, and
the build files. The classic SDL2 renderer is untouched: with the feature
off (or on any failure), Chocolate Doom behaves exactly like upstream.

## Requirements

- A CMake ≥ 3.7 build (as upstream) and a C++23-capable compiler for the
  embedded library (GCC ≥ 12, Clang ≥ 15, MSVC 2022 17.13+).
- **No Vulkan SDK** anywhere. libslang-vk loads the Vulkan runtime at
  runtime through volk, and the presenter in this fork resolves its own
  direct entry points through `SDL_Vulkan_GetVkGetInstanceProcAddr`. You
  just need a Vulkan-capable driver (any modern GPU, or lavapipe).
- SDL2 ≥ 2.0.14 with the `SDL_WINDOW_VULKAN` path (any 2.24+ is fine;
  2.32's window-aware Vulkan API is used when present).

At build time CMake fetches a pinned copy of libslang-vk via FetchContent —
not by release tag but by immutable commit hash: the exact commit the
current `M4.07` milestone points at (`d176d1b` — the SPIR-V cache hooks,
the allocation-free render path and the C API hardening), named in
`CMakeLists.txt`. A tag can be re-pointed upstream; a commit hash cannot.
That in turn fetches glslang 16.6, Vulkan-Headers and volk.
`ENABLE_VK_SHADERS=OFF` builds the stub path instead — no fetches,
no C++, upstream-identical behavior.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build          # chocolate-doom, -heretic, -hexen, -strife
```

All four games share the same video path, so the feature covers all of
them (plus Strife). On Windows, MSYS2/MinGW and MSVC (vcpkg manifest) are
wired up in `.github/workflows/vk-shaders.yml`; the Linux job
smoke-tests on lavapipe and ships a **user-facing x64 tarball** —
the games plus `libslang-vk.so` and the bundled runtime libraries,
relocatable via the executables' `$ORIGIN` rpath. It runs on any
distro new enough for the runner's glibc (the archive's README.txt
names the exact floor); lavapipe is only CI's test driver — on a
player's machine the binaries use whatever Vulkan ICD is installed,
or none at all (classic renderer).

The library is linked as a **shared** artifact: `libslang-vk.dll` on
Windows — identically named on MSVC and MinGW (the target pins an
empty prefix; MinGW's toolchain default would double it into
`liblibslang-vk.dll`) — and `libslang-vk.so` / `libslang-vk.dylib`
elsewhere. A single `slangvk-runtime` copy step puts it next to the
game executables at build time — one command for every game, not one
per game, which raced on the same destination under parallel MSVC
links — so the build tree runs out of the box, the executables stay
small, and the shader library can be swapped or updated without
relinking all four games. On Windows the
exports come from `cmake/libslang-vk-c-api.def` (the upstream C surface
carries no annotation, so the DLL would otherwise export nothing); keep
that file in sync when bumping the pin — a missing name is a link error,
not a silent miss. To go back to the single static link into each game,
set `SLANGVK_BUILD_SHARED OFF` in the root `CMakeLists.txt` and link
`slangvk::slangvk` instead of `slangvk::shared` in `src/CMakeLists.txt`.

## Running

```sh
# One-off:
build/src/chocolate-doom -iwad DOOM.WAD -shader /path/to/crt-easymode.slangp

# Persisted in the config file
# (~/.local/share/chocolate-doom/chocolate-doom.cfg — note the
# "name  value" format, no '=' sign, like the rest of Chocolate Doom):
#   shader                     "/path/to/crt-easymode.slangp"
#   usevulkan                  1
#   shader_base_resolution     640        # 2x base canvas (see below)
```

Setting `shader` (config or `-shader <path>`) is what activates the
Vulkan path. This fork ships **no presets by design** — clone
[libretro/slang-shaders](https://github.com/libretro/slang-shaders) and
point `shader` at any `.slangp` in it; `-shader` accepts any path.
Presets can also be browsed in-game — see "Shader browser" below.

| Flag | Effect |
|------|--------|
| `-shader <path>` | Run this `.slangp` through the chain (persist in `shader` config key) |
| `-novulkan` | Force the classic SDL2 renderer for this run (`usevulkan = 0` persists) |
| `-noaspect` | Feed raw 320x200 (16:10) instead of the authentic 4:3 stretch |
| `-vkbase <n>` | Shader base resolution: 2/3/4 = doubled/tripled/quadrupled chain input (or a pixel width, e.g. `640`); persists in `shader_base_resolution` |
| **Alt+A** (in-game) | Cycle aspect 4:3 → 16:10 → 16:9 live; feed re-creates, chain re-derives (vanilla F11 gamma stays put) |
| **Scroll Lock** (in-game) | Toggle the stats overlay (below) |
| **PrintScreen** (in-game) | Screenshot: read back the next presented frame to the first free `vkshotNNN.tga` (same channel-correct TGA writer as `-vkcapture`) |
| **Alt+B** (in-game) | Cycle the shader base resolution 1x → 2x → 3x → 4x → 1x (persists in `shader_base_resolution`; vanilla F12 spy stays put) |
| `-novkstats` | Start with the stats overlay off (`vkstats = 0` persists) |
| `-vkdebug` | Enable the Vulkan validation layer for this run |
| `-vkcapture <n>` | Dump the n-th presented frame to `vkframe.tga` |

## Shader browser (in-game menu)

The **SHADERS** row in the Options menu opens a preset browser in all
four games. It walks **one folder at a time**: the browser opens in the
active preset's own folder (or the drop-in `shaders/` folder beside the
executable / inside the config dir on a first run), lists that folder's
subfolders (rendered `NAME/`) and its `.slangp` presets, and offers a
`..` row whenever there is a parent folder to climb back to — so a
preset pack laid out like the slang-shaders repo (`crt/`, `handheld/`,
...) is browsable entirely in-game. Drop new presets into any folder
and they appear the next time the browser opens, no restart needed.
The list pages with the arrow keys (up/down one row, left/right a
windowful), the active preset is marked with `*`, and Enter applies
the highlighted row — a folder name descends into it, `..` climbs,
a preset applies:

- **Vulkan path live** — the swap is hot. The new preset is parsed and
  a complete new chain built against the live swapchain *before* the
  old chain/preset pair is destroyed (prepare-then-commit), so a
  broken or missing preset can never leave the renderer chainless: the
  worst case is the current shader staying on screen, with the reason
  flashed on the stats overlay and written to `gpu-scaler.log`. The
  device, swapchain and input image are untouched; only the
  chain/preset pair is replaced. The SPIR-V disk cache applies to
  swaps exactly like warm starts — revisiting a preset seen earlier in
  the session is milliseconds.
- **Classic renderer** — the pick persists (the `shader` config key,
  and a stale `usevulkan = 0` is re-enabled, exactly like passing
  `-shader` on the command line) and a message notes that a restart
  applies it.

The choice persists in the `shader` config key either way. There is
deliberately no "shaders off" row — see the changelog for why the
classic fallback path is not presentation-safe.

## On-screen stats overlay

A compact overlay is drawn into the software frame just before it is
presented — on **both** backends — so it answers "what am I actually
looking at" at a glance:

```
Vulkan | libslang-vk
llvmpipe (LLVM 19.1.7, 256 bits)
crt-easymode | 3 passes
314.2 fps | 3.18 ms | 320x240 4:3
```

- Row 1 is the presenter: `Vulkan | libslang-vk` or `SDL2 | classic`.
- Row 2 is the adapter (Vulkan device name) or, on the classic path,
  the SDL renderer, video driver and window size
  (e.g. `opengl | x11 | 640x480`).
- Row 3 is the active preset — its file name without path or extension —
  with its pass count; a configured preset that failed to start is shown
  as `(unused)`, `no shader` when none is set.
- Row 4 is the smoothed frame rate and frame time, plus the feed mode
  (`320x240 4:3`, `320x200 16:10` or `356x200 16:9`, the state Alt+A
  cycles; with a base multiplier active it shows the enlarged canvas,
  e.g. `640x480 4:3 (2x)`).
  Under normal play the engine caps rendering at its 35 Hz tic rate — that
  cap is what the counter honestly reports; `-timedemo` shows real
  throughput (several hundred fps on lavapipe, for example).

The text is an embedded public-domain 8×8 bitmap font (font8x8), drawn
in palette-index space with `I_GetPaletteIndex()` colors, so it follows
gamma and pain tints like the rest of the frame — and on the Vulkan
path it goes through the shader chain itself (`-vkcapture` frames show
it). **Scroll Lock** toggles it at runtime; `vkstats` in the config
file and `-novkstats` control the startup state (default: on). Note
that Scroll Lock is intercepted by the overlay toggle and therefore
not available for game bindings while this fork runs.

The hotkeys report back through the overlay itself: **PrintScreen**,
**Alt+B** and **Alt+A** each add a short-lived amber feedback row to the
box for two seconds — `shot vkshot000.tga` / `base 640x480 (2x)` /
`aspect 16:9` — so the action is confirmed without leaving the game.
While the Vulkan path is live, **Alt+A** is the aspect toggle and
**Alt+B** is the base-resolution cycle — the same Alt-combo convention
as Alt+Enter fullscreen. The letter is intercepted only with Alt held
and only while the Vulkan path is active, so the vanilla F-keys keep
their jobs: F11 remains the gamma/brightness cycle (Doom, Heretic,
Hexen) and F12 the spy camera (Doom, Heretic; dev-map reload in Hexen).
PrintScreen is unused by the vanilla game; with `-novulkan` the
Alt-combos fall through to the game untouched as well.

## The frame feed and aspect handling

DOOM's software renderer produces 320×200 indexed 8-bit frames — VGA mode
13h with non-square pixels: 4:3 on the CRT, 16:10 in square-pixel terms.
The presenter LUT-expands those indices to RGBA8 through the current
PLAYPAL (gamma and the 6-bit VGA quantization included — the exact same
values the classic path uses) and stretches the frame according to the
tri-state `aspect_ratio_correct`:

| Value | Mode | Feed | How |
|-------|------|------|-----|
| `1` (default) | 4:3 | 320×240 | 6:5 nearest row stretch (authentic mode 13h geometry) |
| `0` | 16:10 | 320×200 | raw square pixels, no stretch |
| `2` | 16:9 | 356×200 | 320→356 nearest column stretch on the raw feed |

libslang-vk's aspect-corrected viewport derives its letterbox from the
feed shape (SPEC F6), so a 16:9 feed fills a 16:9 display edge to edge
(and letterboxes correctly on anything else). **Alt+A cycles the three
modes live** — 4:3 → 16:10 → 16:9 → back — and the choice persists
through the config file like every other setting. The value `2` is a
Vulkan-path-only mode: if the run falls back to the classic SDL2
renderer, any nonzero value renders as corrected 4:3 there.

The 16:9 mode is a *stretch*, not a wider view: the full 320×200 frame
is widened about 11% relative to square pixels (how the console ports
fill widescreen sets). True widescreen — more rendered columns, wider
FOV — would mean engine-side renderer changes; see Known limitations.

## Field of view (FOV)

The Options menu of each game has a FOV slider (64-128 degrees,
default 90 = vanilla). The setting lives entirely in the software
renderer's projection math (`R_InitTextureMapping`), so it behaves
identically on the classic renderer and the Vulkan shader path - the
frame feed is just scaled afterwards. At 90 degrees the code paths
are bit-identical to vanilla; values outside 64-128 found in a
config file are reset to 90 on the next view-size rebuild. The
non-default projection is computed directly from the half-angle
tangent (the previous build derived it from the vanilla finetangent
table, whose fine-angle grid truncated it by up to about 0.2
degrees).

Notes:

- In doom and strife, whose options menus are built from lump
  graphics, the FOV label and value are the small font scaled up to
  the height of the other menu rows (the stock font uses the same
  red ramp, so the row blends in). Heretic and hexen draw it with
  the big menu font like every other row.
- Horizontal and vertical field scale together (a plain zoom out or
  in); walls and sprites stay on a consistent projection, and the
  weapon sprite scales with the same factor while staying anchored
  to the bottom of the view.
- The 16:9 / 16:10 aspect modes (Alt+A) stretch the finished frame;
  they do not add field of view. The slider measures the FOV of the
  internal 320-wide 4:3 frame.
- Saved as `fov` in each game's config file; purely visual, so
  demos and network games are unaffected.

## Shader base resolution (double / triple / quadruple)

DevilutionX's libslang-vk integration exposes two independent knobs: the
**Game Resolution** (the window/swapchain the chain presents to) and the
**Shader Base Resolution** (the canvas the chain consumes). This fork has
the same split — the window side is Chocolate Doom's regular `-2`/`-3`/
`-width`/`-height`/fullscreen handling, and the base side is new:

```sh
# 2x  base canvas (feed 320x240 -> chain input 640x480):
build/src/chocolate-doom -iwad DOOM.WAD -shader crt/crtglow_lanczos.slangp -vkbase 2

# 3x / 4x (960x720 / 1280x960 chain input), or a pixel width — same thing:
... -vkbase 3
... -vkbase 1280

# Config-file form (persisted like every other VK option):
#   shader_base_resolution     960
```

**Alt+B cycles the multiplier live** while the game runs — 1x → 2x → 3x
→ 4x → back to 1x — riding the same re-create-and-re-derive path as the
Alt+A aspect toggle: the input image is rebuilt at the new size, the chain
re-derives every pass FBO from the per-frame input, the overlay's mode
row updates immediately, and the choice persists through
`shader_base_resolution` (multiplier form) on exit.

What it does: after the palette LUT expansion (and the aspect stretch
when enabled), the frame is duplicated k-fold — each game pixel becomes
an exact k×k block, pure nearest-neighbor, no new detail — and *that*
image is what the preset chain sees. The chain re-derives every pass FBO from the input
size, so a `2xScaleHQ` fed a 640×480 base really does its 2x work at 2x
game scale, and the final pass maps the result onto the window as before.

Why you'd want it:

- **Upscaler headroom on small windows.** Presets designed around
  input < output (ScaleFX, HQx, xBR variants) have more room to work
  with a bigger base — the same reason DevilutionX ships a base-size
  setting. On a huge window it matters less (the ratio is already large).
- **Anti-aliasing / sharpening presets** (FXAA, advanced AA) behave
  better with more pixels to smooth: at native 320×240 they can blur the
  whole frame, at 2–3x they work at a finer granularity.
- **CRT masks / scanline presets** that compute in input space get a
  finer pixel grid; output-space presets are unaffected.

Trade-offs: it is a real upscale — heavier input-space presets cost more
GPU time per frame (input-space work scales with k²; on the lavapipe
software rasterizer a 1-pass preset went ~294 → ~211 fps at 2x, ~157 at
3x — real GPUs have far more headroom), and the CPU staging copy grows
(k×k duplication, one `memcpy` per row block — measured in microseconds).
The multiplier is capped at 4 (1280×960) to bound staging memory and
shader cost. `0` (the default) and `320` mean native 1x — the exact
pre-v0.2 look. Vulkan path only; `-novulkan` runs ignore it. The startup
log prints the effective choice:

```
I_VK: shader base resolution 2x — feed 320x240 -> base 640x480 (DevilutionX-style pre-scale before the preset chain)
I_VK: feed 320x240 (4:3), base 640x480, swapchain 1920x1080, present FIFO
```

The Alt+A aspect toggle composes with it (base follows the feed: 640×480 ↔
640×400 ↔ 712×400), and the overlay's mode row always shows the live
canvas.

## Shader cache (SPIR-V disk cache)

Compiling a preset is the expensive part of startup — glslang runs on
every pass, and royale-class presets take seconds. Since libslang-vk
M4.07 the library provides a *content key* per pass (a digest of the
exact GLSL handed to glslang plus the compiler's version and target)
and calls back into the host for the compiled bytes; this fork stores
them in `<configdir>/shader-cache/` and serves them back on later runs.
The startup log says which way each pass went:

```
# first run with a preset (compiles, stores):
I_VK: SPIR-V cache /home/user/.local/share/chocolate-doom/shader-cache/ — 0 passes from cache, 3 compiled
# every later run (bytes served from disk):
I_VK: SPIR-V cache /home/user/.local/share/chocolate-doom/shader-cache/ — 3 passes from cache, 0 compiled
```

Invalidation is entirely content-driven: the key covers the preset, its
`#include` graph and the library's own translation semantics, so editing
a shader or bumping the libslang-vk pin simply produces new keys — there
is no version number to bump and nothing can go stale. Deleting the
directory is always safe (the next run just recompiles). Every store is
write-to-temp + rename and every load is validated (magic, sizes, exact
file length), so a corrupt or half-written entry is simply a miss — the
cache can never break a preset load. No configuration: it is on whenever
the config directory is writable.

## Debug aids

`-vkcapture <n>` reads back the n-th presented frame straight off the
swapchain image (created with `TRANSFER_SRC` when the surface allows it)
and writes `vkframe.tga` — a deterministic, tooling-free proof of the full
present pipeline. **PrintScreen** does the same readback on demand at
any moment, writing the first free `vkshot000.tga`, `vkshot001.tga`, …
(probed with `fopen`, so repeated presses never overwrite).

The readback is a raw `vkCmdCopyImageToBuffer` — the bytes arrive in the
surface format's native packing, and the writer maps that to TGA's
B,G,R,A order per the actual `VkFormat` of the swapchain (`B8G8R8A8*`
is already TGA order and goes out untouched; `R8G8B8A8*` gets R and B
swapped on the way out). Versions before v0.2.1 swapped unconditionally,
which red/blue-flipped every capture made on a B8G8R8A8 surface — i.e.
virtually every real platform — producing the "blue shifted"
screenshots. `-vkdebug` enables the Khronos validation layer; a clean
run produces no output.

## Session log (gpu-scaler.log)

Windows GUI-subsystem builds show no console — `printf` and stderr go
nowhere exactly where the Vulkan path most needs to explain itself. So
the presenter writes a session log: **`gpu-scaler.log`**, one plain-text
file per run, timestamped line by line and flushed as it happens (a
crash cannot eat the tail). It lives in the config directory — next to
the executable (and `default.cfg`, and the `shader-cache/` directory) on
Windows, `~/.local/share/chocolate-doom/` elsewhere — and each start
pushes the previous run to `gpu-scaler.log.old`, so the last two
sessions, including a crashed one, are always on disk. All four games
rotate the same pair of files when run from the same directory.

What lands there:

- the session header (package version, log path) and the startup
  snapshot — preset path, `aspect_ratio_correct`,
  `shader_base_resolution`, `vk_debug` — written before anything can
  fail;
- every message the Vulkan path writes as it works (init steps, the
  `libslang-vk active — …` banner, cache hit/compile counts, aspect and
  base-resolution changes, screenshot confirmations, device-lost
  recovery);
- every **fallback reason** (one guaranteed line per failed init —
  "no shader preset configured", "SDL2 has no Vulkan support", "preset
  failed to load", …), even when the classic renderer takes over and
  the game itself runs fine;
- the final `I_Error` text when a run dies fatally — device-lost
  recovery failing, for instance.

The terminal is left alone: the file is the log's only destination by
default. A run started from a shell used to end with the `I_VK: …`
stream scattered over the console (unbuffered stderr interleaving with
the startup progress dots); now that stream goes to `gpu-scaler.log`
and the console stays clean. To watch it live, echo it back — set
`vk_log_console 1` in `default.cfg` or pass **`-vklog`** on the command
line, and every line lands on stdout/stderr as it is written. Echo also
switches itself on automatically when the log file cannot be opened:
with no durable home, the console is the only witness. Logging is
best-effort by design and never fails the game. Builds with
`ENABLE_VK_SHADERS=OFF` never create a log.

## Fallback behavior

Every failure path — no Vulkan loader, device creation refused, the preset
file missing or failing to parse — records one explanatory line in
`gpu-scaler.log` and drops back to the classic SDL2 renderer for that
run; the detailed per-step errors from the failed init are in the same
file. (The lines reach the console too when `-vklog` is set or when the
log file itself could not be opened.)

A **device lost** event (driver reset, Windows TDR) once the chain is
live is no longer fatal: the library rebuilds its side (device, the
preset's LUT textures, the chain — libslang-vk M4.06) and the presenter
rebuilds its own (swapchain, views, semaphore, entry points, input
image), drops the frame it was on and carries on with the next; a
`Vulkan device lost — recovered` flash confirms it in the overlay. The
loss is detected wherever it surfaces first — inside the chain render
or on the frame upload — with the same recovery. Only a recovery that
itself fails (broken driver, no replacement device) is fatal, via
`I_Error` with the library's message. The games' own sources stay C99
throughout.

## Validation performed (2026-10-22; v0.1.1-vk, base-resolution at v0.2.0, hotkeys + capture fix at v0.2.1, M4.07 pin + SPIR-V cache + device-lost recovery at v0.3.0, session log + shared library at v0.4.0)

- libslang-vk M4.05 standalone: 18/18 ctest on lavapipe (incl. the M4
  swapchain-presentation test and the C API test).
- M4.07 pin bump (`d176d1b`): the c.h surface is purely additive over
  the previous pin (audited by diff) — SPIR-V cache hooks, PERF-09
  allocation-free render path, SEC-07 C API validation. Both game build
  variants (ENABLE_VK_SHADERS on/off) compile clean under
  `-Wall -Wextra` against the new headers.
- SPIR-V disk cache: 45/45 unit checks (`scripts/test_vk_cache.c`) —
  write/read round trip, overwrite, serial arena reuse, corrupt magic /
  truncated file / lying size header rejected as misses, path-traversal
  and malformed keys refused before the filesystem, zero-blob writes
  refused, and the on-disk layout (magic + LE32 sizes) pinned byte by
  byte.
- Full game smoke on lavapipe (llvmpipe 19.1.7) under Xvfb: freedoom1.wad,
  `demo1` timedemo through the chain — 1-pass preset at ~306 fps, 2-pass
  at ~284 fps, frame 240 captured via `-vkcapture`, differential check
  across two different presets (mean abs diff 22/255, 98% of pixels
  changed — the chain demonstrably processes the frame).
- Shader base resolution (v0.2.0): six-run matrix on lavapipe — 1x
  baseline, `-vkbase 2` / `-vkbase 640` (multiplier and width forms,
  byte-identical output: mean diff 0.28/255), `-window -3 -vkbase 2`
  (960x720 window, real upscaler headroom), `-vkbase 4 -vkdebug` (zero
  validation messages), and a config-seeded `shader_base_resolution 960`
  (3x). Overlay decoded glyph-by-glyph from every capture: the mode row
  reads `320x240 4:3`, `640x480 4:3 (2x)`, `960x720 4:3 (3x)`,
  `1280x960 4:3 (4x)` exactly as configured. Content equivalence at 2x
  and 4x: mean abs diff vs the 1x capture 9.9 / 9.2 out of 255 (final-pass
  sampling phase only); at 3x (a 3:2 output ratio, the worst case for the
  test preset's synthetic scanlines) the scan-factor-corrected per-channel
  ratio median is exactly 1.000 — identical scene structure. Timedemo
  throughput on lavapipe: ~294 / ~211 / ~157 / ~117 fps at 1x / 2x / 3x /
  4x (input-space cost scales with k², as expected).
- Zero Vulkan validation messages across a full `-vkdebug` session.
- Hotkeys + capture channel fix (v0.2.1): 25/25 automated checks on
  lavapipe — `-vkcapture` frame 240 and PrintScreen shots at 1x/2x/3x all
  verified channel-correct by pixel analysis (status-bar digits pure red,
  e.g. (196,0,0); thousands of warm overlay glyphs; zero blue-swapped
  pixels) — the pre-v0.2.1 writer unconditionally swapped R/B and produced
  blue-shifted TGAs on every B8G8R8A8 surface. F12 cycled 1x→2x→3x live in
  a real game session (log lines, overlay mode row change measured across
  50% of the box, flash row visible in the shots), and a graceful quit
  wrote `shader_base_resolution 3` back to the config file — the choice
  persists. Hotkeys also exercised under `-vkdebug` with zero validation
  messages.
- F11 aspect cycle exercised live mid-demo: feed re-created 320x240 →
  320x200 → 356x200, chain re-derived, and the presentation letterbox
  switched exactly as specified (edge rows content 57.6/74.1 → pure black
  0.0/0.0, non-black coverage 99.6% → 83.0%; 16:9 fills a 16:9 target
  edge to edge).
- Stats overlay (v0.1.1): 24/24 automated checks on lavapipe — default-on
  box present, `-novkstats` and Scroll Lock each remove it on both
  backends, and the overlay text was decoded glyph-by-glyph out of the
  captured frames: `Vulkan | libslang-vk` / `llvmpipe (LLVM 19.1.7, 256
  bits)` / `demo-bright.slangp | 1 pass` / `275.9 fps | 3.62 ms |
  320x240 4:3` on the Vulkan path, and `SDL2 | classic` / `opengl | x11
  | 640x480` / `no shader (classic renderer)` / `35.1 fps | 28.52 ms |
  320x240 4:3` on the classic path (the engine's 35 Hz tic cap, as
  expected). The overlay's fps reading agrees with `-timedemo`'s own
  summary (275.9 vs 304 average).
- Session log + shared library (v0.4.0): 23/23 checks on the log module
  (`scripts/test_i_log.c`) — rotation to `.old` with the previous
  session's sentinel intact, header, newline append, truncation keeping
  its newline, idempotent re-init, close/after-close safety, disabled
  state without a config dir, and console dual-write preserved on both
  stdout and stderr. Both game build variants (ENABLE_VK_SHADERS on/off)
  compile clean under `-Wall -Wextra` for every touched file
  (i_log.c, i_video_vk.c, i_video.c, i_system.c). The shared build was
  exercised end-to-end on Linux: full CMake configure/generate through
  FetchContent, `slangvk-shared` built as `libslang-vk.so`, all 27 C API
  names exported (checked against the built binary with `nm -D`), the
  game targets link the shared artifact, and the generated build files
  carry the POST_BUILD copy next to each executable. The Windows
  `.def` export list is cross-checked 1:1 against the pinned `c.h` (27
  symbols, exact match); MSVC/MinGW link behavior with a .def is
  standard toolchain behavior not re-tested here.
- DLL deployment + CI packaging fix (v0.4.1): the four per-game
  POST_BUILD copies raced under parallel links (intermittent
  `open_for_read` "permission denied" — a Windows sharing violation —
  on the MSVC runner); replaced by one `slangvk-runtime` copy step the
  game targets depend on. Verified on Linux: full re-configure and
  generate for both `ENABLE_VK_SHADERS` settings, the generated build
  graph carries exactly one copy command with the intended ordering
  (after `slangvk-shared` and the destination anchor, before every game
  link), and executing the recorded copy command produces a
  byte-identical, idempotent deployment. MinGW builds now name the DLL
  `libslang-vk.dll` like MSVC (empty prefix; was the double-prefixed
  `liblibslang-vk.dll`). Both Windows packaging steps copy the DLL
  into the artifact zips and fail the job when it is absent; the
  MSYS2/MSVC runs themselves execute on GitHub's runners.
- Linux artifact + relocatable rpath (v0.4.2): the four games (and only
  those — setup/server never load slangvk) gain `BUILD_RPATH $ORIGIN`
  (macOS: `@loader_path`; Windows needs nothing), additive over CMake's
  automatic build rpath so the build tree keeps working; the shared
  library carries the same entry. Verified locally: full configure in
  both `ENABLE_VK_SHADERS` modes, the link commands of every game
  contain `$ORIGIN`, and `readelf -d` on the built `libslang-vk.so`
  shows the entry in its runpath. The CI packaging script's ldd walk
  was executed against the local build tree's binaries and libraries
  and collects exactly the non-glibc dependency closure; the MSYS2 and
  Linux jobs run on GitHub's runners.
- Not yet automated: interactive window resize / fullscreen-toggle
  swapchain recreation (exercised by design; mirrors the reference host's
  recreate logic — please report anything odd on real hardware).

## Known limitations

- 16:9 mode is a stretch, not true widescreen: the 320×200 frame is
  widened to fill a 16:9 canvas (how the console ports filled widescreen
  sets). True widescreen — rendering extra columns with a wider FOV —
  requires engine-side renderer changes (Crispy Doom is the reference
  implementation) and is out of scope for the presenter.
- On the classic SDL2 renderer (fallback), `aspect_ratio_correct = 2`
  renders as corrected 4:3; the 16:9 feed mode is Vulkan-path-only.
- The Vulkan device is created by the library (create mode); on
  multi-GPU systems it picks the first graphics-capable device and checks
  that it can present to the window's surface. Hosts wanting full control
  can adopt their own device instead (`slangvk_device_adopt` — not wired
  in this fork yet).
- Emscripten is rejected at configure time when `ENABLE_VK_SHADERS=ON`.
- `-timedemo` plus upstream's ENDOOM screen waits for a keypress on exit —
  that is unchanged upstream behavior (it only matters in headless runs).

## License & attribution

Upstream [Chocolate Doom](https://www.chocolate-doom.org): GPL-2.0,
Copyright (C) 1993-1996 Id Software, Inc., 2005-2014 Simon Howard et al.
[libslang-vk](https://github.com/VTSTech/libslang-vk) (MIT, Copyright (C)
VTSTech) is fetched at configure time and statically linked; the combined
work is distributed under the GPL-2.0 (see `COPYING.md`). Freedoom
(BSD-3) is used for automated testing only and never distributed with
this fork. This fork adds no shader presets of its own; anything you
point `-shader` at is governed by its own license (the slang-shaders
corpus is largely MIT — check each preset's header). The stats overlay
embeds [font8x8](https://github.com/dhepper/font8x8) (public domain,
Daniel Hepper / Marcel Sondaar / IBM).
