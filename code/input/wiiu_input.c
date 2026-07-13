/*
 * wiiu_input.c -- Wii U input backend.
 * Polls VPAD/KPAD, translates to ioq3 events via Com_QueueEvent().
 */

#include <string.h>
#include <math.h>

#include <vpad/input.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "client/keycodes.h"
#include "client/keys.h"
#include "../input/wiiu_input.h"

/* Avoid pulling in the full client.h header */
extern int Key_GetCatcher(void);

/* ----------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------- */
#define STICK_DEADZONE      0.15f
/* Engine-wide axis range is ±32767, not ±127 -- learned that one the hard way. */
#define STICK_AXIS_SCALE    32767
#define MENU_CURSOR_SPEED   5.0f    /* pixels/frame at full deflection */
#define MAX_KPAD_CHANNELS   4

/* Axis indices matching j_*_axis cvars set in wiiu_main.c cmdline */
#define AXIS_SIDE       0   /* left stick X -> strafe */
#define AXIS_FORWARD    1   /* left stick Y -> fwd/back */
#define AXIS_YAW        4   /* right stick X -> yaw */
#define AXIS_PITCH      3   /* right stick Y -> pitch */

/* ----------------------------------------------------------------
 * Button mapping tables: hardware bit -> ioq3 keycode, per context.
 * ---------------------------------------------------------------- */
typedef struct {
    uint32_t    mask;
    int         key_game;
    int         key_menu;
} buttonMap_t;

static const buttonMap_t vpadButtons[] = {
    { VPAD_BUTTON_A,        K_PAD0_A,               K_ENTER },
    { VPAD_BUTTON_B,        K_PAD0_B,               K_ESCAPE },
    { VPAD_BUTTON_X,        K_PAD0_X,               0 },
    { VPAD_BUTTON_Y,        K_PAD0_Y,               K_CONSOLE },
    { VPAD_BUTTON_L,        K_PAD0_LEFTSHOULDER,    0 },
    { VPAD_BUTTON_R,        K_PAD0_RIGHTSHOULDER,    0 },
    { VPAD_BUTTON_ZL,       K_PAD0_LEFTTRIGGER,     0 },
    { VPAD_BUTTON_ZR,       K_PAD0_RIGHTTRIGGER,     0 },
    { VPAD_BUTTON_PLUS,     K_ESCAPE,               K_ESCAPE },
    { VPAD_BUTTON_MINUS,    K_PAD0_BACK,            0 },
    { VPAD_BUTTON_STICK_L,  K_PAD0_LEFTSTICK_CLICK, 0 },
    { VPAD_BUTTON_STICK_R,  K_PAD0_RIGHTSTICK_CLICK,0 },
    { VPAD_BUTTON_UP,       K_PAD0_DPAD_UP,         K_UPARROW },
    { VPAD_BUTTON_DOWN,     K_PAD0_DPAD_DOWN,       K_DOWNARROW },
    { VPAD_BUTTON_LEFT,     K_PAD0_DPAD_LEFT,       K_LEFTARROW },
    { VPAD_BUTTON_RIGHT,    K_PAD0_DPAD_RIGHT,      K_RIGHTARROW },
};
#define VPAD_BUTTON_COUNT (sizeof(vpadButtons) / sizeof(vpadButtons[0]))

