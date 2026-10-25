//
// i_video_vk.h — Vulkan post-FX presenter interface (Chocolate Doom VK).
//
// Copyright (C) 2025 VTSTech (Nigel Todman).
// License: GNU General Public License, version 2 (see COPYING.md).
//
// Integrates VTSTech/libslang-vk: libretro slang shader presets (.slangp)
// executed over the 320x200 software frame through a Vulkan swapchain.
//
// Call sites live in i_video.c; every entry point is a no-op (and
// I_VK_AttemptGraphics() returns false) when ENABLE_VK_SHADERS is not
// defined, so the classic SDL2 renderer path is completely unaffected.
//

#ifndef __I_VIDEO_VK__
#define __I_VIDEO_VK__

#include "SDL.h"

#include "doomtype.h"
#include "i_video.h"

// --- config-exposed options (bound in I_BindVideoVariables, i_video.c) ---

// Master switch (config + -novulkan). Only honored when a preset is set.
extern int usevulkan;

// Path to a .slangp preset (config "shader" + -shader <path>). Setting it
// is what activates the Vulkan path at startup; init falls back to the
// classic renderer on any failure.
extern char *shader_preset;

// Frame limiter (config "vk_fps_cap" + -vkfps <n>): maximum presents per
// second, 0 = off (default). Applied ahead of the swapchain, so it
// composes with any present mode. Caps of 1..35 pace the whole loop
// below the tic rate (game time dilates; nothing is skipped). A cap
// above the 35 Hz tic rate uncouples presentation from game logic in
// single player: the main loop re-presents the current state between
// tics and this limiter paces it at the cap (netgames keep the vanilla
// coupled loop). Vulkan path only; ignored by the classic renderer.
extern int vk_fps_cap;

// Swapchain present mode (config "vk_present_mode" + -vkpm <n>):
// 0 = FIFO (default; vsync, guaranteed on every surface), 1 = MAILBOX
// (triple buffer, queued frames replaced — tear-free without pacing the
// queue), 2 = IMMEDIATE (no pacing, may tear), 3 = FIFO_RELAXED (vsync
// that unblocks when a frame is late). A mode the surface does not
// support falls back to FIFO. Vulkan path only.
extern int vk_present_mode;

// -vkdebug: enable the Vulkan validation layer for this run.
extern int vk_debug;

// -vkcapture <frame>: dump that rendered frame to vkframe.tga (debug aid;
// validates the full present pipeline without screen-grab tooling).
extern int vk_capture_frame;

// Shader base resolution (config "shader_base_resolution" + -vkbase <n>):
// the canvas size handed to the shader chain, DevilutionX-style. The feed
// is nearest-neighbor upscaled by an integer factor before the preset runs,
// giving upscaler / anti-aliasing presets input-to-output headroom. Values:
// 0 = auto (native 1x); 1..4 = the multiplier directly; larger values are
// pixel widths (320xk: 640 = 2x, 960 = 3x, 1280 = 4x). Vulkan path only.
extern int shader_base_resolution;

// --- shared state owned by i_video.c ---
// The VK presenter creates the window straight into `screen` (so the
// classic event pump / title / shutdown paths keep working unmodified)
// and debounces swapchain recreation off the same resize flags that
// HandleWindowEvent maintains for the classic path.
extern SDL_Window *screen;
extern boolean need_resize;
extern unsigned int last_resize_time;

// --- runtime state (i_video.c consults these) ---

// True once the Vulkan path owns the window and presents every frame.
boolean I_VK_Active(void);

// Adapter description (deviceName from the library device info)
// and preset pass count, used by the stats overlay (i_overlay.c).
// Return ""/0 when the Vulkan path is not running (or compiled
// out).
const char *I_VK_DeviceName(void);
int I_VK_PassCount(void);

// Human-readable reason the Vulkan path fell back to the classic
// renderer ("" while it is running, or when it was never requested).
// Shown in the stats overlay's shader row — stderr is invisible in
// Windows GUI builds, which is exactly where fallbacks bite hardest.
const char *I_VK_FallbackReason(void);

// Effective shader-chain input size (the base canvas: feed size x base
// multiplier) and the base multiplier itself, for the stats overlay.
// Native feed size / 1 when the Vulkan path is not running (or compiled
// out).
void I_VK_FeedSize(int *w, int *h);
int I_VK_BaseScale(void);

// --- hooks called from i_video.c ---

// From I_InitGraphics: try to take over (window + device + swapchain +
// preset + chain + I_VideoBuffer). On failure cleans up after itself and
// returns false; the caller falls back to the classic SetVideoMode().
boolean I_VK_AttemptGraphics(void);

// From I_SetPalette: copies the 256-entry palette into the expansion LUT.
void I_VK_SetPalette(const SDL_Color *palette);

// From HandleWindowEvent(SDL_WINDOWEVENT_RESIZED).
void I_VK_FlagResize(void);

// Runtime aspect toggle (Alt+A): cycles the feed 4:3 -> 16:10 -> 16:9 ->
// 4:3 — 320x240 (authentic VGA mode 13h stretch), raw 320x200 square
// pixels, then 356x200 (16:9 widescreen column stretch). Persisted as the
// tri-state aspect_ratio_correct (0 / 1 / 2); the classic renderer only
// understands 0/1 and renders 2 as corrected 4:3.
void I_VK_ToggleAspect(void);

// Current aspect mode name ("4:3" / "16:10" / "16:9") for the stats
// overlay's feed row and the Alt+A feedback flash.
const char *I_VK_AspectName(void);

// Runtime hotkeys (from I_GetEvent, VK path only):
// PrintScreen -> I_VK_RequestCapture: read back the next presented frame
// to the first free vkshotNNN.tga (same channel-correct TGA writer as
// -vkcapture). Alt+B -> I_VK_CycleBase: cycle the shader base multiplier
// 1x -> 2x -> 3x -> 4x -> 1x (persisted through shader_base_resolution;
// the input image re-creates and the chain re-derives on the next frame).
// The Alt+letter hotkeys are eaten only with Alt held and only while the
// Vulkan path is live — plain letters and the vanilla F11/F12 features
// (gamma cycle, spy camera) reach the game untouched.
void I_VK_RequestCapture(void);
void I_VK_CycleBase(void);

// SHADERS browser: hot-swap the active preset for the file at `path`.
// Prepare-then-commit (a failed preset leaves the current shader on
// screen); persists the choice through the "shader" config binding.
// Returns false when the Vulkan path is not live or the swap failed.
boolean I_VK_SetPreset(const char *path);

// From I_FinishUpdate: feed + render + present one frame.
void I_VK_FinishUpdate(void);

// From I_ShutdownGraphics.
void I_VK_ShutdownGraphics(void);

#endif
