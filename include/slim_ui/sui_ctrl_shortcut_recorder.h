/* sui_ctrl_shortcut_recorder.h — Unified keyboard/gamepad shortcut recorder.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

/* Must differ from the tooltip timer: both are dispatched on the host HWND. */
#define SUI_SHORTCUT_RECORD_TIMER_ID 0x5554
#define SUI_SHORTCUT_GAMEPAD_TRIGGER_THRESHOLD 80
#define SUI_SHORTCUT_XINPUT_DISCONNECTED_RETRY_MS 5000ULL
#define SUI_SHORTCUT_RAW_GAMEPAD_USAGE_PAGE 0x01
#define SUI_SHORTCUT_RAW_JOYSTICK_USAGE     0x04
#define SUI_SHORTCUT_RAW_GAMEPAD_USAGE      0x05
#define SUI_SHORTCUT_GAMEPAD_EDGE_FN_LEFT       0x00040000u
#define SUI_SHORTCUT_GAMEPAD_EDGE_FN_RIGHT      0x00080000u
#define SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_LEFT   0x00100000u
#define SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_RIGHT  0x00200000u

typedef struct SuiShortcutXInputGamepad {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} SuiShortcutXInputGamepad;

typedef struct SuiShortcutXInputState {
    DWORD dwPacketNumber;
    SuiShortcutXInputGamepad Gamepad;
} SuiShortcutXInputState;
typedef struct SuiShortcutXInputSlot {
    ULONGLONG next_probe_tick;
    int disconnected;
} SuiShortcutXInputSlot;

typedef DWORD (WINAPI *SuiShortcutXInputGetStateFn)(DWORD, SuiShortcutXInputState*);
#ifndef SUI_SHORTCUT_XINPUT_GET_STATE
#define SUI_SHORTCUT_XINPUT_GET_STATE(index, state) \
    sui_ctrl_shortcut_local_xinput_get_state((index), (state))
#define SUI_SHORTCUT_LOCAL_XINPUT_LOADER 1
static DWORD sui_ctrl_shortcut_local_xinput_get_state(
    DWORD index, SuiShortcutXInputState* state);
#endif
static int sui_ctrl_shortcut_any_gamepad_recording(SuiControlHost* host);
static int sui_ctrl_shortcut_apply_gamepad_buttons(SuiControlHost* host, uint32_t pressed);

static uint32_t sui_ctrl_shortcut_raw_gamepad_buttons = 0;
static DWORD sui_ctrl_shortcut_raw_gamepad_tick = 0;
static SuiShortcutXInputSlot sui_ctrl_shortcut_xinput_slots[4];

static void sui_ctrl_shortcut_append(wchar_t* buf, int cap, const wchar_t* part) {
    int len = 0, i = 0;
    if (!buf || cap <= 0 || !part || !part[0]) return;
    while (buf[len] && len < cap - 1) len++;
    if (len > 0 && len < cap - 1) buf[len++] = L'+';
    while (part[i] && len < cap - 1) buf[len++] = part[i++];
    buf[len] = 0;
}

static wchar_t sui_ctrl_shortcut_lower_wide_ascii(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : c;
}

static int sui_ctrl_shortcut_contains_wide_ascii(const wchar_t* haystack, const wchar_t* needle) {
    const wchar_t* h;
    if (!haystack || !needle || !needle[0]) return 0;
    for (h = haystack; *h; ++h) {
        const wchar_t* a = h;
        const wchar_t* b = needle;
        while (*a && *b &&
               sui_ctrl_shortcut_lower_wide_ascii(*a) == sui_ctrl_shortcut_lower_wide_ascii(*b)) {
            ++a;
            ++b;
        }
        if (!*b) return 1;
    }
    return 0;
}

static int sui_ctrl_shortcut_raw_device_info(HANDLE device, RID_DEVICE_INFO* info) {
    UINT size;
    if (!device || !info) return 0;
    memset(info, 0, sizeof(*info));
    info->cbSize = sizeof(*info);
    size = sizeof(*info);
    return GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, info, &size) != (UINT)-1;
}

static int sui_ctrl_shortcut_raw_device_is_gamepad(HANDLE device) {
    RID_DEVICE_INFO info;
    if (!sui_ctrl_shortcut_raw_device_info(device, &info) || info.dwType != RIM_TYPEHID) return 0;
    return info.hid.usUsagePage == SUI_SHORTCUT_RAW_GAMEPAD_USAGE_PAGE &&
        (info.hid.usUsage == SUI_SHORTCUT_RAW_GAMEPAD_USAGE ||
         info.hid.usUsage == SUI_SHORTCUT_RAW_JOYSTICK_USAGE);
}