/* Pro Controller layout, Classic Controller shares it minus stick clicks. */
static const buttonMap_t proButtons[] = {
    { WPAD_PRO_BUTTON_A,        K_PAD0_A,               K_ENTER },
    { WPAD_PRO_BUTTON_B,        K_PAD0_B,               K_ESCAPE },
    { WPAD_PRO_BUTTON_X,        K_PAD0_X,               0 },
    { WPAD_PRO_BUTTON_Y,        K_PAD0_Y,               K_CONSOLE },
    { WPAD_PRO_BUTTON_L,        K_PAD0_LEFTSHOULDER,    0 },
    { WPAD_PRO_BUTTON_R,        K_PAD0_RIGHTSHOULDER,    0 },
    { WPAD_PRO_BUTTON_ZL,       K_PAD0_LEFTTRIGGER,     0 },
    { WPAD_PRO_BUTTON_ZR,       K_PAD0_RIGHTTRIGGER,     0 },
    { WPAD_PRO_BUTTON_PLUS,     K_ESCAPE,               K_ESCAPE },
    { WPAD_PRO_BUTTON_MINUS,    K_PAD0_BACK,            0 },
    { WPAD_PRO_BUTTON_STICK_L,  K_PAD0_LEFTSTICK_CLICK, 0 },
    { WPAD_PRO_BUTTON_STICK_R,  K_PAD0_RIGHTSTICK_CLICK,0 },
    { WPAD_PRO_BUTTON_UP,       K_PAD0_DPAD_UP,         K_UPARROW },
    { WPAD_PRO_BUTTON_DOWN,     K_PAD0_DPAD_DOWN,       K_DOWNARROW },
    { WPAD_PRO_BUTTON_LEFT,     K_PAD0_DPAD_LEFT,       K_LEFTARROW },
    { WPAD_PRO_BUTTON_RIGHT,    K_PAD0_DPAD_RIGHT,      K_RIGHTARROW },
};
#define PRO_BUTTON_COUNT (sizeof(proButtons) / sizeof(proButtons[0]))

static const buttonMap_t classicButtons[] = {
    { WPAD_CLASSIC_BUTTON_A,    K_PAD0_A,               K_ENTER },
    { WPAD_CLASSIC_BUTTON_B,    K_PAD0_B,               K_ESCAPE },
    { WPAD_CLASSIC_BUTTON_X,    K_PAD0_X,               0 },
    { WPAD_CLASSIC_BUTTON_Y,    K_PAD0_Y,               K_CONSOLE },
    { WPAD_CLASSIC_BUTTON_L,    K_PAD0_LEFTSHOULDER,    0 },
    { WPAD_CLASSIC_BUTTON_R,    K_PAD0_RIGHTSHOULDER,    0 },
    { WPAD_CLASSIC_BUTTON_ZL,   K_PAD0_LEFTTRIGGER,     0 },
    { WPAD_CLASSIC_BUTTON_ZR,   K_PAD0_RIGHTTRIGGER,     0 },
    { WPAD_CLASSIC_BUTTON_PLUS, K_ESCAPE,               K_ESCAPE },
    { WPAD_CLASSIC_BUTTON_MINUS,K_PAD0_BACK,            0 },
    { WPAD_CLASSIC_BUTTON_UP,   K_PAD0_DPAD_UP,         K_UPARROW },
    { WPAD_CLASSIC_BUTTON_DOWN, K_PAD0_DPAD_DOWN,       K_DOWNARROW },
    { WPAD_CLASSIC_BUTTON_LEFT, K_PAD0_DPAD_LEFT,       K_LEFTARROW },
    { WPAD_CLASSIC_BUTTON_RIGHT,K_PAD0_DPAD_RIGHT,      K_RIGHTARROW },
};
#define CLASSIC_BUTTON_COUNT (sizeof(classicButtons) / sizeof(classicButtons[0]))

/* ----------------------------------------------------------------
 * State
 * ---------------------------------------------------------------- */
static qboolean     input_initialized;
static qboolean     quit_pressed;

static uint8_t      kpad_prev_ext[MAX_KPAD_CHANNELS];

/* Previous stick axis values (avoid redundant events) */
static int          vpad_prev_axis[4];
static int          kpad_prev_axis[MAX_KPAD_CHANNELS][4];

/* Menu cursor sub-pixel accumulators */
static float        cursor_accum_x, cursor_accum_y;

/* Touch screen state */
static qboolean     touch_prev_valid;
static uint16_t     touch_prev_x, touch_prev_y;

/* Track which ioq3 keys are currently held (for release-all on mode switch) */
static qboolean     key_held[MAX_KEYS];
static qboolean     prev_in_menu;

