# Changelog

Notable changes to **Chocolate Doom VK** — the Vulkan slang-shader path
this fork adds on top of upstream Chocolate Doom. Upstream changes keep
living in upstream's own history and are not tracked here.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions: this repository's release tags (`v3` ships as package
version `3.0.0`; point releases tag as `3.0.<n>`, package version
matching).

## [Unreleased]

### Added
- **`vk_log_console` config variable / `-vklog` switch — console echo
  for the session log.** Echoes the Vulkan path's log stream (the
  `I_VK: …` lines) to stdout/stderr as each line is written, for live
  debugging from a terminal; `gpu-scaler.log` keeps every line either
  way.

### Changed
- **The Vulkan path no longer mirrors its log onto the terminal.**
  `I_LogPrintf`/`I_LogError` used to write every `I_VK: …` line to
  stdout/stderr in addition to `gpu-scaler.log` — a behaviour meant
  for Windows GUI-subsystem builds, where the console is invisible
  anyway. From a Linux terminal it meant a wall of scattered output:
  unbuffered stderr interleaved with the startup progress dots' bare
  `.`, and the mess sat there when the game exited. The log file is
  now the only destination by default; echo is opt-in
  (`vk_log_console` / `-vklog`) and comes back automatically when the
  log file cannot be opened, so a run with no durable home still
  explains itself on the console. Fatal `I_Error` output is
  unaffected (it writes stderr directly and always has).

- **Aspect and base-resolution hotkeys moved off the vanilla F-keys.**
  The Vulkan presenter's dispatch in `I_GetEvent()` ran before the
  engine's responders, so while the Vulkan path was live its hotkeys
  shadowed real vanilla features: F11 is the gamma/brightness cycle in
  Doom, Heretic and Hexen, and F12 is the spy camera in Doom/Heretic
  (dev-map reload in Hexen) — none of those reached the game. The
  aspect cycle moves to **Alt+A** and the shader base-resolution cycle
  to **Alt+B**, following the same Alt-combo convention as the fork's
  Alt+Enter fullscreen shortcut. The letter is intercepted only with
  Alt held and only while the Vulkan path is active: plain A/B pass
  through, F11/F12 always reach the engine (even mid-VK-run), and with
  `-novulkan` the Alt-combos fall through untouched. Alt is still the
  strafe modifier — same as Alt+Enter, harmless unless a movement key
  is held simultaneously. PrintScreen (screenshot) and Scroll Lock
  (stats overlay) are unchanged.

## [3.0.4] - 2026-10-24

The Vulkan presenter gets frame-pacing controls: a presents-per-second
limiter (`vk_fps_cap` / `-vkfps`) that can throttle below the tic rate
for GPU load / battery relief or — above 35 — uncouple presentation
from the 35 Hz game loop entirely, plus a swapchain present-mode
selector (`vk_present_mode` / `-vkpm`, availability-checked with FIFO
fallback). The 3.0.3 known issue is closed: the dedicated server's
bare-run instant crash on Windows (the UCRT rejecting `setvbuf`'s
line-buffered mode), and the crash trap now also covers the CRT's
silent invalid-parameter fastfail path. Every CI artifact now carries
its commit.

### Added
- **Frame limiter + present mode selection for the Vulkan presenter.**
  `vk_fps_cap` / `-vkfps <n>` holds each present until a minimum
  interval has elapsed since the last accepted one (performance-counter
  timed: millisecond sleep, then a bounded busy-wait tail —
  `SDL_Delay`'s scheduler-tick rounding alone would cost whole frames
  at 60+ caps), giving a hard presents-per-second ceiling for GPU load
  and battery relief (`-vkfps 30`) and the throttle that non-pacing
  present modes need. `vk_present_mode` / `-vkpm <n>` selects the
  swapchain present mode — FIFO (default, vsync), MAILBOX (triple
  buffered, queued frames replaced), IMMEDIATE (may tear),
  FIFO_RELAXED — availability-checked against the surface with a
  silent FIFO fallback, and MAILBOX gets its third swapchain image so
  replace-queued-frames semantics actually engage. Caps of 1..35 pace
  the whole loop below the tic rate, so game time dilates rather than
  skipping. The boot log names the resolved mode and any active
  limiter; the stats overlay's fps row reflects the cap automatically.