static int sui_ctrl_shortcut_raw_device_is_sony(HANDLE device) {
    UINT chars = 0;
    wchar_t* name;
    int sony = 0;
    if (!device) return 0;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, NULL, &chars) == (UINT)-1 || chars == 0)
        return 0;
    name = (wchar_t*)sui_malloc(sizeof(wchar_t) * (chars + 1));
    if (!name) return 0;
    memset(name, 0, sizeof(wchar_t) * (chars + 1));
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name, &chars) != (UINT)-1) {
        sony = sui_ctrl_shortcut_contains_wide_ascii(name, L"VID_054C") ||
            sui_ctrl_shortcut_contains_wide_ascii(name, L"VID&0002054C") ||
            sui_ctrl_shortcut_contains_wide_ascii(name, L"dualsense") ||
            sui_ctrl_shortcut_contains_wide_ascii(name, L"dualshock") ||
            sui_ctrl_shortcut_contains_wide_ascii(name, L"playstation") ||
            sui_ctrl_shortcut_contains_wide_ascii(name, L"sony");
    }
    sui_free(name);
    return sony;
}

static void sui_ctrl_shortcut_add_hid_hat_buttons(BYTE hat, uint32_t* buttons) {
    if (!buttons || hat > 7) return;
    if (hat == 0 || hat == 1 || hat == 7) *buttons |= SUI_GAMEPAD_DPAD_UP;
    if (hat >= 1 && hat <= 3) *buttons |= SUI_GAMEPAD_DPAD_RIGHT;
    if (hat >= 3 && hat <= 5) *buttons |= SUI_GAMEPAD_DPAD_DOWN;
    if (hat >= 5 && hat <= 7) *buttons |= SUI_GAMEPAD_DPAD_LEFT;
}

