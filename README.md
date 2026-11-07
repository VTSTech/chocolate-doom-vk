# Chocolate Doom VK

[![CI Build Status](https://github.com/VTSTech/chocolate-doom-vk/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/VTSTech/chocolate-doom-vk/actions/workflows/cppcheck.yml) [![CI Build Status](https://github.com/VTSTech/chocolate-doom-vk/actions/workflows/vk-shaders.yml/badge.svg)](https://github.com/VTSTech/chocolate-doom-vk/actions/workflows/vk-shaders.yml) [![Downloads](https://img.shields.io/github/downloads/VTSTech/chocolate-doom-vk/total)](https://github.com/VTSTech/chocolate-doom-vk/releases) [![Latest Commit](https://img.shields.io/github/last-commit/VTSTech/chocolate-doom-vk/master?label=Latest%20Commit&logo=github)](https://github.com/VTSTech/chocolate-doom-vk/commits/master) [![Release](https://img.shields.io/github/v/tag/VTSTech/chocolate-doom-vk?label=Release&logo=github)](https://github.com/VTSTech/chocolate-doom-vk/releases) [![Discord](https://img.shields.io/badge/Discord-join%20the%20chat-5865F2?logo=discord&logoColor=white)](https://discord.gg/xQXpH5Nj5)

Chocolate Doom VK is a fork of
[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom) that
aims to upgrade and enhance the original DOS version of Doom and other
games based on the Doom engine to a form that can be run on modern
computers with Vulkan GPU rendering and every frame of the game passing
through a **libretro slang shader preset** (`.slangp` / `.slang`) via
VTSTech's [libslang-vk](https://github.com/VTSTech/libslang-vk) C
Library. Other improvements include FOV and widescreen upscaling
support.

CRT scanlines, LCD grids, phosphor decay, bezels — the same
post-processing ecosystem emulators use, applied to the original 1993
engine. With the feature off (or on any failure), the games behave
exactly like upstream Chocolate Doom. See [README-VK.md](README-VK.md)
for the full details, and [CHANGELOG.md](CHANGELOG.md) for what is new.

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/11604b42-1da0-43b2-b9cf-82af8c105e80" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/a4400020-7b63-4643-af74-61f0b040d1f7" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/7584b0fd-3010-47bd-859c-ac25cde76c3c" />

Originally, Chocolate Doom was only a Doom source port. The project
now includes ports of Heretic, Hexen and Strife — and all four games
share the same Vulkan shader path.

The aims are:

 * To always be 100% Free and Open Source software.
 * Portability to as many different operating systems as possible.
 * Accurate reproduction of the original DOS versions of the games,
   including bugs.
 * Compatibility with the DOS demo, configuration and savegame files.
 * To provide an accurate retro “feel” (display and input should
   behave the same), with optional modern shader post-processing.

More information about the philosophy and design behind the port can
be found in the PHILOSOPHY file distributed with the source code.

## Setting up gameplay

For instructions on how to set up the games for play, see the INSTALL
file.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build          # chocolate-doom, -heretic, -hexen, -strife
```

No Vulkan SDK is required — a Vulkan-capable driver is enough, and
`ENABLE_VK_SHADERS=OFF` builds the classic, upstream-identical path.
See [README-VK.md](README-VK.md) for build requirements and options.

## Configuration File

The ports are compatible with the DOS Doom configuration file
(normally named `default.cfg`). Existing configuration files for DOS
Doom should therefore simply work out of the box. However, extra
settings are also provided; these are stored in a separate file named
`chocolate-doom.cfg` (or `chocolate-heretic.cfg`, `chocolate-hexen.cfg`
and `chocolate-strife.cfg` for the other games).

The configuration can be edited using the chocolate-setup tool.

## Command line options

The games support a number of command line parameters, including some
extras that were not originally supported by the DOS versions. For
binary distributions, see the CMDLINE file included with your download.

## Playing TCs

With Vanilla Doom there is no way to include sprites in PWAD files.
The ‘-file’ command line option behaves exactly the same as Vanilla
Doom, and trying to play TCs by adding the WAD files using ‘-file’
will not work.

Many Total Conversions (TCs) are distributed as a PWAD file which must
be merged into the main IWAD. Typically a copy of DEUSF.EXE is
included which performs this merge. A ‘-merge’ option is included
which will simulate this merge. Essentially, the WAD directory is
merged in memory, removing the need to modify the IWAD on disk.

To play TCs, run like this:

```
chocolate-doom -merge thetc.wad
```

Here are some examples:

```
chocolate-doom -merge batman.wad -deh batman.deh vbatman.deh  (Batman Doom)
chocolate-doom -merge aoddoom1.wad -deh aoddoom1.deh  (Army of Darkness Doom)
```

## Other information

 * The games include a number of different options for music playback.
   See the README.Music file for more details.

 * Because the aim is to accurately emulate and reproduce the DOS
   executables, you may encounter Vanilla Doom bugs; these are
   intentionally present; see the NOT-BUGS file for more information.

 * Notes specific to the Strife port are in the README.Strife file.

 * New bug reports, feedback, questions or suggestions can be submitted
   to the issue tracker on Github:

     https://github.com/VTSTech/chocolate-doom-vk/issues

 * Source code patches are welcome, but please follow the style
   guidelines - see the file named HACKING included with the source
   distribution.

 * The games are distributed under the GNU GPL. See the COPYING file
   for more information.
