//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      DOOM graphics stuff for SDL.
//


#include <stdlib.h>
#include <string.h>

#include "SDL.h"
#include "SDL_opengl.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_log.h"
#include "i_overlay.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "i_video_vk.h"
#include "i_glob.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

// These are (1) the window (or the full screen) that our game is rendered to
// and (2) the renderer that scales the texture (see below) into this window.

SDL_Window *screen;             // shared: the VK presenter assigns into it too
static SDL_Renderer *renderer;

// Window title

static const char *window_title = "";

// These are (1) the 320x200x8 paletted buffer that we draw to (i.e. the one
// that holds I_VideoBuffer), (2) the 320x200x32 RGBA intermediate buffer that
// we blit the former buffer to, (3) the intermediate 320x200 texture that we
// load the RGBA buffer to and that we render into another texture (4) which
// is upscaled by an integer factor UPSCALE using "nearest" scaling and which
// in turn is finally rendered to screen using "linear" scaling.

static SDL_Surface *screenbuffer = NULL;
static SDL_Surface *argbbuffer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Texture *texture_upscaled = NULL;

static SDL_Rect blit_rect = {
    0,
    0,
    SCREENWIDTH,
    SCREENHEIGHT
};

// palette

static SDL_Color palette[256];
static boolean palette_to_set;

// display has been set up?

static boolean initialized = false;

// disable mouse?

static boolean nomouse = false;
int usemouse = 1;

// Save screenshots in PNG format.

int png_screenshots = 0;

// SDL video driver name

char *video_driver = "";

// Window position:

char *window_position = "center";

// SDL display number on which to run.

int video_display = 0;

// Screen width and height, from configuration file.

int window_width = 800;
int window_height = 600;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.

int fullscreen_width = 0, fullscreen_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

static int max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

int fullscreen = true;

// Aspect ratio correction mode

int aspect_ratio_correct = true;
static int actualheight;

// Smooth pixel scaling
int smooth_pixel_scaling = true;

// Force integer scales for resolution-independent rendering

int integer_scaling = false;

// VGA Porch palette change emulation

int vga_porch_flash = false;

// Force software rendering, for systems which lack effective hardware
// acceleration

int force_software_renderer = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

static int startup_delay = 1000;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

static int grabmouse = true;
static boolean nograbmouse_override = false;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible = true;

// If true, we display dots at the bottom of the screen to 
// indicate FPS.

static boolean display_fps_dots;

// If this is true, the screen is rendered but not blitted to the
// video buffer.

static boolean noblit;

// Callback function to invoke to determine whether to grab the 
// mouse pointer.

static grabmouse_callback_t grabmouse_callback = NULL;

// Does the window currently have focus?

static boolean window_focused = true;

// Window resize state.

boolean need_resize = false;   // shared: i_video_vk.c debounces off these
unsigned int last_resize_time;
#define RESIZE_DELAY 500

// Gamma correction level to use

int usegamma = 0;

// Joystick/gamepad hysteresis
unsigned int joywait = 0;

// Icon RGB data and dimensions
static const unsigned int *icon_data;
static int icon_w;
static int icon_h;

static boolean MouseShouldBeGrabbed()
{
    // never grab the mouse when in screensaver mode
   
    if (screensaver_mode)
        return false;

    // if the window doesn't have focus, never grab it

    if (!window_focused)
        return false;

    // always grab the mouse when full screen (dont want to 
    // see the mouse pointer)

    if (fullscreen)
        return true;

    // Don't grab the mouse if mouse input is disabled

    if (!usemouse || nomouse)
        return false;

    // if we specify not to grab the mouse, never grab

    if (nograbmouse_override || !grabmouse)
        return false;

    // Invoke the grabmouse callback function to determine whether
    // the mouse should be grabbed

    if (grabmouse_callback != NULL)
    {
        return grabmouse_callback();
    }
    else
    {
        return true;
    }
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
    grabmouse_callback = func;
}

// Set the variable controlling FPS dots.

void I_DisplayFPSDots(boolean dots_on)
{
    display_fps_dots = dots_on;
}

static void SetShowCursor(boolean show)
{
    if (!screensaver_mode)
    {
        // When the cursor is hidden, grab the input.
        // Relative mode implicitly hides the cursor.
        SDL_SetRelativeMouseMode(!show);
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        SetShowCursor(true);

        if (I_VK_Active())
        {
            I_VK_ShutdownGraphics();
        }
        else
        {
            SDL_FreeSurface(argbbuffer);
            SDL_FreeSurface(screenbuffer);
            SDL_DestroyTexture(texture_upscaled);
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
        }
        SDL_DestroyWindow(screen);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        // [CDVK] Leave nothing dangling after teardown: SetVideoMode()
        // only creates the window when screen == NULL, and the
        // surface/texture paths re-free through their != NULL guards.
        // Destroyed-but-still-set pointers made a later I_InitGraphics()
        // build a renderer against the dead window (I_Error) and
        // double-free the screen buffers.
        renderer = NULL;
        texture = NULL;
        texture_upscaled = NULL;
        argbbuffer = NULL;
        screenbuffer = NULL;
        screen = NULL;

        initialized = false;
    }
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

// Adjust window_width / window_height variables to be an an aspect
// ratio consistent with the aspect_ratio_correct variable.
static void AdjustWindowSize(void)
{
    if (aspect_ratio_correct || integer_scaling)
    {
        if (window_width * actualheight <= window_height * SCREENWIDTH)
        {
            // We round up window_height if the ratio is not exact; this leaves
            // the result stable.
            window_height = (window_width * actualheight + SCREENWIDTH - 1) / SCREENWIDTH;
        }
        else
        {
            window_width = window_height * SCREENWIDTH / actualheight;
        }
    }
}

static void HandleWindowEvent(SDL_WindowEvent *event)
{
    int i;

    switch (event->event)
    {
#if 0 // SDL2-TODO
        case SDL_ACTIVEEVENT:
            // need to update our focus state
            UpdateFocus();
            break;
#endif
        case SDL_WINDOWEVENT_EXPOSED:
            palette_to_set = true;
            break;

        case SDL_WINDOWEVENT_RESIZED:
            need_resize = true;
            last_resize_time = SDL_GetTicks();
            break;

        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;

        // We want to save the user's preferred monitor to use for running the
        // game, so that next time we're run we start on the same display. So
        // every time the window is moved, find which display we're now on and
        // update the video_display config variable.

        case SDL_WINDOWEVENT_MOVED:
            i = SDL_GetWindowDisplayIndex(screen);
            if (i >= 0)
            {
                video_display = i;
            }
            break;

        default:
            break;
    }
}

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym->scancode == SDL_SCANCODE_RETURN || 
            sym->scancode == SDL_SCANCODE_KP_ENTER) && (sym->mod & flags) != 0;
}