- **Presentation uncoupled from the 35 Hz game loop (`-vkfps` > 35).**
  `TryRunTics()` used to spin-wait for the next tic on every main-loop
  iteration, welding the present rate to the logic rate — so any cap
  above 35 had nothing to pace. When the Vulkan presenter is live, the
  cap exceeds TICRATE and no netgame is connected, an iteration with
  no due tic now returns immediately and the frame limiter paces the
  loop instead: `-vkfps 60` really presents ~60 fps (FIFO settles at
  the display refresh; `-vkpm 1` MAILBOX skips queue latency), while
  game logic stays vanilla-locked at 35 Hz. Frames between tics repeat
  the current state — motion still advances 35 times per second — but
  the shader chain and its per-frame uniforms now run at the present
  rate, so animated CRT/LCD presets are visibly smoother. Netgames
  (including loopback `-server` hosts) keep the blocking wait for
  server tics; `singletics` / `-timedemo` are untouched. Smooth
  in-between motion remains Tier 3 (view interpolation).
- **The crash trap covers the CRT's silent leg.** A CRT call handed a
  bad argument (a bad `FILE *`, a bad mode, ...) does not raise an SEH
  exception and does not raise `SIGABRT` — release builds jump straight
  to `0xC0000409` fastfail from inside the runtime, invisible to both
  handlers the server already had. That is precisely the shape of the
  bare-run death whose boot log stops one stage short. MSVC builds now
  install an invalid-parameter handler that names the argument,
  function and source line in the boot log, mirrors the report in the
  message box, and exits with code 4 (3 = abort; GS-cookie and
  range-check fastfails stay uncatchable by OS design, so
  `$LASTEXITCODE` still arbitrates those). The boot log also gained
  two stages — `args parsed` and `stdout unbuffered` — and that is
  what closed the case: the user's next crash log ended at
  `args parsed`.
- **Every CI artifact now carries its commit.** All four
  artifact-producing jobs across `main.yml` and `vk-shaders.yml` stamp
  what they upload: a `BUILD-INFO.txt` (full commit hash, ref, event,
  package version, UTC build time, run URL) rides inside each zip and
  tarball, the artifact names in the Actions UI gain the short hash
  (`MSYS2 UCRT64-<hash>`, `chocolate-doom-vk-windows-msvc-<hash>`,
  ...), and branch/PR builds of the MSYS2 release job append it to the
  game zip filenames themselves
  (`chocolate-doom-3.0.3-win64-<hash>.zip`) so two runs of the same
  version can be told apart. Tag builds keep the canonical
  `<game>-<version>-<arch>.zip` names for the GitHub Release; their
  `BUILD-INFO.txt` still pins the exact commit.

### Fixed
- **The server's bare-run instant crash on Windows: found, understood,
  fixed.** The killer was the server's own startup line
  `setvbuf(stdout, NULL, _IOLBF, 0)`. The C standard lets the CRT pick
  a size when the buffer pointer is null, and glibc honors that; the
  UCRT instead documents and enforces `2 <= size <= INT_MAX` for
  buffered modes, and its release build enforces it through the
  no-info invalid-parameter path — no SEH exception, no SIGABRT, a
  straight `0xC0000409` fastfail out of the process. That is the
  instant, silent death the crash trap and boot log were built for:
  the boot log's last stage pinned the call, and the trap's new leg
  caught the validation red-handed (`CRASH: CRT invalid parameter (?
  in ?, ?:0)` — the `?`s are the UCRT saying it has no details in
  release). The fix asks for unbuffered output instead: `_IONBF`
  needs no size, so the validation cannot fire, and it is the only
  mode the UCRT actually delivers per-statement visibility with (its
  `_IOLBF` is full buffering in disguise) — the property the original
  call was after in the first place.

## [3.0.3] - 2026-10-24

The third point release: the dedicated server becomes a diagnosable
program — a console it actually writes to, a crash trap that files a
report, a boot log that records how far boot got, a `--help`, and a
banner that names its build and UDP port — the GUI games answer
`--version` from a terminal at last, the shader browser climbs
folders in all four games, and the MSYS2 build links the server
again. Cut with one known issue still open (below): the server's
bare-run crash on Windows.

### Added
- **The server answers `--help`.** The stripped-down dedicated-server
  entry point had lost upstream's usage block, so `--help` fell
  through into normal startup — indistinguishable from the mystery
  instant crash. It now prints options (`-port`, `-privateserver`,
  `-netlog`, `-nogui`, `--version`, `--help`) and exits.
- **`chocolate-doom.exe --version` finally prints something.** The
  games are GUI-subsystem executables whose stdout belongs to no
  console, so version/help output from a PowerShell or CMD launch
  vanished. They now attach the parent console just for those
  arguments (double-clicked launches stay quiet like a GUI app).
- **Boot-stage log for the server.** `chocolate-server.log` (next to
  the exe, working directory as fallback) records each boot step and
  flushes every line — a server that still dies instantly and
  silently is diagnosed by its last line; the crash trap writes the
  exception report there too.
