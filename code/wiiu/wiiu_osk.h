/*
 * wiiu_osk.h -- custom on-screen keyboard for the Wii U port.
 *
 * nn::swkbd (the system keyboard) hangs forever inside Create() on real
 * hardware and has no known-good homebrew call sequence, so this draws its own
 * key grid through the engine's 2D path and injects the result as SE_CHAR
 * events. The engine-integration half (inject/auto-submit/field-clear) is
 * ported from the PS3 port's code/input/ps3_osk.c, which solves the same
 * problem against a system keyboard that does work.
 */

#ifndef WIIU_OSK_H
#define WIIU_OSK_H

#include <vpad/input.h>

#include "qcommon/q_shared.h"

/* Open the keyboard, picking mode from the current key catcher, or close it
 * again if already open. Raised by L3 + B (chat) or by A while the console /
 * chat prompt holds the key catcher. */
void     WiiU_OSK_Toggle(void);

/* Close without committing (used on shutdown / foreground loss). */
void     WiiU_OSK_Cancel(void);

qboolean WiiU_OSK_IsActive(void);

/* Drive the keyboard from a fresh VPAD sample. Only called while active; the
 * GamePad's normal button/stick/touch mapping is suppressed for that frame. */
void     WiiU_OSK_Frame(const VPADStatus *status);

/* Draw the overlay. Called from SCR_DrawScreenField() after everything else. */
void     WiiU_OSK_Draw(void);

#endif /* WIIU_OSK_H */