static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    // TODO: Consider implementing fullscreen toggle for SDL_WINDOW_FULLSCREEN
    // (mode-changing) setup. This is hard because we have to shut down and
    // restart again.
    if (fullscreen_width != 0 || fullscreen_height != 0)
    {
        return;
    }

    fullscreen = !fullscreen;

    if (fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        AdjustWindowSize();
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}

void I_GetEvent(void)
{
    SDL_Event sdlevent;

    SDL_PumpEvents();

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (sdlevent.key.keysym.scancode == SDL_SCANCODE_SCROLLLOCK)
                {
                    I_OverlayToggle();
                    break;
                }
                if (I_VK_Active()
                    && sdlevent.key.keysym.scancode == SDL_SCANCODE_A
                    && (sdlevent.key.keysym.mod & KMOD_ALT) != 0)
                {
                    // Alt+A cycles the feed aspect. The letter is eaten
                    // only with Alt held (the Alt+Enter convention) and
                    // only while the Vulkan path is live — plain A and
                    // every vanilla binding (F11 gamma, F12 spy) reach
                    // the game untouched.
                    I_VK_ToggleAspect();
                    break;
                }
                if (I_VK_Active()
                    && sdlevent.key.keysym.scancode == SDL_SCANCODE_PRINTSCREEN)
                {
                    I_VK_RequestCapture();
                    break;
                }
                if (I_VK_Active()
                    && sdlevent.key.keysym.scancode == SDL_SCANCODE_B
                    && (sdlevent.key.keysym.mod & KMOD_ALT) != 0)
                {
                    // Alt+B cycles the shader base resolution (same
                    // gating as Alt+A above; vanilla F12 spy camera is
                    // left alone).
                    I_VK_CycleBase();
                    break;
                }
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
                I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (usemouse && !nomouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
                if (screensaver_mode)
                {
                    I_Quit();
                }
                else
                {
                    event_t event;
                    event.type = ev_quit;
                    D_PostEvent(&event);
                }
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
    }
}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();

    if (usemouse && !nomouse && window_focused)
    {
        I_ReadMouse();
    }

    if (joywait < I_GetTime())
    {
        I_UpdateJoystick();
    }
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

static void UpdateGrab(void)
{
    static boolean currently_grabbed = false;
    boolean grab;

    grab = MouseShouldBeGrabbed();

    if (screensaver_mode)
    {
        // Hide the cursor in screensaver mode

        SetShowCursor(false);
    }
    else if (grab && !currently_grabbed)
    {
        SetShowCursor(false);
    }
    else if (!grab && currently_grabbed)
    {
        int screen_w, screen_h;

        SetShowCursor(true);

        // When releasing the mouse from grab, warp the mouse cursor to
        // the bottom-right of the screen. This is a minimally distracting
        // place for it to appear - we may only have released the grab
        // because we're at an end of level intermission screen, for
        // example.

        SDL_GetWindowSize(screen, &screen_w, &screen_h);
        SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
        SDL_GetRelativeMouseState(NULL, NULL);
    }

    currently_grabbed = grab;
}

static void LimitTextureSize(int *w_upscale, int *h_upscale)
{
    SDL_RendererInfo rinfo;
    int orig_w, orig_h;

    orig_w = *w_upscale;
    orig_h = *h_upscale;

    // Query renderer and limit to maximum texture dimensions of hardware:
    if (SDL_GetRendererInfo(renderer, &rinfo) != 0)
    {
        I_Error("CreateUpscaledTexture: SDL_GetRendererInfo() call failed: %s",
                SDL_GetError());
    }

    while (*w_upscale * SCREENWIDTH > rinfo.max_texture_width)
    {
        --*w_upscale;
    }
    while (*h_upscale * SCREENHEIGHT > rinfo.max_texture_height)
    {
        --*h_upscale;
    }

    if ((*w_upscale < 1 && rinfo.max_texture_width > 0) ||
        (*h_upscale < 1 && rinfo.max_texture_height > 0))
    {
        I_Error("CreateUpscaledTexture: Can't create a texture big enough for "
                "the whole screen! Maximum texture size %dx%d",
                rinfo.max_texture_width, rinfo.max_texture_height);
    }

    // We limit the amount of texture memory used for the intermediate buffer,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures, so the user can use this to reduce the maximum texture
    // size if desired.

    if (max_scaling_buffer_pixels < SCREENWIDTH * SCREENHEIGHT)
    {
        I_Error("CreateUpscaledTexture: max_scaling_buffer_pixels too small "
                "to create a texture buffer: %d < %d",
                max_scaling_buffer_pixels, SCREENWIDTH * SCREENHEIGHT);
    }

    while (*w_upscale * *h_upscale * SCREENWIDTH * SCREENHEIGHT
           > max_scaling_buffer_pixels)
    {
        if (*w_upscale > *h_upscale)
        {
            --*w_upscale;
        }
        else
        {
            --*h_upscale;
        }
    }

    if (*w_upscale != orig_w || *h_upscale != orig_h)
    {
        printf("CreateUpscaledTexture: Limited texture size to %dx%d "
               "(max %d pixels, max texture size %dx%d)\n",
               *w_upscale * SCREENWIDTH, *h_upscale * SCREENHEIGHT,
               max_scaling_buffer_pixels,
               rinfo.max_texture_width, rinfo.max_texture_height);
    }
}