static uint32_t sui_ctrl_shortcut_map_dualsense_report_buttons(
    const BYTE* report,
    DWORD size,
    DWORD offset,
    DWORD left_trigger_index,
    DWORD right_trigger_index) {
    uint32_t buttons = 0;
    BYTE b0, b1, b2, hat;
    if (!report || size <= offset + 2 ||
        size <= left_trigger_index || size <= right_trigger_index) return 0;
    b0 = report[offset];
    b1 = report[offset + 1];
    b2 = report[offset + 2];
    hat = b0 & 0x0F;

    sui_ctrl_shortcut_add_hid_hat_buttons(hat, &buttons);
    if (b0 & 0x10) buttons |= SUI_GAMEPAD_X;
    if (b0 & 0x20) buttons |= SUI_GAMEPAD_A;
    if (b0 & 0x40) buttons |= SUI_GAMEPAD_B;
    if (b0 & 0x80) buttons |= SUI_GAMEPAD_Y;
    if (b1 & 0x01) buttons |= SUI_GAMEPAD_LEFT_SHOULDER;
    if (b1 & 0x02) buttons |= SUI_GAMEPAD_RIGHT_SHOULDER;
    if (b1 & 0x04) buttons |= SUI_SHORTCUT_GAMEPAD_LEFT_TRIGGER;
    if (b1 & 0x08) buttons |= SUI_SHORTCUT_GAMEPAD_RIGHT_TRIGGER;
    if (b1 & 0x10) buttons |= SUI_GAMEPAD_BACK;
    if (b1 & 0x20) buttons |= SUI_GAMEPAD_START;
    if (b1 & 0x40) buttons |= SUI_GAMEPAD_LEFT_THUMB;
    if (b1 & 0x80) buttons |= SUI_GAMEPAD_RIGHT_THUMB;
    if (b2 & 0x01) buttons |= SUI_GAMEPAD_GUIDE;
    /* The padded Bluetooth 0x01 compatibility report does not expose Edge
       controls. Its byte 7 is vendor-defined and can mimic Fn-L at startup. */
    if (report[0] != 0x01 || size == 64) {
        if (b2 & 0x10) buttons |= SUI_SHORTCUT_GAMEPAD_EDGE_FN_LEFT;
        if (b2 & 0x20) buttons |= SUI_SHORTCUT_GAMEPAD_EDGE_FN_RIGHT;
        if (b2 & 0x40) buttons |= SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_LEFT;
        if (b2 & 0x80) buttons |= SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_RIGHT;
    }
    if (report[left_trigger_index] >= SUI_SHORTCUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= SUI_SHORTCUT_GAMEPAD_LEFT_TRIGGER;
    if (report[right_trigger_index] >= SUI_SHORTCUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= SUI_SHORTCUT_GAMEPAD_RIGHT_TRIGGER;
    return buttons;
}

static uint32_t sui_ctrl_shortcut_map_dualsense_report(const BYTE* report, DWORD size) {
    if (!report || size < 11) return 0;
    if (report[0] == 0x31 && size > 11)
        return sui_ctrl_shortcut_map_dualsense_report_buttons(report, size, 9, 6, 7);
    if (report[0] == 0x01 && size == 78)
        return sui_ctrl_shortcut_map_dualsense_report_buttons(report, size, 5, 8, 9);
    if (report[0] == 0x01 && size == 64)
        return sui_ctrl_shortcut_map_dualsense_report_buttons(report, size, 8, 5, 6);
    return 0;
}

static int sui_ctrl_shortcut_register_raw_gamepad(HWND hwnd) {
    RAWINPUTDEVICE devices[2];
    if (!hwnd) return 0;
    memset(devices, 0, sizeof(devices));
    devices[0].usUsagePage = SUI_SHORTCUT_RAW_GAMEPAD_USAGE_PAGE;
    devices[0].usUsage = SUI_SHORTCUT_RAW_GAMEPAD_USAGE;
    devices[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    devices[0].hwndTarget = hwnd;
    devices[1].usUsagePage = SUI_SHORTCUT_RAW_GAMEPAD_USAGE_PAGE;
    devices[1].usUsage = SUI_SHORTCUT_RAW_JOYSTICK_USAGE;
    devices[1].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    devices[1].hwndTarget = hwnd;
    return RegisterRawInputDevices(devices, 2, sizeof(devices[0])) != FALSE;
}

static int sui_ctrl_shortcut_read_raw_input_gamepad(HRAWINPUT input, uint32_t* out_buttons) {
    UINT size = 0;
    RAWINPUT* raw;
    uint32_t buttons = 0;
    int is_gamepad, sony, parsed = 0;
    if (out_buttons) *out_buttons = 0;
    if (!input || !out_buttons) return 0;
    if (GetRawInputData(input, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0)
        return 0;
    raw = (RAWINPUT*)sui_malloc(size);
    if (!raw) return 0;
    if (GetRawInputData(input, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) != size ||
        raw->header.dwType != RIM_TYPEHID) {
        sui_free(raw);
        return 0;
    }

    is_gamepad = sui_ctrl_shortcut_raw_device_is_gamepad(raw->header.hDevice);
    sony = sui_ctrl_shortcut_raw_device_is_sony(raw->header.hDevice);
    if (is_gamepad || sony) {
        DWORD i;
        BYTE* report = raw->data.hid.bRawData;
        DWORD report_size = raw->data.hid.dwSizeHid;
        DWORD report_count = raw->data.hid.dwCount;
        parsed = 1;
        if (report_size > 0) {
            for (i = 0; i < report_count; ++i) {
                if (sony) {
                    buttons |= sui_ctrl_shortcut_map_dualsense_report(report + (i * report_size),
                                                                      report_size);
                }
            }
        }
    }
    sui_free(raw);
    *out_buttons = buttons;
    return parsed;
}

static void sui_ctrl_shortcut_on_raw_input(SuiControlHost* host, HRAWINPUT input) {
#ifdef SUI_SHORTCUT_GAMEPAD_BUTTONS_PROVIDER
    /* The host owns packet parsing and exposes one coherent controller state.
     * Its 30 ms recorder timer applies that state without duplicating HID work. */
    (void)host;
    (void)input;
#else
    uint32_t buttons = 0;
    if (!sui_ctrl_shortcut_read_raw_input_gamepad(input, &buttons)) return;
    sui_ctrl_shortcut_raw_gamepad_buttons = buttons;
    sui_ctrl_shortcut_raw_gamepad_tick = GetTickCount();
    if (host && host->hwnd) {
        sui_ctrl_shortcut_apply_gamepad_buttons(host, buttons);
    }
#endif
}

static void sui_ctrl_shortcut_on_gamepad_device_change(void) {
    memset(sui_ctrl_shortcut_xinput_slots, 0, sizeof(sui_ctrl_shortcut_xinput_slots));
}

static void sui_ctrl_shortcut_format_gamepad(uint32_t buttons, wchar_t* buf, int cap) {
    buf[0] = 0;
    if (buttons & SUI_GAMEPAD_BACK)           sui_ctrl_shortcut_append(buf, cap, L"View");
    if (buttons & SUI_GAMEPAD_START)          sui_ctrl_shortcut_append(buf, cap, L"Menu");
    if (buttons & SUI_GAMEPAD_LEFT_SHOULDER)  sui_ctrl_shortcut_append(buf, cap, L"LB");
    if (buttons & SUI_GAMEPAD_RIGHT_SHOULDER) sui_ctrl_shortcut_append(buf, cap, L"RB");
    if (buttons & SUI_SHORTCUT_GAMEPAD_LEFT_TRIGGER) sui_ctrl_shortcut_append(buf, cap, L"LT");
    if (buttons & SUI_SHORTCUT_GAMEPAD_RIGHT_TRIGGER) sui_ctrl_shortcut_append(buf, cap, L"RT");
    if (buttons & SUI_GAMEPAD_LEFT_THUMB)     sui_ctrl_shortcut_append(buf, cap, L"LS");
    if (buttons & SUI_GAMEPAD_RIGHT_THUMB)    sui_ctrl_shortcut_append(buf, cap, L"RS");
    if (buttons & SUI_GAMEPAD_GUIDE)          sui_ctrl_shortcut_append(buf, cap, L"PS");
    if (buttons & SUI_SHORTCUT_GAMEPAD_EDGE_FN_LEFT) sui_ctrl_shortcut_append(buf, cap, L"Fn-L");
    if (buttons & SUI_SHORTCUT_GAMEPAD_EDGE_FN_RIGHT) sui_ctrl_shortcut_append(buf, cap, L"Fn-R");
    if (buttons & SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_LEFT) sui_ctrl_shortcut_append(buf, cap, L"Paddle-L");
    if (buttons & SUI_SHORTCUT_GAMEPAD_EDGE_PADDLE_RIGHT) sui_ctrl_shortcut_append(buf, cap, L"Paddle-R");
    if (buttons & SUI_GAMEPAD_DPAD_UP)        sui_ctrl_shortcut_append(buf, cap, L"DPad Up");
    if (buttons & SUI_GAMEPAD_DPAD_DOWN)      sui_ctrl_shortcut_append(buf, cap, L"DPad Down");
    if (buttons & SUI_GAMEPAD_DPAD_LEFT)      sui_ctrl_shortcut_append(buf, cap, L"DPad Left");
    if (buttons & SUI_GAMEPAD_DPAD_RIGHT)     sui_ctrl_shortcut_append(buf, cap, L"DPad Right");
    if (buttons & SUI_GAMEPAD_A)              sui_ctrl_shortcut_append(buf, cap, L"A");
    if (buttons & SUI_GAMEPAD_B)              sui_ctrl_shortcut_append(buf, cap, L"B");
    if (buttons & SUI_GAMEPAD_X)              sui_ctrl_shortcut_append(buf, cap, L"X");
    if (buttons & SUI_GAMEPAD_Y)              sui_ctrl_shortcut_append(buf, cap, L"Y");
}

#ifdef SUI_SHORTCUT_LOCAL_XINPUT_LOADER
static DWORD sui_ctrl_shortcut_local_xinput_get_state(
    DWORD index, SuiShortcutXInputState* state) {
    static int tried = 0;
    static SuiShortcutXInputGetStateFn get_state = NULL;
    static HMODULE mod = NULL;
    const wchar_t* dlls[] = { L"xinput1_4.dll", L"xinput9_1_0.dll", L"xinput1_3.dll" };
    int i;
    if (!tried) {
        tried = 1;
        for (i = 0; i < 3 && !mod; ++i) {
            mod = LoadLibraryW(dlls[i]);
        }
        if (mod) {
            get_state = (SuiShortcutXInputGetStateFn)GetProcAddress(mod, "XInputGetState");
        }
    }
    return get_state ? get_state(index, state) : ERROR_DEVICE_NOT_CONNECTED;
}
#endif

static uint32_t sui_ctrl_shortcut_read_gamepad_buttons(void) {
#ifdef SUI_SHORTCUT_GAMEPAD_BUTTONS_PROVIDER
    return SUI_SHORTCUT_GAMEPAD_BUTTONS_PROVIDER();
#else
    uint32_t pressed = 0;
    ULONGLONG now_tick = GetTickCount64();
    int i;

    for (i = 0; i < 4; ++i) {
        SuiShortcutXInputSlot* slot = &sui_ctrl_shortcut_xinput_slots[i];
        SuiShortcutXInputState state;
        if (slot->disconnected && now_tick < slot->next_probe_tick) continue;
        memset(&state, 0, sizeof(state));
        if (SUI_SHORTCUT_XINPUT_GET_STATE((DWORD)i, &state) == ERROR_SUCCESS) {
            uint32_t buttons = (uint32_t)state.Gamepad.wButtons;
            slot->disconnected = 0;
            slot->next_probe_tick = 0;
            if (state.Gamepad.bLeftTrigger >= SUI_SHORTCUT_GAMEPAD_TRIGGER_THRESHOLD)
                buttons |= SUI_SHORTCUT_GAMEPAD_LEFT_TRIGGER;
            if (state.Gamepad.bRightTrigger >= SUI_SHORTCUT_GAMEPAD_TRIGGER_THRESHOLD)
                buttons |= SUI_SHORTCUT_GAMEPAD_RIGHT_TRIGGER;
            pressed |= buttons;
        } else {
            slot->disconnected = 1;
            slot->next_probe_tick =
                now_tick + SUI_SHORTCUT_XINPUT_DISCONNECTED_RETRY_MS;
        }
    }
    if (pressed != 0) return pressed;
    if (sui_ctrl_shortcut_raw_gamepad_tick != 0) {
        pressed |= sui_ctrl_shortcut_raw_gamepad_buttons;
        if (pressed != 0) return pressed;
    }
    return pressed;
#endif
}

static void sui_ctrl_shortcut_sync_display(SuiControl* c) {
    if (!c || c->type != SUI_CTRL_SHORTCUT_RECORDER) return;
    if (c->d.shortcut.kind == SUI_SHORTCUT_GAMEPAD) {
        sui_ctrl_shortcut_format_gamepad(c->d.shortcut.gamepad_buttons, c->text, SUI_CTRL_TEXT_MAX);
    } else {
        sui_shortcut_key_format(c->d.shortcut.vk, c->d.shortcut.modifiers,
                                c->d.shortcut.scan_code, c->d.shortcut.extended,
                                c->d.shortcut.mouse_button, c->text, SUI_CTRL_TEXT_MAX);
    }
}

static void sui_ctrl_shortcut_set_recording(SuiControlHost* host, SuiControl* c, int recording) {
    int was_recording;
    if (!c || c->type != SUI_CTRL_SHORTCUT_RECORDER) return;
    was_recording = c->d.shortcut.recording;
    c->d.shortcut.recording = recording ? 1 : 0;
    c->d.shortcut.candidate_vk = 0;
    c->d.shortcut.candidate_modifiers = 0;
    c->d.shortcut.candidate_scan_code = 0;
    c->d.shortcut.candidate_extended = 0;
    c->d.shortcut.candidate_mouse_button = SUI_SHORTCUT_MOUSE_NONE;
    c->d.shortcut.candidate_mouse_released = 0;
    c->d.shortcut.candidate_gamepad_buttons = 0;
    c->d.shortcut.initial_gamepad_buttons = 0;
    if (c->d.shortcut.recording) {
        sui__wstrncpy_z(c->text, c->d.shortcut.prompt, SUI_CTRL_TEXT_MAX);
    } else {
        sui_ctrl_shortcut_sync_display(c);
    }
    if (host && host->hwnd && c->d.shortcut.kind == SUI_SHORTCUT_KEYBOARD) {
        if (c->d.shortcut.recording) {
            SetCapture(host->hwnd);
        } else if (was_recording && GetCapture() == host->hwnd) {
            ReleaseCapture();
        }
    }
    if (host && host->hwnd) InvalidateRect(host->hwnd, NULL, FALSE);
}

static int sui_ctrl_shortcut_any_gamepad_recording(SuiControlHost* host) {
    int i;
    if (!host) return 0;
    for (i = 0; i < host->control_count; ++i) {
        SuiControl* c = &host->controls[i];
        if (c->type == SUI_CTRL_SHORTCUT_RECORDER &&
            c->d.shortcut.kind == SUI_SHORTCUT_GAMEPAD &&
            c->d.shortcut.recording) {
            return 1;
        }
    }
    return 0;
}

static void sui_ctrl_shortcut_cancel_recording(SuiControlHost* host, SuiControl* c, int notify) {
    int was_recording;
    if (!c || c->type != SUI_CTRL_SHORTCUT_RECORDER) return;
    was_recording = c->d.shortcut.recording;
    sui_ctrl_shortcut_set_recording(host, c, 0);
    if (host && host->hwnd && !sui_ctrl_shortcut_any_gamepad_recording(host))
        KillTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID);
    if (notify && was_recording && c->d.shortcut.kind == SUI_SHORTCUT_GAMEPAD && host && host->on_click)
        host->on_click(c->id, host->on_click_data);
}

static void sui_ctrl_shortcut_set_prompt(SuiControl* c, const wchar_t* prompt) {
    if (!c || c->type != SUI_CTRL_SHORTCUT_RECORDER) return;
    sui__wstrncpy_z(c->d.shortcut.prompt, prompt ? prompt : L"Press shortcut...", 64);
    if (c->d.shortcut.recording) sui__wstrncpy_z(c->text, c->d.shortcut.prompt, SUI_CTRL_TEXT_MAX);
}

static int sui_ctrl_add_shortcut_recorder_keyboard_physical(SuiControlHost* host, int id,
                                                            float x, float y, float w, float h,
                                                            UINT vk, UINT modifiers,
                                                            UINT scan_code, int extended,
                                                            UINT mouse_button,
                                                            const wchar_t* prompt) {
    int idx = sui_ctrl_add(host, SUI_CTRL_SHORTCUT_RECORDER, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.shortcut.kind = SUI_SHORTCUT_KEYBOARD;
        c->d.shortcut.vk = vk;
        c->d.shortcut.modifiers = modifiers;
        c->d.shortcut.scan_code = scan_code;
        c->d.shortcut.extended = extended ? 1 : 0;
        c->d.shortcut.mouse_button = mouse_button;
        sui_ctrl_shortcut_set_prompt(c, prompt ? prompt : L"Press a key combination...");
        sui_ctrl_shortcut_sync_display(c);
    }
    return idx;
}

static int sui_ctrl_add_shortcut_recorder_keyboard(SuiControlHost* host, int id,
                                                   float x, float y, float w, float h,
                                                   UINT vk, UINT modifiers,
                                                   const wchar_t* prompt) {
    return sui_ctrl_add_shortcut_recorder_keyboard_physical(host, id, x, y, w, h,
                                                            vk, modifiers, 0, 0,
                                                            SUI_SHORTCUT_MOUSE_NONE, prompt);
}

static int sui_ctrl_add_shortcut_recorder_gamepad(SuiControlHost* host, int id,
                                                   float x, float y, float w, float h,
                                                   uint32_t buttons,
                                                   const wchar_t* prompt) {
    int idx = sui_ctrl_add(host, SUI_CTRL_SHORTCUT_RECORDER, id, x, y, w, h, L"");
    if (idx >= 0) {
        SuiControl* c = &host->controls[idx];
        c->d.shortcut.kind = SUI_SHORTCUT_GAMEPAD;
        c->d.shortcut.gamepad_buttons = buttons;
        sui_ctrl_shortcut_set_prompt(c, prompt ? prompt : L"Press gamepad buttons...");
        sui_ctrl_shortcut_sync_display(c);
    }
    return idx;
}

static void sui_ctrl_shortcut_draw_single_line_fit(SuiRenderer* r, const wchar_t* text,
    float x, float y, float w, float h, uint32_t color, float size,
    int bold, const wchar_t* family) {
    float draw_size = size;
    float text_w;
    if (!text || !text[0]) return;
    text_w = sui_renderer_measure_text_width_ex(r, text, bold, draw_size, family);
    if (text_w > w && text_w > 0.0f && w > 0.0f) {
        float min_size = 10.0f;
        draw_size = size * (w / text_w);
        if (draw_size < min_size) draw_size = min_size;
    }
    sui_renderer_draw_text_middle_singleline_ex(r, text, x, y, w, h, color,
        bold, draw_size, family);
}

static void sui_ctrl_draw_shortcut_recorder(SuiControlHost* host, SuiControl* ctrl) {
    SuiRenderer* r = host->renderer;
    const SuiColorScheme* sc = host->scheme;
    float s = sui_ctrl_dpi_scale(host), rad = 4.0f * s;
    float pad = 10.0f * s;
    int active = ctrl->d.shortcut.recording || (ctrl->state & SUI_CTRL_FOCUSED);
    uint32_t bg_color = sui_ctrl_style_color(ctrl, SUI_STYLE_BG_COLOR, sc->input_bg);
    uint32_t hover_color = sui_ctrl_style_color(ctrl, SUI_STYLE_HOVER_COLOR, sc->btn_hover);
    uint32_t press_color = sui_ctrl_style_color(ctrl, SUI_STYLE_PRESS_COLOR, sc->btn_press);
    uint32_t border_base = sui_ctrl_style_color(ctrl, SUI_STYLE_BORDER_COLOR, sc->input_border);
    uint32_t accent_color = sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->input_focus_border);
    uint32_t text_color = sui_ctrl_style_color(ctrl, SUI_STYLE_TEXT_COLOR, sc->input_text);
    uint32_t placeholder_color = sui_ctrl_style_color(ctrl, SUI_STYLE_PLACEHOLDER_COLOR, sc->input_placeholder);
    float font_size = sui_ctrl_style_font_size(ctrl, r->font_size);
    const wchar_t* font_family = sui_ctrl_style_font_family(ctrl);
    int bold = (ctrl->state & SUI_CTRL_BOLD) ? 1 : 0;
    uint32_t fill = (ctrl->state & SUI_CTRL_PRESSED)
        ? press_color
        : sui_color_lerp(bg_color, hover_color, ctrl->hover_t);

    if (ctrl->state & SUI_CTRL_HIDDEN) return;
    sui_renderer_fill_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad, fill);
    sui_renderer_draw_rounded_rect(r, ctrl->x, ctrl->y, ctrl->w, ctrl->h, rad,
        active ? accent_color
               : sui_color_lerp(border_base,
                   sui_ctrl_style_color(ctrl, SUI_STYLE_ACCENT_COLOR, sc->btn_default_border),
                   ctrl->hover_t),
        active ? 2.0f : 1.0f);

    if (ctrl->d.shortcut.recording && ctrl->d.shortcut.candidate_vk == 0 &&
        ctrl->d.shortcut.candidate_mouse_button == SUI_SHORTCUT_MOUSE_NONE &&
        ctrl->d.shortcut.candidate_gamepad_buttons == 0) {
        sui_ctrl_shortcut_draw_single_line_fit(r, ctrl->d.shortcut.prompt, ctrl->x + pad,
            ctrl->y, ctrl->w - pad * 2.0f, ctrl->h,
            placeholder_color, font_size, bold, font_family);
    } else {
        sui_ctrl_shortcut_draw_single_line_fit(r, ctrl->text, ctrl->x + pad,
            ctrl->y, ctrl->w - pad * 2.0f, ctrl->h,
            ctrl->text[0] ? text_color : placeholder_color, font_size, bold, font_family);
    }
    sui_ctrl_draw_kb_focus_ring(host, ctrl, rad);
}

static void sui_ctrl_shortcut_activate(SuiControlHost* host, SuiControl* c) {
    if (!host || !c || c->type != SUI_CTRL_SHORTCUT_RECORDER) return;
    if (c->d.shortcut.recording) {
        sui_ctrl_shortcut_cancel_recording(host, c, 1);
        return;
    }
    sui_ctrl_shortcut_set_recording(host, c, 1);
    if (c->d.shortcut.kind == SUI_SHORTCUT_GAMEPAD) {
        sui_ctrl_shortcut_raw_gamepad_buttons = 0;
        sui_ctrl_shortcut_raw_gamepad_tick = 0;
        sui_ctrl_shortcut_on_gamepad_device_change();
        sui_ctrl_shortcut_register_raw_gamepad(host->hwnd);
        c->d.shortcut.initial_gamepad_buttons = sui_ctrl_shortcut_read_gamepad_buttons();
        SetTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID, 30, NULL);
        if (host->on_click) host->on_click(c->id, host->on_click_data);
    }
}

