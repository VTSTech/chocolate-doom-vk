//
// i_log.c — gpu-scaler.log session log for the Vulkan shader path.
// See i_log.h for the contract.
//
// Copyright (C) 2025 VTSTech (Nigel Todman) — veritas@vts-tech.org
// License: GNU General Public License, version 2 (see COPYING.md).
//

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "doomtype.h"
#include "i_log.h"
#include "m_config.h"
#include "m_misc.h"

#define I_LOG_FILENAME "gpu-scaler.log"
#define I_LOG_OLDNAME  "gpu-scaler.log.old"

// Path headroom: any real configdir plus the fixed tail.
#define I_LOG_PATH_MAX 1024

// Longest rendered entry. Every caller message is a short single line;
// I_Error's 512-byte buffer through "I_Error: %s\n" fits with room to
// spare (a truncated entry still gets its newline).
#define I_LOG_LINE_MAX 1024

static FILE *log_file = NULL;
static char log_path[I_LOG_PATH_MAX] = "";

// Console echo switch (vk_log_console / -vklog, see i_log.h). Bound to
// the config var from I_BindVideoVariables; read per write, so a value
// from default.cfg or the command line is honoured whenever it lands.
int vk_log_console = 0;

// Set by the first I_LogInit call: init runs once per process, later
// attempts (video re-init) reuse the already-open session.
static boolean log_tried = false;

// "[%H:%M:%S] " — enough to line a stall or a crash up with the wall
// clock; within a session the file order is the true order anyway.
static void TimestampPrefix(char *out, size_t out_size)
{
    time_t now;
    struct tm *tmv;

    now = time(NULL);
    tmv = localtime(&now);
    if (tmv == NULL
        || strftime(out, out_size, "[%H:%M:%S] ", tmv) == 0)
    {
        M_StringCopy(out, "[??:??:??] ", out_size);
    }
}

// Should this entry also go to the console? Two cases say yes: the
// user asked for it (vk_log_console, -vklog), or there is no durable
// home — when the log file could not be opened the console is the
// only place the message will ever exist, exactly the situation the
// module was built for (a Windows GUI-subsystem binary whose stderr
// is invisible has no console to pollute, and a terminal run without
// a log prefers noisy over blind). The default, healthy case — file
// open, echo off — keeps the terminal clean: the game must not spray
// its log over the shell it was started from (2026-10-26 field
// report: I_VK lines interleaved with the startup dots made a wall
// of text and left the cursor stranded mid-scroll on exit).
static boolean ConsoleEcho(void)
{
    return vk_log_console != 0 || log_file == NULL;
}

// Render one entry and append it — stamp first, newline last, flushed
// immediately so a crash cannot eat the tail of the log.
static void LogWrite(const char *fmt, va_list args)
{
    char line[I_LOG_LINE_MAX];
    char stamp[16];
    int len;

    if (log_file == NULL)
    {
        return;
    }

    len = M_vsnprintf(line, sizeof(line), fmt, args);
    if (len < 0)
    {
        return;             // formatting error: skip, never log garbage
    }
    if ((size_t) len > sizeof(line) - 2)
    {
        len = (int) sizeof(line) - 2;   // truncated: keep room for '\n'
    }

    TimestampPrefix(stamp, sizeof(stamp));
    fputs(stamp, log_file);
    fputs(line, log_file);
    if (len == 0 || line[len - 1] != '\n')
    {
        fputc('\n', log_file);
    }
    fflush(log_file);
}

void I_LogInit(void)
{
    char old_path[I_LOG_PATH_MAX];
    const char *sep;
    size_t len;
    int n;

    if (log_tried)
    {
        return;
    }
    log_tried = true;

    // The log lives with default.cfg and the shader cache; without a
    // config dir there is nowhere sensible to put it — stay off.
    if (configdir == NULL || configdir[0] == '\0')
    {
        return;
    }

    // configdir conventionally ends in a separator (M_SetConfigDir),
    // but a hand-set one may not — join either way.
    len = strlen(configdir);
    sep = (len > 0 && configdir[len - 1] != '/'
           && configdir[len - 1] != '\\') ? "/" : "";

    n = snprintf(log_path, sizeof(log_path), "%s%s%s",
                 configdir, sep, I_LOG_FILENAME);
    if (n < 0 || (size_t) n >= sizeof(log_path) - 1)
    {
        log_path[0] = '\0';
        return;
    }
    n = snprintf(old_path, sizeof(old_path), "%s%s%s",
                 configdir, sep, I_LOG_OLDNAME);
    if (n < 0 || (size_t) n >= sizeof(old_path) - 1)
    {
        old_path[0] = '\0';
    }

    // One session per file: push the previous run to .old. Best-effort
    // — a locked or unwritable previous log only costs history, never
    // the current session.
    if (old_path[0] != '\0')
    {
        M_remove(old_path);
        M_rename(log_path, old_path);
    }

    log_file = M_fopen(log_path, "w");
    if (log_file == NULL)
    {
        printf("I_VK: cannot open %s for writing — file logging disabled\n",
               log_path);
        log_path[0] = '\0';
        return;
    }

    atexit(I_LogClose);

    I_LogMessage("gpu-scaler session log — %s\n", PACKAGE_STRING);
    I_LogMessage("previous session is kept as %s\n", I_LOG_OLDNAME);
    I_LogMessage("log file: %s\n", log_path);
}

void I_LogClose(void)
{
    if (log_file == NULL)
    {
        return;
    }

    I_LogMessage("session end\n");
    fclose(log_file);
    log_file = NULL;
}

int I_LogEnabled(void)
{
    return log_file != NULL;
}

const char *I_LogPath(void)
{
    return log_path;
}

void I_LogMessage(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogWrite(fmt, args);
    va_end(args);
}

void I_LogPrintf(const char *fmt, ...)
{
    va_list args, copy;

    va_start(args, fmt);
    va_copy(copy, args);
    if (ConsoleEcho())
    {
        vprintf(fmt, args);
    }
    va_end(args);
    LogWrite(fmt, copy);
    va_end(copy);
}

void I_LogError(const char *fmt, ...)
{
    va_list args, copy;

    va_start(args, fmt);
    va_copy(copy, args);
    if (ConsoleEcho())
    {
        vfprintf(stderr, fmt, args);
    }
    va_end(args);
    LogWrite(fmt, copy);
    va_end(copy);
}