static void CreateUpscaledTexture(boolean force)
{
    int w, h;
    int h_upscale, w_upscale;
    static int h_upscale_old, w_upscale_old;

    SDL_Texture *new_texture, *old_texture;

    // Get the size of the renderer output. The units this gives us will be
    // real world pixels, which are not necessarily equivalent to the screen's
    // window size (because of highdpi).
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0)
    {
        I_Error("Failed to get renderer output size: %s", SDL_GetError());
    }

    // When the screen or window dimensions do not match the aspect ratio
    // of the texture, the rendered area is scaled down to fit. Calculate
    // the actual dimensions of the rendered area.

    if (w * actualheight < h * SCREENWIDTH)
    {
        // Tall window.

        h = w * actualheight / SCREENWIDTH;
    }
    else
    {
        // Wide window.

        w = h * SCREENWIDTH / actualheight;
    }

    // Pick texture size the next integer multiple of the screen dimensions.
    // If one screen dimension matches an integer multiple of the original
    // resolution, there is no need to overscale in this direction.

    w_upscale = (w + SCREENWIDTH - 1) / SCREENWIDTH;
    h_upscale = (h + SCREENHEIGHT - 1) / SCREENHEIGHT;

    // Minimum texture dimensions of 320x200.

    if (w_upscale < 1)
    {
        w_upscale = 1;
    }
    if (h_upscale < 1)
    {
        h_upscale = 1;
    }

    LimitTextureSize(&w_upscale, &h_upscale);

    // Create a new texture only if the upscale factors have actually changed.

    if (h_upscale == h_upscale_old && w_upscale == w_upscale_old && !force)
    {
        return;
    }

    h_upscale_old = h_upscale;
    w_upscale_old = w_upscale;

    // Set the scaling quality for rendering the upscaled texture to "linear",
    // which looks much softer and smoother than "nearest" but does a better
    // job at downscaling from the upscaled texture to screen.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    new_texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_TARGET,
                                w_upscale*SCREENWIDTH,
                                h_upscale*SCREENHEIGHT);

    old_texture = texture_upscaled;
    texture_upscaled = new_texture;

    if (old_texture != NULL)
    {
        SDL_DestroyTexture(old_texture);
    }
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    static int lasttic;
    int tics;
    int i;

    if (!initialized)
        return;

    if (noblit)
        return;

    if (need_resize && !I_VK_Active())
    {
        if (SDL_GetTicks() > last_resize_time + RESIZE_DELAY)
        {
            int flags;
            // When the window is resized (we're not in fullscreen mode),
            // save the new window size.
            flags = SDL_GetWindowFlags(screen);
            if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
            {
                SDL_GetWindowSize(screen, &window_width, &window_height);

                // Adjust the window by resizing again so that the window
                // is the right aspect ratio.
                AdjustWindowSize();
                SDL_SetWindowSize(screen, window_width, window_height);
            }
            CreateUpscaledTexture(false);
            need_resize = false;
            palette_to_set = true;
        }
        else
        {
            return;
        }
    }

    UpdateGrab();

#if 0 // SDL2-TODO
    // Don't update the screen if the window isn't visible.
    // Not doing this breaks under Windows when we alt-tab away 
    // while fullscreen.

    if (!(SDL_GetAppState() & SDL_APPACTIVE))
        return;
#endif

    // draws little dots on the bottom of the screen

    if (display_fps_dots)
    {
        i = I_GetTime();
        tics = i - lasttic;
        lasttic = i;
        if (tics > 20) tics = 20;

        for (i=0 ; i<tics*4 ; i+=4)
            I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
        for ( ; i<20*4 ; i+=4)
            I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }

    // Draw disk icon before blit, if necessary.
    V_DrawDiskIcon();

    I_OverlayDraw();

    if (I_VK_Active())
    {
        I_VK_FinishUpdate();
        V_RestoreDiskBackground();
        return;
    }

    if (palette_to_set)
    {
        SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
        palette_to_set = false;

        if (vga_porch_flash)
        {
            // "flash" the pillars/letterboxes with palette changes, emulating
            // VGA "porch" behaviour (GitHub issue #832)
            SDL_SetRenderDrawColor(renderer, palette[0].r, palette[0].g,
                palette[0].b, SDL_ALPHA_OPAQUE);
        }
    }

    // Blit from the paletted 8-bit screen buffer to the intermediate
    // 32-bit RGBA buffer and update the intermediate texture with the
    // contents of the RGBA buffer.

    SDL_LockTexture(texture, &blit_rect, &argbbuffer->pixels,
                    &argbbuffer->pitch);
    SDL_LowerBlit(screenbuffer, &blit_rect, argbbuffer, &blit_rect);
    SDL_UnlockTexture(texture);

    // Make sure the pillarboxes are kept clear each frame.

    SDL_RenderClear(renderer);

    if (smooth_pixel_scaling && !force_software_renderer)
    {
        // Render this intermediate texture into the upscaled texture
        // using "nearest" integer scaling.
        SDL_SetRenderTarget(renderer, texture_upscaled);
        SDL_RenderCopy(renderer, texture, NULL, NULL);

        // Finally, render this upscaled texture to screen using linear scaling.

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, texture_upscaled, NULL, NULL);
    }
    else
    {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
    }


    // Draw!

    SDL_RenderPresent(renderer);

    // Restore background and undo the disk indicator, if it was drawn.
    V_RestoreDiskBackground();
}


//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH*SCREENHEIGHT*sizeof(*scr));
}