/* Stick response tuning (registered in IN_Init) */
static cvar_t       *stick_move_deadzone;
static cvar_t       *stick_move_curve;
static cvar_t       *stick_look_deadzone;
static cvar_t       *stick_look_curve;

/* Touch response tuning (registered in IN_Init) */
static cvar_t       *touch_sensitivity;
static cvar_t       *touch_accel;

/* Rumble (registered in IN_Init) */
static cvar_t       *rumble_enable;
static int           s_rumbleExpiryMs;
static qboolean      s_rumbleActive;

/* VPADControlMotor timing is undocumented -- send a long pattern, cut it early with
 * VPADStopMotor() instead. Gave up guessing the byte-to-ms ratio. */
static uint8_t s_rumblePattern[40];  /* filled with 0xFF in WiiU_Input_Init */

/* ----------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------- */

static qboolean InMenu(void)
{
    int catcher = Key_GetCatcher();
    return (catcher & (KEYCATCH_UI | KEYCATCH_CGAME | KEYCATCH_CONSOLE)) ? qtrue : qfalse;
}

static void InjectKey(int key, qboolean down)
{
    if (key <= 0 || key >= MAX_KEYS)
        return;
    if (key_held[key] == down)
        return;
    key_held[key] = down;
    Com_QueueEvent(0, SE_KEY, key, down, 0, NULL);
}

static void ReleaseAllKeys(void)
{
    int i;
    for (i = 0; i < MAX_KEYS; i++) {
        if (key_held[i]) {
            key_held[i] = qfalse;
            Com_QueueEvent(0, SE_KEY, i, qfalse, 0, NULL);
        }
    }
}

/* Deadzone + response curve, -1..1 -> ±32767 axis range. curve==1 is linear. */
static int FilterStick(float raw, float deadzone, float curve)
{
    if (deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 0.9f) deadzone = 0.9f;
    if (curve < 0.25f) curve = 0.25f;
    if (curve > 4.0f) curve = 4.0f;

    if (raw > -deadzone && raw < deadzone)
        return 0;

    float sign = (raw < 0.0f) ? -1.0f : 1.0f;
    float mag = (fabsf(raw) - deadzone) / (1.0f - deadzone);
    if (mag > 1.0f) mag = 1.0f;
    if (curve != 1.0f) mag = powf(mag, curve);

    return (int)(sign * mag * STICK_AXIS_SCALE);
}

/* Scale a clamped touch delta by sensitivity + quadratic accel ramp (fast flicks move further). */
static int ScaleTouchDelta(int raw, float sensitivity, float accel)
{
    float mag = fabsf((float)raw) / 40.0f;   /* 0..1 against the clamp */
    float mult = sensitivity + accel * mag * mag;
    return (int)((float)raw * mult);
}

/* Emit SE_MOUSE from a stick for menu cursor movement (squared curve + sub-pixel accum). */
static void InjectMenuCursor(float sx, float sy)
{
    if (sx > -STICK_DEADZONE && sx < STICK_DEADZONE) sx = 0.0f;
    if (sy > -STICK_DEADZONE && sy < STICK_DEADZONE) sy = 0.0f;

    /* Squared response for fine control at small deflections */
    float fx = sx * fabsf(sx) * MENU_CURSOR_SPEED;
    float fy = sy * fabsf(sy) * MENU_CURSOR_SPEED;

    cursor_accum_x += fx;
    cursor_accum_y += fy;

    int dx = (int)cursor_accum_x;
    int dy = (int)cursor_accum_y;

    cursor_accum_x -= (float)dx;
    cursor_accum_y -= (float)dy;

    /* Q3 mouse Y is down-positive, stick Y is up-positive -- negate. */
    if (dx || dy)
        Com_QueueEvent(0, SE_MOUSE, dx, -dy, 0, NULL);
}

/* Emit SE_JOYSTICK_AXIS with change detection */
static void InjectAxis(int axis, int value, int *prev)
{
    if (value != *prev) {
        *prev = value;
        Com_QueueEvent(0, SE_JOYSTICK_AXIS, axis, value, 0, NULL);
    }
}

