/* pc_pad.c - GC controller input via SDL gamepad + keyboard */
#include "pc_platform.h"
#include "pc_typing.h"
#include "pc_keybindings.h"
#include "pc_settings.h"
#include <dolphin/pad.h>

/* analog stick constants */
#define STICK_MAGNITUDE     80
#define RUMBLE_DURATION_MS  200

// every connected controller is open and read; inputs are combined (adapters like the Mayflash
// GameCube one show up as 4 controllers, one per port, empty ports included)
#define PC_PAD_MAX 8
static SDL_GameController* s_pads[PC_PAD_MAX];

/* deadzone percent (0-40) -> raw SDL axis threshold */
static int deadzone_threshold(int percent) {
    if (percent < 0)  percent = 0;
    if (percent > 90) percent = 90;
    return percent * 32767 / 100;
}

/* is a remappable pad binding currently held? */
static int pad_code_pressed(SDL_GameController* c, PCPadCode code) {
    if (code < 0) return 0;
    if (code & PC_PAD_AXIS_BIT) {
        return SDL_GameControllerGetAxis(c,
            (SDL_GameControllerAxis)(code & 0xFF)) > PC_PAD_AXIS_PRESS;
    }
    return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)code);
}

/* analog trigger value for the L/R binding (digital bindings read as full press) */
static u8 pad_trigger_value(SDL_GameController* c, PCPadCode code) {
    if (code < 0) return 0;
    if (code & PC_PAD_AXIS_BIT) {
        s16 v = SDL_GameControllerGetAxis(c, (SDL_GameControllerAxis)(code & 0xFF));
        if (v < 0) v = 0;
        return (u8)(v >> 7);
    }
    return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)code) ? 255 : 0;
}

// close unplugged controllers, open new ones (hotplug; a few lookups per frame)
static void pads_refresh(void) {
    for (int p = 0; p < PC_PAD_MAX; p++) {
        if (s_pads[p] && !SDL_GameControllerGetAttached(s_pads[p])) {
            printf("[PAD] closed: %s\n", SDL_GameControllerName(s_pads[p]));
            SDL_GameControllerClose(s_pads[p]);
            s_pads[p] = NULL;
        }
    }
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        int open = 0;
        int free_slot = -1;
        for (int p = 0; p < PC_PAD_MAX; p++) {
            if (!s_pads[p]) {
                if (free_slot < 0) free_slot = p;
                continue;
            }
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_pads[p])) == id) open = 1;
        }
        if (open || free_slot < 0) continue;
        s_pads[free_slot] = SDL_GameControllerOpen(i);
        if (s_pads[free_slot]) {
            printf("[PAD] opened %d: %s\n", free_slot, SDL_GameControllerName(s_pads[free_slot]));
        }
    }
}

// keep whichever value is pushed further from the center
static s16 axis_max(s16 a, s16 b) {
    return abs(b) > abs(a) ? b : a;
}

BOOL PADInit(void) {
    pads_refresh();
    return TRUE;
}