//
// I_SetPalette
//
void I_SetPalette (byte *doompalette)
{
    int i;

    for (i=0; i<256; ++i)
    {
        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.

        palette[i].a = 0xFFU;
        palette[i].r = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].g = gammatable[usegamma][*doompalette++] & ~3;
        palette[i].b = gammatable[usegamma][*doompalette++] & ~3;
    }

    palette_to_set = true;

    if (I_VK_Active())
    {
        I_VK_SetPalette(palette);
    }
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
    int best, best_diff, diff;
    int i;

    best = 0; best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        diff = (r - palette[i].r) * (r - palette[i].r)
             + (g - palette[i].g) * (g - palette[i].g)
             + (b - palette[i].b) * (b - palette[i].b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

// 
// Set the window title
//

void I_SetWindowTitle(const char *title)
{
    window_title = title;
}

//
// Call the SDL function to set the window title, based on 
// the title set with I_SetWindowTitle.
//

void I_InitWindowTitle(void)
{
    char *buf;

    buf = M_StringJoin(window_title, " - ", PACKAGE_STRING,
                       I_VK_Active() ? " [VK]" : "", NULL);
    SDL_SetWindowTitle(screen, buf);
    free(buf);
}

void I_RegisterWindowIcon(const unsigned int *icon, int width, int height)
{
    icon_data = icon;
    icon_w = width;
    icon_h = height;
}

// Set the application icon

void I_InitWindowIcon(void)
{
    SDL_Surface *surface;

    surface = SDL_CreateRGBSurfaceFrom((void *) icon_data, icon_w, icon_h,
                                       32, icon_w * 4,
                                       0xffu << 24, 0xffu << 16,
                                       0xffu << 8, 0xffu << 0);

    SDL_SetWindowIcon(screen, surface);
    SDL_FreeSurface(surface);
}

// Set video size to a particular scale factor (1x, 2x, 3x, etc.)

static void SetScaleFactor(int factor)
{
    int height;

    // Pick 320x200 or 320x240, depending on aspect ratio correct
    if (aspect_ratio_correct)
    {
        height = SCREENHEIGHT_4_3;
    }
    else
    {
        height = SCREENHEIGHT;
    }

    window_width = factor * SCREENWIDTH;
    window_height = factor * height;
    fullscreen = false;
}

void I_GraphicsCheckCommandLine(void)
{
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    noblit = M_CheckParm ("-noblit");

    //!
    // @category video 
    //
    // Don't grab the mouse when running in windowed mode.
    //

    nograbmouse_override = M_ParmExists("-nograbmouse");

    // default to fullscreen mode, allow override with command line
    // nofullscreen because we love prboom

    //!
    // @category video 
    //
    // Run in a window.
    //

    if (M_CheckParm("-window") || M_CheckParm("-nofullscreen"))
    {
        fullscreen = false;
    }

    //!
    // @category video 
    //
    // Run in fullscreen mode.
    //

    if (M_CheckParm("-fullscreen"))
    {
        fullscreen = true;
    }

    //!
    // @category video 
    //
    // Disable the mouse.
    //

    nomouse = M_CheckParm("-nomouse") > 0;

    //!
    // @category video
    // @arg <W>
    //
    // Specify the screen width, in pixels.  Implies -window.
    //

    i = M_CheckParmWithArgs("-width", 1);

    if (i > 0)
    {
        window_width = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <H>
    //
    // Specify the screen height, in pixels.  Implies -window.
    //

    i = M_CheckParmWithArgs("-height", 1);

    if (i > 0)
    {
        window_height = atoi(myargv[i + 1]);
        fullscreen = false;
    }

    //!
    // @category video
    // @arg <WxH>
    //
    // Specify the dimensions of the window.  Implies -window.
    //

    i = M_CheckParmWithArgs("-geometry", 1);

    if (i > 0)
    {
        int w, h, s;

        s = sscanf(myargv[i + 1], "%ix%i", &w, &h);
        if (s == 2)
        {
            window_width = w;
            window_height = h;
            fullscreen = false;
        }
    }

    //!
    // @category video
    // @arg <x>
    //
    // Specify the display number on which to show the screen.
    //

    i = M_CheckParmWithArgs("-display", 1);

    if (i > 0)
    {
        int display = atoi(myargv[i + 1]);
        if (display >= 0)
        {
            video_display = display;
        }
    }

    //!
    // @category video
    //
    // Don't scale up the screen.  Implies -window.
    //

    if (M_CheckParm("-1")) 
    {
        SetScaleFactor(1);
    }

    //!
    // @category video
    //
    // Double up the screen to 2x its normal size.  Implies -window.
    //

    if (M_CheckParm("-2")) 
    {
        SetScaleFactor(2);
    }

    //!
    // @category video
    //
    // Double up the screen to 3x its normal size.  Implies -window.
    //

    if (M_CheckParm("-3")) 
    {
        SetScaleFactor(3);
    }

    //!
    // @category video
    // @arg <preset>
    //
    // Run the game's frames through a libretro slang shader preset
    // (.slangp) using Vulkan (libslang-vk). Falls back to the classic
    // renderer if Vulkan or the preset is unavailable.
    //

    i = M_CheckParmWithArgs("-shader", 1);
    if (i > 0)
    {
        shader_preset = myargv[i + 1];
        // Asking for a shader on the command line is explicit intent:
        // re-enable the Vulkan path even if an earlier -novulkan run
        // persisted usevulkan = 0 in the config. (-novulkan below still
        // wins when both are given, so forcing the classic renderer
        // stays possible.)
        usevulkan = 1;
    }

    //!
    // @category video
    //
    // Disable the Vulkan shader path for this run (classic renderer).
    //

    if (M_ParmExists("-novulkan"))
    {
        usevulkan = 0;
    }

    //!
    // @category video
    //
    // Enable the Vulkan validation layer for this run.
    //

    if (M_ParmExists("-vkdebug"))
    {
        vk_debug = 1;
    }

    //!
    // @category video
    //
    // Feed the shader chain raw 320x200 pixels instead of the authentic
    // 4:3 (320x240) stretch.
    //

    if (M_ParmExists("-noaspect"))
    {
        aspect_ratio_correct = 0;
    }

    //!
    // @category video
    // @arg <n>
    //
    // Shader base resolution (DevilutionX-style): integer multiplier
    // for the canvas fed to the shader chain. 1 = native 320x240 (or
    // 320x200 raw); 2, 3, 4 = doubled, tripled, quadrupled (640x480,
    // 960x720, 1280x960). A pixel width (320..1280) is also accepted.
    // Vulkan path only; ignored by the classic renderer.
    //

    i = M_CheckParmWithArgs("-vkbase", 1);
    if (i > 0)
    {
        shader_base_resolution = atoi(myargv[i + 1]);
    }

    //!
    // @category video
    // @arg <frame>
    //
    // Capture that presented frame to vkframe.tga (Vulkan path debug aid).
    //

    i = M_CheckParmWithArgs("-vkcapture", 1);
    if (i > 0)
    {
        vk_capture_frame = atoi(myargv[i + 1]);
    }

    //!
    // @category video
    // @arg <fps>
    //
    // Cap the Vulkan presenter at this many presents per second
    // (0 = off, the default). Applied ahead of the swapchain, so it
    // also throttles MAILBOX and IMMEDIATE present modes. Caps of
    // 1..35 pace the whole loop below the tic rate (game time dilates,
    // nothing is skipped). A cap above the tic rate (~35 fps) uncouples
    // presentation from game logic in single player: up to <fps>
    // presents per second while the game keeps running at 35 Hz
    // (e.g. -vkfps 60; pair with -vkpm 1 for lowest latency). Low caps
    // trade responsiveness for GPU load / battery (e.g. -vkfps 30).
    // Vulkan path only.
    //

    i = M_CheckParmWithArgs("-vkfps", 1);
    if (i > 0)
    {
        vk_fps_cap = atoi(myargv[i + 1]);
        if (vk_fps_cap < 0)
        {
            vk_fps_cap = 0;
        }
    }

    //!
    // @category video
    // @arg <mode>
    //
    // Vulkan swapchain present mode: 0 = FIFO (default, vsync),
    // 1 = MAILBOX (triple buffered, queued frames replaced — pairs
    // with -vkfps), 2 = IMMEDIATE (no pacing, may tear), 3 =
    // FIFO_RELAXED (vsync except when late). Modes a surface does not
    // support fall back to FIFO. Vulkan path only.
    //

    i = M_CheckParmWithArgs("-vkpm", 1);
    if (i > 0)
    {
        vk_present_mode = atoi(myargv[i + 1]);
        if (vk_present_mode < 0 || vk_present_mode > 3)
        {
            vk_present_mode = 0;
        }
    }

    //!
    // @category video
    //
    // Disable the on-screen stats overlay (FPS, graphics
    // backend, active shader) for this run. Scroll Lock
    // toggles it at runtime.
    //

    if (M_ParmExists("-novkstats"))
    {
        vkstats = 0;
    }
}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
    char *env;

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        screensaver_mode = true;
    }
}