/* Process a button map table against trigger/release bitmasks */
static void ProcessButtons(const buttonMap_t *map, int count,
                           uint32_t triggered, uint32_t released,
                           qboolean in_menu)
{
    int i;
    for (i = 0; i < count; i++) {
        int key = in_menu ? map[i].key_menu : map[i].key_game;
        if (!key) key = map[i].key_game;  /* fallback if menu key is 0 */

        if (triggered & map[i].mask)
            InjectKey(key, qtrue);
        if (released & map[i].mask)
            InjectKey(key, qfalse);
    }
}

/* Set a default bind only if the key is currently unbound */
static void SetDefaultBind(int key, const char *binding)
{
    const char *current = Key_GetBinding(key);
    if (!current || !current[0])
        Key_SetBinding(key, binding);
}

/* ----------------------------------------------------------------
 * Rumble (GamePad motor). On/off only, so strength = pulse duration;
 * a new pulse extends (not restarts) the expiry so autofire doesn't stutter it.
 * ---------------------------------------------------------------- */
void WiiU_SetRumble(int durationMs)
{
    if (!input_initialized)
        return;
    if (!rumble_enable || !rumble_enable->integer)
        return;

    VPADControlMotor(VPAD_CHAN_0, s_rumblePattern, sizeof(s_rumblePattern));

    int expiry = Com_Milliseconds() + durationMs;
    if (!s_rumbleActive || expiry > s_rumbleExpiryMs)
        s_rumbleExpiryMs = expiry;
    s_rumbleActive = qtrue;
}

/* Stop the motor once the latest pulse has expired, called every frame. */
static void WiiU_Input_RumbleTick(void)
{
    if (!s_rumbleActive)
        return;
    if (Com_Milliseconds() < s_rumbleExpiryMs)
        return;

    VPADStopMotor(VPAD_CHAN_0);
    s_rumbleActive = qfalse;
}

/* ----------------------------------------------------------------
 * VPAD (GamePad) polling
 * ---------------------------------------------------------------- */