static int sui_ctrl_shortcut_is_modifier(WPARAM vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU ||
           vk == VK_LWIN    || vk == VK_RWIN;
}

static int sui_ctrl_shortcut_vk_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static int sui_ctrl_shortcut_mods_down(UINT mods) {
    if ((mods & MOD_CONTROL) &&
        (sui_ctrl_shortcut_vk_down(VK_CONTROL) || sui_ctrl_shortcut_vk_down(VK_LCONTROL) || sui_ctrl_shortcut_vk_down(VK_RCONTROL)))
        return 1;
    if ((mods & MOD_ALT) &&
        (sui_ctrl_shortcut_vk_down(VK_MENU) || sui_ctrl_shortcut_vk_down(VK_LMENU) || sui_ctrl_shortcut_vk_down(VK_RMENU)))
        return 1;
    if ((mods & MOD_SHIFT) &&
        (sui_ctrl_shortcut_vk_down(VK_SHIFT) || sui_ctrl_shortcut_vk_down(VK_LSHIFT) || sui_ctrl_shortcut_vk_down(VK_RSHIFT)))
        return 1;
    if ((mods & MOD_WIN) &&
        (sui_ctrl_shortcut_vk_down(VK_LWIN) || sui_ctrl_shortcut_vk_down(VK_RWIN)))
        return 1;
    return 0;
}