static void SetSDLVideoDriver(void)
{
    // Allow a default value for the SDL video driver to be specified
    // in the configuration file.

    if (strcmp(video_driver, "") != 0)
    {
        char *env_string;

        env_string = M_StringJoin("SDL_VIDEODRIVER=", video_driver, NULL);
        putenv(env_string);
        free(env_string);
    }
}

// Check the display bounds of the display referred to by 'video_display' and
// set x and y to a location that places the window in the center of that
// display.
static void CenterWindow(int *x, int *y, int w, int h)
{
    SDL_Rect bounds;

    if (SDL_GetDisplayBounds(video_display, &bounds) < 0)
    {
        fprintf(stderr, "CenterWindow: Failed to read display bounds "
                        "for display #%d!\n", video_display);
        return;
    }

    *x = bounds.x + SDL_max((bounds.w - w) / 2, 0);
    *y = bounds.y + SDL_max((bounds.h - h) / 2, 0);
}

void I_GetWindowPosition(int *x, int *y, int w, int h)
{
    // Check that video_display corresponds to a display that really exists,
    // and if it doesn't, reset it.
    if (video_display < 0 || video_display >= SDL_GetNumVideoDisplays())
    {
        fprintf(stderr,
                "I_GetWindowPosition: We were configured to run on display #%d, "
                "but it no longer exists (max %d). Moving to display 0.\n",
                video_display, SDL_GetNumVideoDisplays() - 1);
        video_display = 0;
    }

    // in fullscreen mode, the window "position" still matters, because
    // we use it to control which display we run fullscreen on.

    if (fullscreen)
    {
        CenterWindow(x, y, w, h);
        return;
    }

    // in windowed mode, the desired window position can be specified
    // in the configuration file.

    if (window_position == NULL || !strcmp(window_position, ""))
    {
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
    else if (!strcmp(window_position, "center"))
    {
        // Note: SDL has a SDL_WINDOWPOS_CENTER, but this is useless for our
        // purposes, since we also want to control which display we appear on.
        // So we have to do this ourselves.
        CenterWindow(x, y, w, h);
    }
    else if (sscanf(window_position, "%i,%i", x, y) != 2)
    {
        // invalid format: revert to default
        fprintf(stderr, "I_GetWindowPosition: invalid window_position setting\n");
        *x = *y = SDL_WINDOWPOS_UNDEFINED;
    }
}

static void SetVideoMode(void)
{
    int w, h;
    int x, y;
    int window_flags = 0, renderer_flags = 0;
    SDL_DisplayMode mode;

    w = window_width;
    h = window_height;

    // In windowed mode, the window can be resized while the game is
    // running.
    window_flags = SDL_WINDOW_RESIZABLE;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    window_flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    if (fullscreen)
    {
        if (fullscreen_width == 0 && fullscreen_height == 0)
        {
            // This window_flags means "Never change the screen resolution!
            // Instead, draw to the entire screen by scaling the texture
            // appropriately".
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
            w = fullscreen_width;
            h = fullscreen_height;
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }
    }

    // Running without window decorations is potentially useful if you're
    // playing in three window mode and want to line up three game windows
    // next to each other on a single desktop.
    // Deliberately not documented because I'm not sure how useful this is yet.
    if (M_ParmExists("-borderless"))
    {
        window_flags |= SDL_WINDOW_BORDERLESS;
    }

    I_GetWindowPosition(&x, &y, w, h);

    // Create window and renderer contexts. We set the window title
    // later anyway and leave the window position "undefined". If
    // "window_flags" contains the fullscreen flag (see above), then
    // w and h are ignored.

    if (screen == NULL)
    {
        screen = SDL_CreateWindow(NULL, x, y, w, h, window_flags);

        if (screen == NULL)
        {
            I_Error("Error creating window for video startup: %s",
            SDL_GetError());
        }

        SDL_SetWindowMinimumSize(screen, SCREENWIDTH, actualheight);

        I_InitWindowTitle();
        I_InitWindowIcon();
    }

    // The SDL_RENDERER_TARGETTEXTURE flag is required to render the
    // intermediate texture into the upscaled texture.
    renderer_flags = SDL_RENDERER_TARGETTEXTURE;
        
    if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
    {
        I_Error("Could not get display mode for video display #%d: %s",
        video_display, SDL_GetError());
    }

    // Turn on vsync if we aren't in a -timedemo
    if (!singletics && mode.refresh_rate > 0)
    {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    if (force_software_renderer)
    {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;
    }

    if (renderer != NULL)
    {
        SDL_DestroyRenderer(renderer);
        // all associated textures get destroyed
        texture = NULL;
        texture_upscaled = NULL;
    }

    renderer = SDL_CreateRenderer(screen, -1, renderer_flags);

    // If we could not find a matching render driver,
    // try again without hardware acceleration.

    if (renderer == NULL && !force_software_renderer)
    {
        renderer_flags |= SDL_RENDERER_SOFTWARE;
        renderer_flags &= ~SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(screen, -1, renderer_flags);

        // If this helped, save the setting for later.
        if (renderer != NULL)
        {
            force_software_renderer = 1;
        }
    }

    if (renderer == NULL)
    {
        I_Error("Error creating renderer for screen window: %s",
                SDL_GetError());
    }

    // Important: Set the "logical size" of the rendering context. At the same
    // time this also defines the aspect ratio that is preserved while scaling
    // and stretching the texture into the window.

    if (aspect_ratio_correct || integer_scaling)
    {
        SDL_RenderSetLogicalSize(renderer,
                                 SCREENWIDTH,
                                 actualheight);
    }

    // Force integer scales for resolution-independent rendering.

    SDL_RenderSetIntegerScale(renderer, integer_scaling);

    // Blank out the full screen area in case there is any junk in
    // the borders that won't otherwise be overwritten.

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    // Create the 8-bit paletted and the 32-bit RGBA screenbuffer surfaces.

    if (screenbuffer != NULL)
    {
        SDL_FreeSurface(screenbuffer);
        screenbuffer = NULL;
    }

    if (screenbuffer == NULL)
    {
        screenbuffer = SDL_CreateRGBSurface(0,
                                            SCREENWIDTH, SCREENHEIGHT, 8,
                                            0, 0, 0, 0);
        SDL_FillRect(screenbuffer, NULL, 0);
    }

    // Format of argbbuffer must match the screen pixel format because we
    // import the surface data into the texture.

    if (argbbuffer != NULL)
    {
        SDL_FreeSurface(argbbuffer);
        argbbuffer = NULL;
    }

    if (argbbuffer == NULL)
    {
            // pixels and pitch will be filled with the texture's values
            // in I_FinishUpdate()
            argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
                     NULL, w, h, 0, 0, SDL_PIXELFORMAT_ARGB8888);
    }

    if (texture != NULL)
    {
        SDL_DestroyTexture(texture);
    }

    // Set the scaling quality for rendering the intermediate texture into
    // the upscaled texture to "nearest", which is gritty and pixelated and
    // resembles software scaling pretty well.

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    // Create the intermediate texture that the RGBA surface gets loaded into.
    // The SDL_TEXTUREACCESS_STREAMING flag means that this texture's content
    // is going to change frequently.

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                SCREENWIDTH, SCREENHEIGHT);

    // Workaround for SDL 2.0.14+ alt-tab bug (taken from Doom Retro via Prboom-plus and Woof)
#if defined(_WIN32)
    {
        SDL_version ver;
        SDL_GetVersion(&ver);
        if (ver.major == 2 && ver.minor == 0 && (ver.patch == 14 || ver.patch == 16))
        {
           SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "1", SDL_HINT_OVERRIDE);
        }
    }