- **The shader browser climbs folders.** The SHADERS menu in all four
  games now walks one folder at a time instead of flat-listing a
  merged scan: subfolder rows render as `NAME/`, Enter descends into
  one, and a literal `..` row (shown whenever the folder has a parent)
  climbs back up — so preset packs laid out like the slang-shaders
  repo (`crt/`, `handheld/`, ...) are browsable entirely in-game.
  The browser still opens on the active preset's own folder (falling
  back to the drop-in `shaders/` folders, which now appear as
  descendable rows even when empty), re-scans on every open so
  mid-session drops show up, and pages exactly as before (up/down one
  row, left/right a windowful, `*` marks the active preset). Backend
  is `I_ShaderBrowse()`/`I_ShaderBrowseGoto()` in `i_video.c` (the
  old `I_ShaderPresets()` flat scan is gone) plus a `GLOB_FLAG_DIRS`
  mode in `i_glob.c` so the folder listing rides the existing sorted,
  case-insensitive glob machinery.

### Fixed
- **chocolate-server is no longer a silent black box on Windows.**
  Upstream's 2018 blanket "apply WIN32 to executables" built the
  dedicated server as a GUI-subsystem binary — no console, so its
  banner and every diagnostic printf went nowhere; a double-clicked
  server ran (or died) invisibly. The server is now a console app (a
  console window it actually writes to), its stdout is line-buffered
  so redirected output survives a kill, and the startup banner names
  the UDP port it will bind (`-port`, default 2342, echoed back; a
  bind failure was already an `I_Error` box) plus the
  `-privateserver` note. Verified in-sandbox: the server boots,
  prints the banner through a pipe, and serves until killed.
- **chocolate-server survives its own crashes with a report.** A
  Windows-native fault (access violation, heap-corruption abort, ...)
  killed the console-subsystem server instantly and silently — a
  window flash with zero diagnostics, indistinguishable from every
  other instant death. The server now installs a crash trap
  (`SetUnhandledExceptionFilter` + a `SIGABRT` handler, server only)
  that prints the exception code, a plain-English translation, and
  the faulting address to stderr and mirrors it in a message box that
  stays up until acknowledged; an address landing inside a DLL (e.g.
  `SDL2_net.dll`) rather than the exe points straight at the culprit.
  (`0xC0000409` fastfail stays uncatchable by OS design.) Also: the
  server target now defines `SDL_MAIN_HANDLED` so the console binary
  enters through its own native `main` — MSVC console builds no
  longer need the SDL2main/WinMain detour the GUI game binaries use
  (`i_main.c` calls `SDL_SetMainReady()` for such targets) — and
  `NET_SDL_InitClient`/`NET_SDL_InitServer` check `SDLNet_Init`'s
  return value and the receive-packet allocation instead of ignoring
  them, so a refused WSAStartup or an allocation failure names its
  stage in an `I_Error` instead of dying downstream with no hint
  networking never came up.
- **The version string says what the tags say.** `3.0.2` was tagged
  with the build files still reading 3.0.1, so every binary
  self-reported 3.0.1; `project()`/`AC_INIT` now carry 3.0.3 and the
  server banner prints the full `PACKAGE_STRING` so the build an exe
  came from is always answerable.
- **MSYS2 (MinGW) builds link the server again.** The MSYS2 CI job
  died at `chocolate-server.exe` with `undefined reference to
  SDL_main`: `CMAKE_FIND_PACKAGE_PREFER_CONFIG` (default On) makes
  `find_package(SDL2)` use SDL2's own package config, and on MinGW
  its `SDL2::SDL2main` target carries `-Wl,--undefined=WinMain` —
  which force-loads SDL2main's `WinMain` object, which calls
  `SDL_main`, a symbol the server no longer defines now that its
  `main()` is a real `main` (`SDL_MAIN_HANDLED`). MSVC console exes
  never reference `WinMain`, which is why only the MSYS2 job broke.
  The server now links without SDL2main on Windows (it needs none of
  it); game binaries keep their exact linkage, and non-Windows
  platforms are untouched.
- **No more bogus `%zu` format warnings on MinGW.** GCC's MinGW
  `printf` format archetype predates C99 and flagged every `%zu` in
  `I_Error`/`M_snprintf` messages (`unknown conversion type
  character 'z'`) even though the UCRT runtime formats it correctly;
  the printf attribute is now `gnu_printf` under MinGW
  (`doomtype.h`, `textscreen/txt_main.h`), which knows the full C99
  set.

### Known issues
- **The server's bare-run instant crash on Windows is still open.**
  The new boot log narrows it to a handful of lines: `main` is
  entered, `D_DoomMain` is entered, the crash trap installs — and the
  log stops there. The next stage sits behind two argv scans and a
  `setvbuf` that have no business crashing, and neither the exception
  filter nor the `SIGABRT` handler filed a report — which fits the
  uncatchable class of deaths: `0xC0000409` fastfail, taken by design
  by the MSVC release CRT's `abort()` and invalid-parameter paths.
  One number narrows it further: `$LASTEXITCODE` from the PowerShell
  run, read immediately after the process dies.