static int sui_ctrl_shortcut_on_keydown(SuiControlHost* host, int idx, WPARAM vk, LPARAM lp) {
    SuiControl* c = &host->controls[idx];
    UINT mods, scan_code;
    int extended;
    if (c->d.shortcut.kind != SUI_SHORTCUT_KEYBOARD) return 0;
    if (!c->d.shortcut.recording) {
        if (vk == VK_SPACE || vk == VK_RETURN) {
            sui_ctrl_shortcut_activate(host, c);
            return 1;
        }
        return 0;
    }
    if (vk == VK_ESCAPE) {
        sui_ctrl_shortcut_cancel_recording(host, c, 0);
        return 1;
    }
    if (sui_ctrl_shortcut_is_modifier(vk)) return 1;
    if (vk == VK_TAB) return 0;

    mods = sui_shortcut_key_modifiers();
    scan_code = (UINT)((lp >> 16) & 0xFF);
    extended = (lp & 0x01000000) != 0;
    if (scan_code == 0) {
        UINT mapped = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC_EX);
        scan_code = mapped & 0xFF;
        extended = (mapped & 0xFF00) == 0xE000;
    }

    c->d.shortcut.candidate_vk = (UINT)vk;
    c->d.shortcut.candidate_modifiers = mods;
    c->d.shortcut.candidate_scan_code = scan_code;
    c->d.shortcut.candidate_extended = extended ? 1 : 0;
    c->d.shortcut.candidate_mouse_button = SUI_SHORTCUT_MOUSE_NONE;
    c->d.shortcut.candidate_mouse_released = 0;
    sui_shortcut_key_format_physical((UINT)vk, mods, scan_code, extended, c->text, SUI_CTRL_TEXT_MAX);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