#endif

    // Initially create the upscaled texture for rendering to screen

    CreateUpscaledTexture(true);
}

void I_InitGraphics(void)
{
    SDL_Event dummy;
    byte *doompal;
    char *env;

    // Pass through the XSCREENSAVER_WINDOW environment variable to 
    // SDL_WINDOWID, to embed the SDL window into the Xscreensaver
    // window.

    env = getenv("XSCREENSAVER_WINDOW");

    if (env != NULL)
    {
        char winenv[30];
        unsigned int winid;

        sscanf(env, "0x%x", &winid);
        M_snprintf(winenv, sizeof(winenv), "SDL_WINDOWID=%u", winid);

        putenv(winenv);
    }

    SetSDLVideoDriver();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) 
    {
        I_Error("Failed to initialize video: %s", SDL_GetError());
    }

    // When in screensaver mode, run full screen and auto detect
    // screen dimensions (don't change video mode)
    if (screensaver_mode)
    {
        fullscreen = true;
    }

    // aspect_ratio_correct is tri-state on the Vulkan presenter (Alt+A:
    // 1 = 4:3, 0 = 16:10 raw, 2 = 16:9 widescreen). The classic renderer
    // only understands 0/1, so any nonzero value — including a persisted
    // 2 from a Vulkan run — renders as corrected 4:3 here.
    if (aspect_ratio_correct)
    {
        actualheight = SCREENHEIGHT_4_3;
    }
    else
    {
        actualheight = SCREENHEIGHT;
    }

    // Create the game window; this may switch graphic modes depending
    // on configuration.
    AdjustWindowSize();
    if (!I_VK_AttemptGraphics())
    {
        // A shader was requested but the Vulkan path did not start —
        // say why (gpu-scaler.log carries the same line plus the
        // detailed init-step errors; the same reason is mirrored in
        // the stats overlay's shader row).
        if (shader_preset != NULL && shader_preset[0] != '\0')
        {
            I_LogPrintf("I_VK: Vulkan path inactive (%s) — classic renderer\n",
                   I_VK_FallbackReason());
        }
        SetVideoMode();
    }

    // Start with a clear black screen
    // (screen will be flipped after we set the palette)

    if (!I_VK_Active())
    {
        SDL_FillRect(screenbuffer, NULL, 0);
    }

    // Set the palette

    doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    I_SetPalette(doompal);
    if (!I_VK_Active())
    {
        SDL_SetPaletteColors(screenbuffer->format->palette, palette, 0, 256);
    }

    // SDL2-TODO UpdateFocus();
    UpdateGrab();

    // On some systems, it takes a second or so for the screen to settle
    // after changing modes.  We include the option to add a delay when
    // setting the screen mode, so that the game doesn't start immediately
    // with the player unable to see anything.

    if (fullscreen && !screensaver_mode)
    {
        SDL_Delay(startup_delay);
    }

    // The actual 320x200 canvas that we draw to. This is the pixel buffer of
    // the 8-bit paletted screen buffer that gets blit on an intermediate
    // 32-bit RGBA screen buffer that gets loaded into a texture that gets
    // finally rendered into our window or full screen in I_FinishUpdate().

    if (!I_VK_Active())
    {
        I_VideoBuffer = screenbuffer->pixels;
    }
    V_RestoreBuffer();

    // Clear the screen to black.

    memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT * sizeof(*I_VideoBuffer));

    // clear out any events waiting at the start and center the mouse
  
    while (SDL_PollEvent(&dummy));

    initialized = true;

    // Call I_ShutdownGraphics on quit

    I_AtExit(I_ShutdownGraphics, true);
}