static void PollVPAD(qboolean in_menu)
{
    VPADStatus status;
    VPADReadError error = VPAD_READ_SUCCESS;

    int samples = VPADRead(VPAD_CHAN_0, &status, 1, &error);
    if (samples <= 0 || error != VPAD_READ_SUCCESS)
        return;

    /* Quit combo: Plus + Minus held; mask them so ESCAPE/CONSOLE don't also fire. */
    if ((status.hold & VPAD_BUTTON_PLUS) &&
        (status.hold & VPAD_BUTTON_MINUS)) {
        quit_pressed = qtrue;
        status.trigger &= ~(VPAD_BUTTON_PLUS | VPAD_BUTTON_MINUS);
        status.release &= ~(VPAD_BUTTON_PLUS | VPAD_BUTTON_MINUS);
    }

    /* Rumble toggle: L3 + X, fires on edge. At least this one only took one hw cycle. */
    if ((status.trigger & VPAD_BUTTON_X && status.hold & VPAD_BUTTON_STICK_L) ||
        (status.trigger & VPAD_BUTTON_STICK_L && status.hold & VPAD_BUTTON_X)) {
        qboolean newState = rumble_enable->integer ? qfalse : qtrue;
        Cvar_Set("rumble_enable", newState ? "1" : "0");
        Com_Printf(newState ? "ENABLED RUMBLE\n" : "DISABLED RUMBLE\n");
        if (!newState) {
            VPADStopMotor(VPAD_CHAN_0);
            s_rumbleActive = qfalse;
        }
        /* Mask X and Stick-L so their normal binds don't also fire. */
        status.trigger &= ~(VPAD_BUTTON_X | VPAD_BUTTON_STICK_L);
        status.release &= ~(VPAD_BUTTON_X | VPAD_BUTTON_STICK_L);
    }

    /* Button edge detection using hardware trigger/release fields */
    ProcessButtons(vpadButtons, VPAD_BUTTON_COUNT,
                   status.trigger, status.release, in_menu);

    /* Analog sticks */
    float lx = status.leftStick.x;
    float ly = status.leftStick.y;
    float rx = status.rightStick.x;
    float ry = status.rightStick.y;

    if (in_menu) {
        /* Left stick drives menu cursor, right stick ignored */
        InjectMenuCursor(lx, ly);
    } else {
        /* Left stick strafe/forward, right stick look, both via SE_JOYSTICK_AXIS */
        InjectAxis(AXIS_SIDE,    FilterStick(lx,  stick_move_deadzone->value, stick_move_curve->value),  &vpad_prev_axis[0]);
        InjectAxis(AXIS_FORWARD, FilterStick(-ly, stick_move_deadzone->value, stick_move_curve->value),  &vpad_prev_axis[1]);
        InjectAxis(AXIS_YAW,    FilterStick(rx,  stick_look_deadzone->value, stick_look_curve->value),  &vpad_prev_axis[2]);
        InjectAxis(AXIS_PITCH,  FilterStick(-ry, stick_look_deadzone->value, stick_look_curve->value),  &vpad_prev_axis[3]);
    }

    /* Touch screen -> SE_MOUSE (delta model, clamped) */
    VPADTouchData tp;
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &tp, &status.tpNormal);

    if (tp.touched && tp.validity == VPAD_VALID) {
        if (touch_prev_valid) {
            int tdx = (int)tp.x - (int)touch_prev_x;
            int tdy = (int)tp.y - (int)touch_prev_y;

            /* Clamp to prevent jumps when finger re-lands */
            if (tdx > 40) tdx = 40;
            if (tdx < -40) tdx = -40;
            if (tdy > 40) tdy = 40;
            if (tdy < -40) tdy = -40;

            int mx = ScaleTouchDelta(tdx, touch_sensitivity->value, touch_accel->value);
            int my = ScaleTouchDelta(tdy, touch_sensitivity->value, touch_accel->value);

            if (mx || my)
                Com_QueueEvent(0, SE_MOUSE, mx, my, 0, NULL);
        }
        touch_prev_valid = qtrue;
        touch_prev_x = tp.x;
        touch_prev_y = tp.y;
    } else {
        touch_prev_valid = qfalse;
    }

}

/* ----------------------------------------------------------------
 * KPAD (Pro Controller / Classic Controller) polling
 * ---------------------------------------------------------------- */