static void sui_ctrl_shortcut_commit_keyboard(SuiControlHost* host, SuiControl* c) {
    c->d.shortcut.vk = c->d.shortcut.candidate_vk;
    c->d.shortcut.modifiers = c->d.shortcut.candidate_modifiers;
    c->d.shortcut.scan_code = c->d.shortcut.candidate_scan_code;
    c->d.shortcut.extended = c->d.shortcut.candidate_extended;
    c->d.shortcut.mouse_button = c->d.shortcut.candidate_mouse_button;
    sui_ctrl_shortcut_set_recording(host, c, 0);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static int sui_ctrl_shortcut_on_keyup(SuiControlHost* host, int idx, WPARAM vk) {
    SuiControl* c = &host->controls[idx];
    (void)vk;
    if (c->type != SUI_CTRL_SHORTCUT_RECORDER ||
        c->d.shortcut.kind != SUI_SHORTCUT_KEYBOARD ||
        !c->d.shortcut.recording ||
        (c->d.shortcut.candidate_vk == 0 &&
         c->d.shortcut.candidate_mouse_button == SUI_SHORTCUT_MOUSE_NONE)) {
        return 0;
    }
    if (c->d.shortcut.candidate_mouse_button != SUI_SHORTCUT_MOUSE_NONE) {
        if (!c->d.shortcut.candidate_mouse_released ||
            sui_ctrl_shortcut_mods_down(c->d.shortcut.candidate_modifiers)) {
            return 1;
        }
        sui_ctrl_shortcut_commit_keyboard(host, c);
        return 1;
    }
    if (sui_ctrl_shortcut_vk_down((int)c->d.shortcut.candidate_vk) ||
        sui_ctrl_shortcut_mods_down(c->d.shortcut.candidate_modifiers)) {
        return 1;
    }

    sui_ctrl_shortcut_commit_keyboard(host, c);
    return 1;
}

static int sui_ctrl_shortcut_on_mouse_down(SuiControlHost* host, UINT mouse_button) {
    int fc;
    SuiControl* c;
    if (!host) return 0;
    fc = host->focused_control;
    if (fc < 0 || fc >= host->control_count) return 0;
    c = &host->controls[fc];
    if (c->type != SUI_CTRL_SHORTCUT_RECORDER ||
        c->d.shortcut.kind != SUI_SHORTCUT_KEYBOARD ||
        !c->d.shortcut.recording) {
        return 0;
    }
    c->d.shortcut.candidate_vk = 0;
    c->d.shortcut.candidate_modifiers = sui_shortcut_key_modifiers();
    c->d.shortcut.candidate_scan_code = 0;
    c->d.shortcut.candidate_extended = 0;
    c->d.shortcut.candidate_mouse_button = mouse_button;
    c->d.shortcut.candidate_mouse_released = 0;
    sui_shortcut_key_format(0, c->d.shortcut.candidate_modifiers, 0, 0,
                            mouse_button, c->text, SUI_CTRL_TEXT_MAX);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 1;
}

static int sui_ctrl_shortcut_on_mouse_up(SuiControlHost* host, UINT mouse_button) {
    int fc;
    SuiControl* c;
    if (!host) return 0;
    fc = host->focused_control;
    if (fc < 0 || fc >= host->control_count) return 0;
    c = &host->controls[fc];
    if (c->type != SUI_CTRL_SHORTCUT_RECORDER ||
        c->d.shortcut.kind != SUI_SHORTCUT_KEYBOARD ||
        !c->d.shortcut.recording ||
        c->d.shortcut.candidate_mouse_button != mouse_button) {
        return 0;
    }
    c->d.shortcut.candidate_mouse_released = 1;
    if (sui_ctrl_shortcut_mods_down(c->d.shortcut.candidate_modifiers)) {
        return 1;
    }
    sui_ctrl_shortcut_commit_keyboard(host, c);
    return 1;
}

static int sui_ctrl_shortcut_count_buttons(uint32_t buttons) {
    int count = 0;
    while (buttons != 0) {
        buttons &= buttons - 1;
        ++count;
    }
    return count;
}

static void sui_ctrl_shortcut_commit_gamepad(SuiControlHost* host, SuiControl* c, uint32_t buttons) {
    if (!buttons) {
        sui_ctrl_shortcut_cancel_recording(host, c, 1);
        return;
    }
    c->d.shortcut.gamepad_buttons = buttons;
    sui_ctrl_shortcut_set_recording(host, c, 0);
    if (!sui_ctrl_shortcut_any_gamepad_recording(host))
        KillTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static int sui_ctrl_shortcut_apply_gamepad_buttons(SuiControlHost* host, uint32_t pressed) {
    int i, any = 0;
    if (!host) return 0;
    for (i = 0; i < host->control_count; ++i) {
        SuiControl* c = &host->controls[i];
        if (c->type != SUI_CTRL_SHORTCUT_RECORDER ||
            c->d.shortcut.kind != SUI_SHORTCUT_GAMEPAD ||
            !c->d.shortcut.recording) {
            continue;
        }
        any = 1;
        if (c->d.shortcut.initial_gamepad_buttons != 0) {
            if (pressed != 0 &&
                (pressed & c->d.shortcut.initial_gamepad_buttons) == c->d.shortcut.initial_gamepad_buttons) {
                continue;
            }
            c->d.shortcut.initial_gamepad_buttons = 0;
        }
        if (pressed == 0) {
            if (c->d.shortcut.candidate_gamepad_buttons != 0)
                sui_ctrl_shortcut_commit_gamepad(host, c, c->d.shortcut.candidate_gamepad_buttons);
            continue;
        }
        if (c->d.shortcut.candidate_gamepad_buttons == 0 ||
            sui_ctrl_shortcut_count_buttons(pressed) >= sui_ctrl_shortcut_count_buttons(c->d.shortcut.candidate_gamepad_buttons)) {
            if (pressed != c->d.shortcut.candidate_gamepad_buttons) {
                c->d.shortcut.candidate_gamepad_buttons = pressed;
                sui_ctrl_shortcut_format_gamepad(pressed, c->text, SUI_CTRL_TEXT_MAX);
                InvalidateRect(host->hwnd, NULL, FALSE);
            }
        }
    }
    return any;
}

static void sui_ctrl_shortcut_poll_gamepad_recorders(SuiControlHost* host) {
    uint32_t pressed;
    if (!host) return;
    if (!sui_ctrl_shortcut_any_gamepad_recording(host)) {
        KillTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID);
        return;
    }

    pressed = sui_ctrl_shortcut_read_gamepad_buttons();
    if (!sui_ctrl_shortcut_apply_gamepad_buttons(host, pressed))
        KillTimer(host->hwnd, SUI_SHORTCUT_RECORD_TIMER_ID);
}