## [3.0.2] - 2026-10-23

The second point release: the SHADERS menu row becomes a working shader
browser in all four games — every `.slangp` preset is listed in-game and
hot-swappable on the Vulkan path — and Strife's keyboard-only inventory
controls become bindable on a gamepad, with setup-tool rows and
Strife-tuned calibration defaults to match.

### Added
- The SHADERS row is now a **shader browser** in all four games. It
  pages through every `.slangp` preset found in the active preset's
  own folder, `shaders/` beside the executable and `shaders/` in the
  config dir — presets dropped into either folder appear the next
  time the browser opens, no restart needed. The list is a sliding
  window that scrolls both ways over the whole preset set — arrow keys
  move one row, left/right skip a windowful — and it opens on the
  active preset (marked `*`), drawn at normal small-font size (the
  rows were briefly double-height). Enter applies the highlighted
  entry. With the Vulkan path live the swap is hot:
  the new preset is parsed and a complete new chain built against the
  live swapchain *before* the old chain/preset pair is destroyed
  (prepare-then-commit), so a broken or missing preset cannot leave
  the renderer chainless — the worst case is the current shader
  staying on screen, with the reason flashed on the stats overlay and
  logged. Device, swapchain and input image are untouched; only the
  chain/preset pair is replaced, deliberately a far smaller surface
  than the full backend restart whose removal was its own fix. The
  SPIR-V disk cache applies to swaps exactly like warm starts, so
  revisiting a preset seen earlier in the session is milliseconds.
  On the classic renderer the pick persists (the `shader` config key,
  and a stale `usevulkan = 0` is re-enabled exactly like a `-shader`
  command-line pass) and a message notes that a restart applies it;
  there is still no "shaders off" row.