static void PollKPAD(qboolean in_menu)
{
    int ch;
    for (ch = 0; ch < MAX_KPAD_CHANNELS; ch++) {
        KPADStatus status;
        KPADError error = KPAD_ERROR_OK;

        uint32_t samples = KPADReadEx((KPADChan)ch, &status, 1, &error);
        if (samples == 0 || error != KPAD_ERROR_OK)
            continue;

        /* Release all keys if extension type changed (controller swap) */
        if (status.extensionType != kpad_prev_ext[ch]) {
            kpad_prev_ext[ch] = status.extensionType;
            memset(kpad_prev_axis[ch], 0, sizeof(kpad_prev_axis[ch]));
            ReleaseAllKeys();
        }

        if (status.extensionType == WPAD_EXT_PRO_CONTROLLER) {
            ProcessButtons(proButtons, PRO_BUTTON_COUNT,
                           status.pro.trigger, status.pro.release, in_menu);

            /* Quit combo: Plus + Minus */
            if ((status.pro.hold & WPAD_PRO_BUTTON_PLUS) &&
                (status.pro.hold & WPAD_PRO_BUTTON_MINUS)) {
                quit_pressed = qtrue;
            }

            float lx = status.pro.leftStick.x;
            float ly = status.pro.leftStick.y;
            float rx = status.pro.rightStick.x;
            float ry = status.pro.rightStick.y;

            if (in_menu) {
                InjectMenuCursor(lx, ly);
            } else {
                InjectAxis(AXIS_SIDE,    FilterStick(lx,  stick_move_deadzone->value, stick_move_curve->value),  &kpad_prev_axis[ch][0]);
                InjectAxis(AXIS_FORWARD, FilterStick(-ly, stick_move_deadzone->value, stick_move_curve->value),  &kpad_prev_axis[ch][1]);
                InjectAxis(AXIS_YAW,    FilterStick(rx,  stick_look_deadzone->value, stick_look_curve->value),  &kpad_prev_axis[ch][2]);
                InjectAxis(AXIS_PITCH,  FilterStick(-ry, stick_look_deadzone->value, stick_look_curve->value),  &kpad_prev_axis[ch][3]);
            }

        } else if (status.extensionType == WPAD_EXT_CLASSIC) {
            ProcessButtons(classicButtons, CLASSIC_BUTTON_COUNT,
                           status.classic.trigger, status.classic.release, in_menu);

            if ((status.classic.hold & WPAD_CLASSIC_BUTTON_PLUS) &&
                (status.classic.hold & WPAD_CLASSIC_BUTTON_MINUS)) {
                quit_pressed = qtrue;
            }

            float lx = status.classic.leftStick.x;
            float ly = status.classic.leftStick.y;
            float rx = status.classic.rightStick.x;
            float ry = status.classic.rightStick.y;

            if (in_menu) {
                InjectMenuCursor(lx, ly);
            } else {
                InjectAxis(AXIS_SIDE,    FilterStick(lx,  stick_move_deadzone->value, stick_move_curve->value),  &kpad_prev_axis[ch][0]);
                InjectAxis(AXIS_FORWARD, FilterStick(-ly, stick_move_deadzone->value, stick_move_curve->value),  &kpad_prev_axis[ch][1]);
                InjectAxis(AXIS_YAW,    FilterStick(rx,  stick_look_deadzone->value, stick_look_curve->value),  &kpad_prev_axis[ch][2]);
                InjectAxis(AXIS_PITCH,  FilterStick(-ry, stick_look_deadzone->value, stick_look_curve->value),  &kpad_prev_axis[ch][3]);
            }

        }
        /* Bare Wii Remote (no extension) is not useful for Q3, skip. */
    }
}

/* ----------------------------------------------------------------
 * Public interface
 * ---------------------------------------------------------------- */

void WiiU_Input_Init(void)
{
    if (input_initialized)
        return;

    VPADInit();
    KPADInit();
    WPADEnableURCC(TRUE);

    memset(key_held, 0, sizeof(key_held));
    memset(vpad_prev_axis, 0, sizeof(vpad_prev_axis));
    memset(kpad_prev_ext, 0xFF, sizeof(kpad_prev_ext));
    memset(kpad_prev_axis, 0, sizeof(kpad_prev_axis));

    memset(s_rumblePattern, 0xFF, sizeof(s_rumblePattern));
    s_rumbleActive = qfalse;
    s_rumbleExpiryMs = 0;

    cursor_accum_x = 0.0f;
    cursor_accum_y = 0.0f;
    touch_prev_valid = qfalse;
    quit_pressed = qfalse;
    prev_in_menu = qfalse;

    input_initialized = qtrue;
}

void WiiU_Input_Shutdown(void)
{
    if (!input_initialized)
        return;
    /* Stop any in-flight rumble before VPADShutdown, or the motor pattern gets stuck with no way to cancel it. */
    VPADStopMotor(VPAD_CHAN_0);
    s_rumbleActive = qfalse;
    /* Leaving KPAD/VPAD alive at exit stalls the OS finalizers -- cost a whole freeze investigation. */
    KPADShutdown();
    VPADShutdown();
    input_initialized = qfalse;
}