u32 PADRead(PADStatus* status) {
    memset(status, 0, sizeof(PADStatus) * 4);

    const u8* keys = SDL_GetKeyboardState(NULL);
    u32 mouse = SDL_GetMouseState(NULL, NULL);
    u16 buttons = 0;
    s8 stickX = 0, stickY = 0;
    s8 cstickX = 0, cstickY = 0;

    /* Suppress keyboard-to-button mapping when typing into the in-game text editor */
    if (!(g_pc_typing_mode && g_pc_editor_active)) {
        /* helper: check if a PCInputCode is currently pressed */
        #define INPUT_PRESSED(code) \
            (((code) & PC_INPUT_MOUSE_BIT) \
                ? (mouse & SDL_BUTTON((code) & 0xFF)) \
                : keys[(SDL_Scancode)(code)])

        /* buttons (from keybindings.ini) */
        PCKeybindings* kb = &g_pc_keybindings;
        if (INPUT_PRESSED(kb->a))     buttons |= PAD_BUTTON_A;
        if (INPUT_PRESSED(kb->b))     buttons |= PAD_BUTTON_B;
        if (INPUT_PRESSED(kb->x))     buttons |= PAD_BUTTON_X;
        if (INPUT_PRESSED(kb->y))     buttons |= PAD_BUTTON_Y;
        if (INPUT_PRESSED(kb->start)) buttons |= PAD_BUTTON_START;
        if (INPUT_PRESSED(kb->z))     buttons |= PAD_TRIGGER_Z;
        if (INPUT_PRESSED(kb->l))     buttons |= PAD_TRIGGER_L;
        if (INPUT_PRESSED(kb->r))     buttons |= PAD_TRIGGER_R;

        /* main stick */
        if (INPUT_PRESSED(kb->stick_up))    stickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_down))  stickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_left))  stickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->stick_right)) stickX += STICK_MAGNITUDE;

        /* C-stick */
        if (INPUT_PRESSED(kb->cstick_up))    cstickY += STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_down))  cstickY -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_left))  cstickX -= STICK_MAGNITUDE;
        if (INPUT_PRESSED(kb->cstick_right)) cstickX += STICK_MAGNITUDE;

        /* D-pad */
        if (INPUT_PRESSED(kb->dpad_up))    buttons |= PAD_BUTTON_UP;
        if (INPUT_PRESSED(kb->dpad_down))  buttons |= PAD_BUTTON_DOWN;
        if (INPUT_PRESSED(kb->dpad_left))  buttons |= PAD_BUTTON_LEFT;
        if (INPUT_PRESSED(kb->dpad_right)) buttons |= PAD_BUTTON_RIGHT;

        #undef INPUT_PRESSED
    }

    pads_refresh();

    // all open controllers combined: buttons ORed, each axis/trigger from the one pushed furthest
    int any_pad = 0;
    s16 lx = 0, ly = 0, rx = 0, ry = 0;
    u8 trig_l = 0, trig_r = 0;
    PCPadBindings* pb = &g_pc_padbindings;
    for (int p = 0; p < PC_PAD_MAX; p++) {
        SDL_GameController* c = s_pads[p];
        if (!c) continue;
        any_pad = 1;
        if (pad_code_pressed(c, pb->a))     buttons |= PAD_BUTTON_A;
        if (pad_code_pressed(c, pb->b))     buttons |= PAD_BUTTON_B;
        if (pad_code_pressed(c, pb->x))     buttons |= PAD_BUTTON_X;
        if (pad_code_pressed(c, pb->y))     buttons |= PAD_BUTTON_Y;
        if (pad_code_pressed(c, pb->start)) buttons |= PAD_BUTTON_START;
        if (pad_code_pressed(c, pb->z))     buttons |= PAD_TRIGGER_Z;
        if (pad_code_pressed(c, pb->l))     buttons |= PAD_TRIGGER_L;
        if (pad_code_pressed(c, pb->r))     buttons |= PAD_TRIGGER_R;
        if (pad_code_pressed(c, pb->dpad_up))    buttons |= PAD_BUTTON_UP;
        if (pad_code_pressed(c, pb->dpad_down))  buttons |= PAD_BUTTON_DOWN;
        if (pad_code_pressed(c, pb->dpad_left))  buttons |= PAD_BUTTON_LEFT;
        if (pad_code_pressed(c, pb->dpad_right)) buttons |= PAD_BUTTON_RIGHT;

        lx = axis_max(lx, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX));
        ly = axis_max(ly, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
        rx = axis_max(rx, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX));
        ry = axis_max(ry, SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));

        u8 tl = pad_trigger_value(c, pb->l);
        u8 tr = pad_trigger_value(c, pb->r);
        if (tl > trig_l) trig_l = tl;
        if (tr > trig_r) trig_r = tr;
    }

    if (any_pad) {
        int stick_dz  = deadzone_threshold(g_pc_settings.stick_deadzone);
        int cstick_dz = deadzone_threshold(g_pc_settings.cstick_deadzone);

        if (abs(lx) > stick_dz) {
            int sx = lx >> 8;
            if (sx > 127) sx = 127; else if (sx < -128) sx = -128;
            stickX = (s8)sx;
        }
        if (abs(ly) > stick_dz) {
            int sy = -(ly >> 8);
            if (sy > 127) sy = 127; else if (sy < -128) sy = -128;
            stickY = (s8)sy;
        }

        if (abs(rx) > cstick_dz) {
            int srx = rx >> 8;
            if (srx > 127) srx = 127; else if (srx < -128) srx = -128;
            cstickX = (s8)srx;
        }
        if (abs(ry) > cstick_dz) {
            int sry = -(ry >> 8);
            if (sry > 127) sry = 127; else if (sry < -128) sry = -128;
            cstickY = (s8)sry;
        }

        status[0].triggerLeft  = trig_l;
        status[0].triggerRight = trig_r;
    }

    status[0].button = buttons;
    status[0].stickX = stickX;
    status[0].stickY = stickY;
    status[0].substickX = cstickX;
    status[0].substickY = cstickY;
    status[0].err = 0; /* PAD_ERR_NONE */

    return PAD_CHAN0_BIT; /* Controller 1 connected */
}

void PADControlMotor(s32 chan, u32 command) {
    if (chan != 0) return;
    u16 intensity = (command == 1) ? 0xFFFF : 0;
    for (int p = 0; p < PC_PAD_MAX; p++) {
        if (s_pads[p]) SDL_GameControllerRumble(s_pads[p], intensity, intensity, RUMBLE_DURATION_MS);
    }
}

void PADControlAllMotors(const u32* commands) {
    PADControlMotor(0, commands[0]);
}

void PADCleanup(void) {
    for (int p = 0; p < PC_PAD_MAX; p++) {
        if (s_pads[p]) {
            SDL_GameControllerClose(s_pads[p]);
            s_pads[p] = NULL;
        }
    }
}

BOOL PADReset(u32 mask) { (void)mask; return TRUE; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }
BOOL PADSync(void) { return TRUE; }
void PADSetSpec(u32 spec) { (void)spec; }
void PADSetAnalogMode(u32 mode) { (void)mode; }
/* PADClamp compiled from decomp: src/static/dolphin/pad/Padclamp.c */
BOOL PADGetType(s32 chan, u32* type) { if (type) *type = 0x09000000; return TRUE; }
