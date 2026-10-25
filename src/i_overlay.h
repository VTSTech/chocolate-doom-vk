//
// i_overlay.h — on-screen stats overlay (Chocolate Doom VK).
//
// Copyright (C) 2025 VTSTech (Nigel Todman).
// License: GNU General Public License, version 2 (see COPYING.md).
//
// A compact stats overlay (FPS, graphics backend, active shader) drawn
// into the 320x200 paletted software frame just before it is handed to
// whichever presenter owns the window — the Vulkan shader chain or the
// classic SDL2 renderer — so it works identically on both backends and,
// on the Vulkan path, is itself run through the active preset.
//

#ifndef __I_OVERLAY__
#define __I_OVERLAY__

#include "doomtype.h"

// Config-exposed ("vkstats" in the config file, -novkstats on the command
// line). Non-zero draws the overlay. Scroll Lock toggles it at runtime.
extern int vkstats;

// Toggle the overlay at runtime (Scroll Lock, from I_GetEvent).
void I_OverlayToggle(void);

// Show a short-lived message row in the overlay box (2 seconds) — the
// on-screen feedback for the PrintScreen screenshot and Alt+B base-cycle
// hotkeys, so the action is confirmed without leaving the game.
void I_OverlayFlash(const char *fmt, ...);

// Draw the overlay into I_VideoBuffer. Called from I_FinishUpdate after
// V_DrawDiskIcon(), before the frame reaches either presenter.
void I_OverlayDraw(void);

#endif