void WiiU_Input_Frame(void)
{
    if (!input_initialized)
        return;

    qboolean in_menu = InMenu();

    /* Release all keys on mode transition to prevent stuck keys */
    if (in_menu != prev_in_menu) {
        ReleaseAllKeys();
        cursor_accum_x = 0.0f;
        cursor_accum_y = 0.0f;
        touch_prev_valid = qfalse;
        memset(vpad_prev_axis, 0, sizeof(vpad_prev_axis));
        memset(kpad_prev_axis, 0, sizeof(kpad_prev_axis));
        prev_in_menu = in_menu;
    }

    PollVPAD(in_menu);
    PollKPAD(in_menu);
    WiiU_Input_RumbleTick();
}

int WiiU_Input_QuitPressed(void)
{
    return quit_pressed;
}

void WiiU_Input_ReleaseForeground(void)
{
    if (!input_initialized)
        return;
    VPADStopMotor(VPAD_CHAN_0);
    s_rumbleActive = qfalse;
}

/* ----------------------------------------------------------------
 * ioq3 input interface (called by engine via IN_* function pointers)
 * ---------------------------------------------------------------- */

void IN_Init(void *windowData)
{
    (void)windowData;

    /* Default gameplay binds (only if unbound), mirrors the vWii port's DRC layout. */
    SetDefaultBind(K_PAD0_A,                "+moveup");       /* jump */
    SetDefaultBind(K_PAD0_B,                "+movedown");     /* crouch */
    SetDefaultBind(K_PAD0_X,                "weapnext");
    SetDefaultBind(K_PAD0_Y,                "weapprev");
    SetDefaultBind(K_PAD0_LEFTSHOULDER,     "+speed");        /* walk */
    SetDefaultBind(K_PAD0_RIGHTSHOULDER,    "+button2");      /* use item */
    SetDefaultBind(K_PAD0_LEFTTRIGGER,      "+zoom");
    SetDefaultBind(K_PAD0_RIGHTTRIGGER,     "+attack");       /* fire */
    SetDefaultBind(K_PAD0_LEFTSTICK_CLICK,  "+scores");
    SetDefaultBind(K_PAD0_RIGHTSTICK_CLICK, "+scores");
    SetDefaultBind(K_PAD0_BACK,             "+scores");       /* Minus */
    SetDefaultBind(K_PAD0_DPAD_UP,          "weapnext");
    SetDefaultBind(K_PAD0_DPAD_DOWN,        "weapprev");
    SetDefaultBind(K_PAD0_DPAD_LEFT,        "weapprev");
    SetDefaultBind(K_PAD0_DPAD_RIGHT,       "weapnext");

    /* Joystick axis cvars set on the command line in wiiu_main.c. */
    Cvar_SetSafe("in_joystick", "1");
    Cvar_SetSafe("in_joystickUseAnalog", "1");

    /* Stick deadzone/curve, applied in FilterStick(). Defaults = old hardcoded linear behavior. */
    stick_move_deadzone = Cvar_Get("stick_move_deadzone", "0.15", CVAR_ARCHIVE);
    stick_move_curve    = Cvar_Get("stick_move_curve",    "1.0",  CVAR_ARCHIVE);
    stick_look_deadzone = Cvar_Get("stick_look_deadzone", "0.15", CVAR_ARCHIVE);
    stick_look_curve    = Cvar_Get("stick_look_curve",    "1.0",  CVAR_ARCHIVE);

    /* Touch response tuning, scaled in ScaleTouchDelta(). touch_accel 0.0 = flat multiply. */
    touch_sensitivity   = Cvar_Get("touch_sensitivity",   "1.5",  CVAR_ARCHIVE);
    touch_accel         = Cvar_Get("touch_accel",         "0.0",  CVAR_ARCHIVE);

    /* Rumble on weapon fire/damage (see snd_dma.c). Toggle with L3+X. */
    rumble_enable       = Cvar_Get("rumble_enable",       "1",    CVAR_ARCHIVE);
}

void IN_Shutdown(void)
{
    /* Nothing to do; WiiU_Input_Shutdown handles hardware */
}

void IN_Frame(void)
{
    /* Polling done in WiiU_Input_Frame(), called before Com_Frame(). */
}

void IN_Restart(void)
{
    IN_Shutdown();
    IN_Init(NULL);
}