- **Strife's Strife-only controls now exist on gamepads.** Upstream
  reads them from the keyboard only, so the setup tool had no rows it
  could offer: use medkit, drop item, inventory popup, objectives
  popup, keys popup, item query and jump-to-first/last inventory item
  are new `joyb_*` config keys wired into the engine (ticcmd flags for
  medkit/drop exactly like the keyboard, edge-detected presses for the
  popups so holding a button doesn't page through them) and offered in
  the setup tool under Gamepad/Joystick → "More controls...". The
  modern-gamepad defaults applied by calibration are Strife-tuned: the
  dpad walks the inventory instead of the fly controls Strife never
  had, L3 uses a medkit and Select drops the highlighted item. Doom,
  Heretic and Hexen keep the stock layout.
- CMake builds now produce the four per-game `chocolate-*-setup`
  binaries, same as autotools — the setup tool picks which game to
  configure from its executable name, so `chocolate-strife-setup` is
  the (only) way to get a Strife-specific settings window.

### Fixed
- The shader browser's first landing turned every CI build job red:
  `I_VK_SetPreset` wired up the SPIR-V disk-cache callbacks
  (`VK_SpirvCacheLoad` / `VK_SpirvCacheStore`) about 140 lines before
  their definitions. GCC before 14 let the implicit declarations
  through as warnings — which is why local builds passed — but MSVC
  rejects them outright (C2065) and GCC 14+ errors too, so every
  platform failed on the same commit. The callbacks are now
  forward-declared, and the browser's `n = strlen(configdir)` got the
  explicit `(int)` cast that MSVC's C4267 was asking for. Build-only
  fix; no functional changes.

## [3.0.1] - 2026-10-22

The first point release: an in-game FOV slider and a new ADVANCED
options submenu in all four games, weapon sprites that keep their
vanilla size at any FOV, floors and ceilings that hold still under
the camera, and a README that describes what the fork actually is.

### Added
- ADVANCED options submenu in all four games, replacing the END GAME
  row in the Options menu (F7 still ends the game, as before). The
  FOV slider moved into it from the Options menu; labels for the new
  rows are drawn with the small font scaled to the height of the M_*
  patches, since no ADVANCED patch ships with the game WADs (same
  technique the FOV row introduced).
- SHADERS placeholder row in the Options menu of all four games
  (replacing Graphic Detail in doom and the FOV row in
  heretic/hexen/strife). The row is named and selectable but
  intentionally inert — see Removed below.

- In-game field-of-view slider in the Options menu of all four games
  (64-128 degrees, default 90 = vanilla).  The weapon sprite scales
  with the projection and stays anchored to the bottom of the view;
  at 90 degrees the code paths are bit-identical to vanilla (non-default
values are computed exactly from the half-angle tangent).  In
  doom and strife the label and value are drawn with the small font
  scaled up to the height of the other menu rows, so the FOV row
  matches the rest of the options menu.  Saved as `fov` in the
  config file; purely a rendering change, so demos and network
  games are unaffected.
- `patches/` archive entries 0014–0018, keeping the in-tree patch
  record aligned with the git history (the autotools build fixes, the
  FOV slider batches, and the reverted ui-scale experiment, kept for
  reference).

### Changed
- README description rewritten: the fork no longer bills itself as
  aiming to "accurately reproduce" the original DOS versions — it now
  aims to "upgrade and enhance" them to a form that runs on modern
  computers with Vulkan GPU rendering, every frame passing through a
  libretro slang shader preset (.slangp / .slang) via libslang-vk, and
  lists FOV and widescreen upscaling support among its other
  improvements.
- Graphic Detail removed from the doom Options menu; the renderer is
  locked to HIGH detail. `detailLevel` is forced back to 0 (HIGH)
  after config load in doom — stale `detaillevel = 1` values in
  existing configs are ignored and overwritten on the next save.
  heretic/hexen/strife never bound `detaillevel`, so they were always
  HIGH already.
- The FOV slider no longer scales the player sprite. The weapon keeps
  its vanilla size at every field of view (64-128); only the view
  frustum widens or narrows, which reads as the camera moving forward
  or back rather than the gun growing and shrinking. The sky keeps its
  projection-relative size via a new `skyiscale` (prboom-style) so
  decoupling the weapon does not change the sky; heretic/hexen already
  drew the sky from their own `skyiscale`.

### Removed
- The SHADERS ON/OFF toggle — the menu action, the **Shift+F12**
  hotkey in all four games, and the F5 binding in doom and heretic.
  Turning shaders off left the game unplayable: the classic SDL2
  fallback path kept stale frame content on screen (graphics
  overwriting themselves), and restarting the video backend mid-session
  also desynced view state. The Vulkan + slang path is now always used
  when it is available; the menu row remains as a named placeholder for
  a future use, and the keypad **+** / **-** keys are now accepted as
  aliases for the screen-size hotkeys alongside the main-row keys.
- Upstream project documentation left over from the 1.x tree:
  `NEWS.md`, `TODO.md`, `RELEASE_NOTES.md` and the old `ChangeLog` are
  gone, and `CONTRIBUTING.md`, the issue template, `AUTHORS`,
  `CODE_OF_CONDUCT.md` and the packaging metadata (`rpm.spec.in`,
  `pkg/config.make.in`) no longer self-identify as plain Chocolate
  Doom — the repository presents as Chocolate Doom VK throughout.

### Fixed

- Floors and ceilings no longer swim ("the floor textures are
  moving/rotating") or smear into radial streaks, and they now line up
  with the walls at any FOV. Four defects in the flat mapping: the
  span origin came from the 8192-entry fine tangent tables, so distant
  floor texels jumped by up to a full texel or more every time the
  view angle crossed a table step; the exact-origin replacement first
  shipped with the pixel offset divided into the fixed-point
  projection unconverted, which started every span row on the
  center-ray texel; that first unit fix still left the ray length
  computed in fixed-point 16.16 units and then multiplied by 65536 a
  second time, so the +/-32768 range clamp (meant for world units)
  saturated every origin to half a texel and the overflowed
  conversion wrapped int32 - every span origin became garbage and the
  flats rendered as static radial smears at any FOV, default included
  (floors and ceilings only, walls unaffected); and the per-row
  distance was built from `viewwidth/2` instead of the FOV focal
  length, so at any FOV other than 90 the floors were sampled at a
  different depth than the walls show (up to ~73% too far at
  FOV 120). A fifth defect made the flats look almost correct from a
  single heading only: the angle conversion macro divided the binary
  angle by 2^30 instead of by the degrees-per-binary-unit (2^32/360),
  shrinking every view angle 90x, so the flats kept the perspective of
  a viewer permanently facing east - correct facing one way, with
  walls rotating over a frozen floor/ceiling pattern at every other
  heading. The span origin and the per-row step are now derived from
  exact sin/cos/atan of the true column ray (doubles, no table grid),
  in real map units throughout; the view offset is wrapped int32
  exactly like vanilla fixed-point math (the span blitter masks texels
  to 6 bits, so wrap-around is harmless, while the previous clamp
  pinned far and map-edge spans to a wrong texel phase); and the step
  coefficients are rounded half away from zero instead of truncated,
  removing up to a full 16.16 lsb of per-row step bias. At FOV 90 with
  320x200 the wall/sprite/sky rendering is untouched.
- The doom/strife menu labels ("ADVANCED...", "SHADERS", "FOV") were
  drawn by stretching every HU-font glyph to the full 15px box of the
  M_SVOL title patch: visibly taller than the 12px item patches around
  them, and the 4x3 period glyph became a 21x16 solid block that read
  as "ADVANCED---". M_DrawTextScaled now scales uniformly to a
  reference patch's visible ink height (M_MSGOFF for rows, M_OPTTTL
  for the submenu title, M_GDHIGH for values), preserves glyph aspect
  ratios, and aligns punctuation to the cap baseline — the dots are
  dots again, and the ADVANCED submenu title is centered like the
  OPTIONS title.
- The doc-cleanup batch expanded the TAB recipe prefixes in the root
  `Makefile.am` `INSTALL` rule to spaces (the same tooling failure mode
  as patches 0002/0004), so every autotools build died at parse time
  with GNU make's `Makefile:1000: *** missing separator (did you mean
  TAB instead of 8 spaces?)`.  The recipe is restored byte-identical to
  the upstream form (tab-prefixed first and last lines, space-indented
  backslash continuations); verified with `autoreconf -fi` plus a
  before/after `make -n` parse test of the generated rule.
- The upstream "Continuous Integration" workflow (autotools) has been
  red since the gpu-scaler.log batch: `i_log.c` and `i_vk_cache.c`
  were added to the CMake build but never to `src/Makefile.am`, so
  every autotools link — all four games, setup and the dedicated
  server — failed with undefined `I_LogMessage`/`I_LogPrintf`/
  `I_LogError` (`i_system.c`'s fatal-error hook and the classic-
  fallback announcement in `i_video.c` call the log API
  unconditionally, by design). Both modules now sit in the autotools
  source lists, mirroring their CMake placement (`COMMON_SOURCE_FILES`
  and the game files). The `mus2mid` tool rule was a second casualty of
  the same omission (it compiles `i_system.c`, whose fatal-error hook
  calls the log): it gained `i_log.c` plus the `m_config.c` chain that
  pulls in, and its libraries now follow its sources on the link line
  — the old order (`-lSDL2` before the objects) left upstream's own
  SDL calls in `i_system.c` unresolved, a latent bug the missing
  `I_LogMessage` had been hiding behind. `make mus2mid` links again.
  The autotools build remains classic-renderer-only (`ENABLE_VK_SHADERS`
  is a CMake-side option); it builds the log and cache modules as
  their no-op stubs.

## [v3] - 2026-10-21

The GPU-scaler maturity release: true 16:9, warm-start shader caching,
survivable GPU driver resets, a session log you can actually read on
Windows, and `libslang-vk` as a shared DLL.

### Added
- **16:9 aspect** — a third stop on the F11 cycle
  (4:3 → 16:10 → 16:9 → 4:3; `aspect_ratio_correct` 0/1/2). The feed is
  column-stretched to 356x200 *before* the shader chain, so presets see
  a genuine 16:9 source and CRT masks stay square on a 16:9 display.
  The classic renderer treats any non-zero value as 4:3. (Stretched,
  not true widescreen FOV — that needs engine-side column rendering;
  see README-VK.md, "Known limitations".)
- **SPIR-V disk cache** — compiled passes persist in `shader-cache/`
  beside the executable and reload in milliseconds on warm starts
  instead of full slang recompiles. Every entry is validated on read
  (magic, size caps, exact file length) and cache keys are whitelisted
  to `[0-9a-f]{1,64}`, so a corrupt or hostile file is a silent cache
  miss, never a failed startup. Writes are atomic (temp file + rename).
  Built on libslang-vk M4.07's new `slangvk_spirv_cache` API.
- **Device-lost recovery** — `VK_ERROR_DEVICE_LOST` (driver crash /
  Windows TDR) no longer kills the game with `I_Error`. All three loss
  surfaces (render, acquire, frame upload) re-acquire host objects,
  drop the dead swapchain and rebuild it lazily on the next frame,
  then confirm recovery with an overlay flash and a log line. Only a
  failed *recovery* is fatal.
- **`gpu-scaler.log`** — a timestamped session log written beside the
  executable (previous run rotated to `.old`). Windows GUI builds have
  no console, so the path's ~36 printf diagnostics were invisible
  exactly where they matter most; startup, preset state, SPIR-V cache
  stats, hotkey actions, fallback reasons and even the final `I_Error`
  message now land in the log, with a per-line flush so a crash never
  truncates the tail. Best-effort by design: an unwritable location
  degrades to console-only and never blocks the game.
- **PrintScreen** hotkey — screenshot at any moment: reads back the next
  presented frame to the first free `vkshotNNN.tga` (names probed with
  `fopen`, so nothing is ever overwritten). Same channel-correct TGA
  writer as `-vkcapture`.
- **F12** hotkey — cycle the shader base resolution 1x → 2x → 3x → 4x →
  1x, riding the same re-create-and-re-derive path as the F11 aspect
  toggle; the overlay mode row follows immediately and the choice
  persists in `shader_base_resolution` on exit.
- Transient amber feedback row (2 s) in the stats overlay for either
  hotkey (`shot vkshot000.tga` / `base 640x480 (2x)`).
- **Linux x64 CI artifact** — the lavapipe smoke-test job now ships
  what it builds: `chocolate-doom-vk-linux-x64.tar.gz` with the four
  games, setup, the dedicated server, `libslang-vk.so` and the
  bundled runtime libraries (SDL2, SDL2_mixer, libpng, the X11 stack
  — everything except the glibc core, which can never be bundled).
  The executables carry a `$ORIGIN` rpath, so the tar runs from any
  directory on any distro new enough for the runner's glibc (see the
  README.txt inside the archive). lavapipe itself is only the CI test
  driver — the binaries use whatever Vulkan ICD the player has, or
  none (classic renderer).
- `README-VK.md` grows with the code: 16:9 aspect table, Shader cache
  and Session log sections, shared-library build instructions, and a
  Validation section recording the per-batch test harness runs.
- This `CHANGELOG.md`.

### Changed
- **The fork now identifies as Chocolate Doom VK 3.0.0** — every
  user-facing surface used to say upstream `Chocolate Doom 3.1.1`:
  the startup banner, the `gpu-scaler.log` header, the window title,
  `-version` output, message-box titles, the Windows file properties
  (VERSIONINFO) and the string exchanged with network peers.
  Executable, config and savegame names deliberately stay
  `chocolate-*` (`PROGRAM_PREFIX` is unchanged), so existing installs
  keep working. One behavioral note: the network version check
  compares this string verbatim, so network games now need matching
  VK builds — upstream 3.1.1 peers get a clean version-mismatch
  rejection (and would desync anyway).
- `libslang-vk` pinned to **M4.07** (commit `d176d1b…`, immutable full
  hash; v2 used the mutable `M4.05` tag). The M4.05-era library had
  bugs that made the Vulkan path fail to start on real drivers — the
  game silently fell back to the classic renderer — so v3 rides the
  M4.06 fixes and settles on M4.07 for its additions: the SPIR-V cache
  API the disk cache above is built on, C-API null-handle validation
  (SEC-07) and a zero-allocation render path (PERF-09). Bumping the pin
  is always a deliberate hash swap in `CMakeLists.txt`.
- **`libslang-vk` now builds as a shared library** — `libslang-vk.dll`
  (identically named on MSVC and MinGW — an empty target prefix keeps
  MinGW from doubling it to `liblibslang-vk.dll`) is deployed beside
  the game executables at build time by a single `slangvk-runtime`
  copy step (`libslang-vk.so` on Linux), exporting its 27 C-API
  symbols via `cmake/libslang-vk-c-api.def` (MSVC and MinGW need the
  explicit `.def`; ELF exports the symbols by default anyway).
  Relinking a game no longer relinks the shader compiler. The Linux
  executables carry a `$ORIGIN` rpath so they — and a relocated
  `libslang-vk.so` — keep finding each other next to one another
  outside the build tree; on Windows the exe's directory is searched
  by default. Prefer static
  again? One CMake toggle — see README-VK.md, "Building".
- F11 is now a three-way aspect cycle (was 4:3 ↔ 16:10).
- Stats overlay: the shader row shows the preset's basename
  (`crt-easymode | 3 passes`) instead of the full path, and the old
  "1:1" aspect label is now correctly labeled "16:10".

### Removed
- `cpp-linter` GitHub Actions workflow.

### Fixed
- A silent-fallback trap: if an earlier `-novulkan` run persisted
  `usevulkan = 0`, later runs with `-shader <path>` stayed on the
  classic renderer with no indication why. Passing `-shader` on the
  command line is explicit intent and now re-enables the Vulkan path
  for that run (and persists `usevulkan = 1`); `-novulkan` still wins
  when both are given.
- Vulkan fallbacks are no longer invisible: every failure exit of
  `I_VK_AttemptGraphics` records a human-readable reason, shown in the
  stats overlay's shader row (`shader.slangp (unused: preset failed to
  load)`) and written to stdout *and* `gpu-scaler.log` — stderr is
  invisible in Windows GUI builds, exactly where fallbacks bite
  hardest.
- MSVC/vcpkg builds linked an SDL2 **without** Vulkan support (the
  `vulkan` feature of vcpkg's `sdl2` port is not in its defaults), and
  the manifest's CLI-style `"sdl2[vulkan]"` string is invalid in
  manifest mode and broke `vcpkg install` outright. `vcpkg.json` now
  uses the proper object form —
  `{"name": "sdl2", "features": ["vulkan"]}` — and additionally
  requests the `vulkan` port (headers + ICD loader) for the renderer's
  own `find_package(Vulkan)`.
- The Windows CI artifacts ship `libslang-vk.dll` — and the jobs fail
  loudly when it is absent. Two holes closed: the MSYS2 zip's ldd-based
  runtime walk only collected `/ucrt64` DLLs and silently omitted it
  (the zip launched straight into a "not found" dialog), and the MSVC
  package step never looked at the build tree at all. Deployment itself
  no longer races either: the initial per-game POST_BUILD copies could
  collide on the same destination under parallel links (a Windows
  sharing violation — intermittent `permission denied` on the MSVC
  runner); one copy step that every game depends on replaces them.
- `-vkcapture` screenshots came out blue-shifted. The readback
  (`slangvk_device_read_pixels`) is a raw `vkCmdCopyImageToBuffer`, so
  bytes arrive in the surface format's native packing: B8G8R8A8 memory
  is B,G,R,A — already exactly TGA's component order — but the writer
  swapped channels 0 and 2 unconditionally, red/blue-flipping every
  capture on every B8G8R8A8 swapchain (virtually every real platform).
  The writer now swaps only for R8G8B8A8* surfaces, per the actually
  chosen `VkFormat`.

## [v2] - 2026-10-21

### Added
- On-screen stats overlay, drawn into the software frame before
  presentation on **both** backends: FPS, presenter (`Vulkan |
  libslang-vk` / `SDL2 | classic`), active shader preset and pass
  count, adapter / renderer string. **Scroll Lock** toggles it live;
  `-novkstats` starts without it (`vkstats` config key persists).
- Shader base resolution, mirroring DevilutionX's libslang-vk knobs:
  `-vkbase <n>` / `shader_base_resolution` accepts a 2/3/4 multiplier
  (or a pixel width, e.g. `640`) and rebuilds the input image at that
  size before the chain, so shaders designed for higher-resolution
  sources light up correctly.
- **F11** hotkey — live 4:3 ↔ raw 320x200 (16:10) aspect toggle; the
  feed re-creates and the chain re-derives on the next frame.
- `./patches/` — the per-feature patch archive this fork keeps in-tree
  (0001–0006 at this point).

## [v1] - 2026-10-21

### Added
- Vulkan presentation path via
  [VTSTech/libslang-vk](https://github.com/VTSTech/libslang-vk) (M4.05,
  FetchContent): the 8-bit software frame is expanded through the
  palette LUT into an input image and run through any libretro slang
  preset chain (`.slangp`), presented via a Vulkan swapchain sharing
  the SDL window. The games' own sources stay C99; the classic SDL2
  renderer is untouched and takes over on any failure or `-novulkan`.
- CLI: `-shader <path>` (activates the path, persists in `shader`),
  `-novulkan`, `-vkdebug` (validation layer), `-noaspect` (raw 16:10
  feed), `-vkcapture <n>` (dump the n-th presented frame to
  `vkframe.tga`).
- Build: `ENABLE_VK_SHADERS` option in both CMake and autotools; no
  Vulkan SDK needed anywhere (volk at runtime, entry points via
  `SDL_Vulkan_GetVkInstanceProcAddr`).
- CI: `.github/workflows/vk-shaders.yml` — Linux lavapipe smoke test
  plus MSYS2 UCRT64 and MSVC (vcpkg) Windows builds.
- `README-VK.md` — full feature documentation.

### Fixed
- Windows CI: MSYS2 packaging zipped nothing (bare `.` with no `-r`;
  now globs the files and walks `ldd` over every game binary for the
  runtime DLLs). MSVC job: `lukka/run-vcpkg` refused to run because
  upstream's `vcpkg.json` ships no builtin-baseline — pinned one
  (vcpkg release `2026.07.29`, commit
  `9e593bb18ea69cc5095e012465dcd675a822ed0d`) so the manifest resolves
  deterministically.

[Unreleased]: https://github.com/VTSTech/chocolate-doom-vk/compare/3.0.2...HEAD
[3.0.2]: https://github.com/VTSTech/chocolate-doom-vk/compare/3.0.1...3.0.2
[3.0.1]: https://github.com/VTSTech/chocolate-doom-vk/compare/v3...3.0.1
[v3]: https://github.com/VTSTech/chocolate-doom-vk/compare/v2...v3
[v2]: https://github.com/VTSTech/chocolate-doom-vk/compare/v1...v2
[v1]: https://github.com/VTSTech/chocolate-doom-vk/releases/tag/v1