// ------------------------------------------------------------------
// [CDVK] SHADERS browser support. Each game's menu lists one folder's
// worth of rows from I_ShaderBrowse() and navigates with
// I_ShaderBrowseGoto(); a preset pick goes to I_SelectShaderPreset().
// The browser starts in the active preset's own folder (its siblings
// are the most likely next pick), else the drop-in "shaders" folder
// beside the executable or in the config dir - mirroring where
// gpu-scaler.log and shader-cache live. Subfolders appear as rows and
// a literal ".." row climbs back up, so a preset pack laid out like
// the slang-shaders repo (crt/, handheld/, ...) is browsable in-game.
// ------------------------------------------------------------------

#define I_SHADER_BROWSE_MAX 128
#define I_SHADER_DIR_MAX    512

// The browser's current folder ("" = unset; the next I_ShaderBrowse
// picks the default). Full paths only - never relative.
static char shader_browse_dir[I_SHADER_DIR_MAX];

// The current folder's rows, rebuilt by every I_ShaderBrowse call.
// Strings are M_StringDuplicate/M_StringJoin'd; freed on the next scan.
static const char *shader_browse[I_SHADER_BROWSE_MAX];
static int shader_browse_n = 0;

// True when dir can be opened at all (opendir succeeds) - used to skip
// nonexistent default-candidate folders.
static boolean ShaderDirOpenable(const char *dir)
{
    glob_t *glob = I_StartGlob(dir, "*", GLOB_FLAG_NOCASE);

    if (glob == NULL)
    {
        return false;
    }
    I_EndGlob(glob);
    return true;
}

// Pick the browser's default folder (I_ShaderBrowseReset cleared the
// current one). Priority: the active preset's own folder, then the
// drop-in shaders folders, then just the exe/config dirs so a not-yet-
// created shaders/ at least shows up as a descendable row.
static void ShaderBrowseDefault(void)
{
    char buf[I_SHADER_DIR_MAX];
    char *dir = M_DirName(shader_preset != NULL ? shader_preset : "");

    if (dir != NULL && dir[0] != '\0' && strcmp(dir, ".") != 0
        && ShaderDirOpenable(dir))
    {
        M_snprintf(shader_browse_dir, sizeof(shader_browse_dir), "%s", dir);
        free(dir);
        return;
    }
    free(dir);

    // <exedir>/shaders - exedir already ends in a separator.
    if (exedir != NULL)
    {
        M_snprintf(buf, sizeof(buf), "%sshaders", exedir);
        if (ShaderDirOpenable(buf))
        {
            M_snprintf(shader_browse_dir, sizeof(shader_browse_dir),
                       "%s", buf);
            return;
        }
        M_snprintf(shader_browse_dir, sizeof(shader_browse_dir), "%s",
                   exedir);
        return;
    }

    // <configdir>/shaders - configdir "conventionally" ends in a
    // separator (M_SetConfigDir); add one when it doesn't.
    if (configdir != NULL && configdir[0] != '\0')
    {
        int n = (int) strlen(configdir);

        if (configdir[n-1] == '/' || configdir[n-1] == '\\')
        {
            M_snprintf(buf, sizeof(buf), "%sshaders", configdir);
        }
        else
        {
            M_snprintf(buf, sizeof(buf), "%s%cshaders", configdir,
                       DIR_SEPARATOR);
        }
        if (ShaderDirOpenable(buf))
        {
            M_snprintf(shader_browse_dir, sizeof(shader_browse_dir),
                       "%s", buf);
            return;
        }
        M_snprintf(shader_browse_dir, sizeof(shader_browse_dir), "%s",
                   configdir);
    }
}

void I_ShaderBrowseReset(void)
{
    shader_browse_dir[0] = '\0';
}

// The folder one ".." above dir, or NULL when dir is a filesystem root
// (nothing left to climb). M_DirName has sharp edges this smooths over:
// a top-level POSIX folder ("M_DirName("/usr")" -> "") still climbs to
// "/", a Windows drive root stays put ("C:" alone means the drive's
// current directory, and "C:\" has no parent), and a bare name's "."
// means no climb either.
static char *ShaderDirParent(const char *dir)
{
    char *parent = M_DirName(dir);

    if (parent == NULL)
    {
        return NULL;
    }
#ifdef _WIN32
    if (strlen(parent) == 2 && parent[1] == ':')
    {
        char *root = M_StringJoin(parent, DIR_SEPARATOR_S, NULL);

        free(parent);
        parent = root;
    }
#endif
    if (parent[0] == '\0' || strcmp(parent, ".") == 0
        || strcmp(parent, dir) == 0)
    {
        free(parent);
        if ((dir[0] == '/' || dir[0] == '\\') && strlen(dir) > 1)
        {
            return M_StringDuplicate("/");
        }
        return NULL;
    }
    return parent;
}

// Folder rows carry a trailing DIR_SEPARATOR (I_ShaderEntryIsDir's
// marker); presets are plain full paths.
boolean I_ShaderEntryIsDir(const char *entry)
{
    size_t len;

    if (entry == NULL)
    {
        return false;
    }
    len = strlen(entry);
    return len > 0 && (entry[len - 1] == '/' || entry[len - 1] == '\\');
}

// Append one row (bounds-checked); drops rows past the cap. A preset
// pack root can hold more folders than the cap when someone browses
// the slang-shaders checkout itself; the window just shows fewer.
static void ShaderBrowseAppend(char *row)
{
    if (shader_browse_n < I_SHADER_BROWSE_MAX)
    {
        shader_browse[shader_browse_n++] = row;
    }
    else
    {
        free(row);
    }
}

const char **I_ShaderBrowse(int *count)
{
    glob_t *glob;
    const char *path;
    char *parent;
    int n;

    // Re-scan on every call (a few readdirs). The previous rows are
    // freed first.
    for (n = 0; n < shader_browse_n; ++n)
    {
        free((char *) shader_browse[n]);
        shader_browse[n] = NULL;
    }
    shader_browse_n = 0;

    if (shader_browse_dir[0] == '\0')
    {
        ShaderBrowseDefault();
    }

    // 1) The ".." row, whenever this folder has a parent to climb to.
    parent = ShaderDirParent(shader_browse_dir);
    if (parent != NULL)
    {
        ShaderBrowseAppend(M_StringDuplicate(".."));
        free(parent);
    }

    // 2) Subfolders, sorted (full paths from I_StartGlob, marker added).
    glob = I_StartGlob(shader_browse_dir, "*",
                       GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED | GLOB_FLAG_DIRS);
    if (glob != NULL)
    {
        while ((path = I_NextGlob(glob)) != NULL)
        {
            ShaderBrowseAppend(M_StringJoin(path, DIR_SEPARATOR_S, NULL));
        }
        I_EndGlob(glob);
    }

    // 3) Presets, sorted. An unreadable folder (deleted under us,
    //    permissions) lands here with zero rows; the ".." row - or the
    //    menus' no-shaders fallback - is the way out.
    glob = I_StartGlob(shader_browse_dir, "*.slangp",
                       GLOB_FLAG_NOCASE | GLOB_FLAG_SORTED);
    if (glob != NULL)
    {
        while ((path = I_NextGlob(glob)) != NULL)
        {
            ShaderBrowseAppend(M_StringDuplicate(path));
        }
        I_EndGlob(glob);
    }

    if (count != NULL)
    {
        *count = shader_browse_n;
    }
    return shader_browse;
}

boolean I_ShaderBrowseGoto(const char *entry)
{
    char target[I_SHADER_DIR_MAX];
    size_t len;

    if (entry == NULL || entry[0] == '\0')
    {
        return false;
    }

    if (strcmp(entry, "..") == 0)
    {
        char *parent = ShaderDirParent(shader_browse_dir);

        if (parent == NULL)
        {
            return false;
        }
        M_snprintf(target, sizeof(target), "%s", parent);
        free(parent);
    }
    else if (I_ShaderEntryIsDir(entry))
    {
        // Descend; strip the marker separator(s) so the stored folder
        // is canonical (but keep a lone "/" root as-is).
        M_snprintf(target, sizeof(target), "%s", entry);
        len = strlen(target);
        while (len > 1 && (target[len - 1] == '/'
                           || target[len - 1] == '\\'))
        {
            target[--len] = '\0';
        }
    }
    else
    {
        return false;
    }

    M_snprintf(shader_browse_dir, sizeof(shader_browse_dir), "%s", target);
    return true;
}

// Row label: ".." as-is; folders as NAME/ (basename minus the marker,
// slash re-added for display); presets as NAME (extension stripped).
void I_ShaderRowName(const char *entry, char *buf, size_t buf_size)
{
    char tmp[I_SHADER_DIR_MAX];
    char *dot;
    size_t len;

    if (strcmp(entry, "..") == 0)
    {
        M_snprintf(buf, buf_size, "..");
        return;
    }
    if (I_ShaderEntryIsDir(entry))
    {
        M_snprintf(tmp, sizeof(tmp), "%s", entry);
        len = strlen(tmp);
        while (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        {
            tmp[--len] = '\0';
        }
        M_snprintf(buf, buf_size, "%s%c", M_BaseName(tmp),
                   DIR_SEPARATOR);
        return;
    }
    M_snprintf(tmp, sizeof(tmp), "%s", M_BaseName(entry));
    dot = strrchr(tmp, '.');
    if (dot != NULL)
    {
        *dot = '\0';
    }
    M_snprintf(buf, buf_size, "%s", tmp);
}

const char *I_CurrentShaderPreset(void)
{
    return shader_preset != NULL ? shader_preset : "";
}

// Menu-facing view of I_VK_Active(): keeps i_video_vk.h (which declares
// SDL_Window *screen) out of the game menus - hexen's h2def.h declares
// the byte * form and the two cannot coexist in one translation unit.
boolean I_VulkanActive(void)
{
    return I_VK_Active();
}

// Apply a browser pick. With the Vulkan path live this hot-swaps the
// preset (prepare-then-commit inside I_VK_SetPreset) and persists it;
// on the classic renderer it only persists - picking a shader is
// explicit intent, so a stale usevulkan = 0 is re-enabled for the next
// run exactly like a -shader command-line pass. Returns true only when
// the swap went live.
boolean I_SelectShaderPreset(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }
    if (I_VK_Active())
    {
        return I_VK_SetPreset(path);
    }
    usevulkan = 1;
    shader_preset = M_StringDuplicate(path);
    return false;
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
    M_BindIntVariable("use_mouse",                 &usemouse);
    M_BindIntVariable("fullscreen",                &fullscreen);
    M_BindIntVariable("video_display",             &video_display);
    M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
    M_BindIntVariable("integer_scaling",           &integer_scaling);
    M_BindIntVariable("smooth_pixel_scaling",      &smooth_pixel_scaling);
    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    M_BindIntVariable("startup_delay",             &startup_delay);
    M_BindIntVariable("fullscreen_width",          &fullscreen_width);
    M_BindIntVariable("fullscreen_height",         &fullscreen_height);
    M_BindIntVariable("force_software_renderer",   &force_software_renderer);
    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    M_BindIntVariable("window_width",              &window_width);
    M_BindIntVariable("window_height",             &window_height);
    M_BindIntVariable("grabmouse",                 &grabmouse);
    M_BindStringVariable("video_driver",           &video_driver);
    M_BindStringVariable("window_position",        &window_position);
    M_BindIntVariable("usegamma",                  &usegamma);
    M_BindIntVariable("png_screenshots",           &png_screenshots);

    // Chocolate Doom VK (libslang-vk):

    M_BindIntVariable("usevulkan",                  &usevulkan);
    M_BindStringVariable("shader",                   &shader_preset);
    M_BindIntVariable("vkstats",                    &vkstats);
    M_BindIntVariable("shader_base_resolution",  &shader_base_resolution);
    M_BindIntVariable("vk_fps_cap",                 &vk_fps_cap);
    M_BindIntVariable("vk_present_mode",            &vk_present_mode);
}