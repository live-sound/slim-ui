/* sui_shortcut_keys.h - keyboard shortcut formatting/capture helpers.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION.
 */

static const wchar_t* sui_shortcut_key_physical_label(UINT scan_code, int extended) {
    if (!extended) {
        switch (scan_code) {
        case 0x01: return L"Esc";
        case 0x02: return L"1";
        case 0x03: return L"2";
        case 0x04: return L"3";
        case 0x05: return L"4";
        case 0x06: return L"5";
        case 0x07: return L"6";
        case 0x08: return L"7";
        case 0x09: return L"8";
        case 0x0A: return L"9";
        case 0x0B: return L"0";
        case 0x0C: return L"-";
        case 0x0D: return L"=";
        case 0x0E: return L"Backspace";
        case 0x0F: return L"Tab";
        case 0x10: return L"Q";
        case 0x11: return L"W";
        case 0x12: return L"E";
        case 0x13: return L"R";
        case 0x14: return L"T";
        case 0x15: return L"Y";
        case 0x16: return L"U";
        case 0x17: return L"I";
        case 0x18: return L"O";
        case 0x19: return L"P";
        case 0x1A: return L"[";
        case 0x1B: return L"]";
        case 0x1C: return L"Enter";
        case 0x1E: return L"A";
        case 0x1F: return L"S";
        case 0x20: return L"D";
        case 0x21: return L"F";
        case 0x22: return L"G";
        case 0x23: return L"H";
        case 0x24: return L"J";
        case 0x25: return L"K";
        case 0x26: return L"L";
        case 0x27: return L";";
        case 0x28: return L"'";
        case 0x29: return L"`";
        case 0x2B: return L"\\";
        case 0x2C: return L"Z";
        case 0x2D: return L"X";
        case 0x2E: return L"C";
        case 0x2F: return L"V";
        case 0x30: return L"B";
        case 0x31: return L"N";
        case 0x32: return L"M";
        case 0x33: return L",";
        case 0x34: return L".";
        case 0x35: return L"/";
        case 0x39: return L"Space";
        case 0x3B: return L"F1";
        case 0x3C: return L"F2";
        case 0x3D: return L"F3";
        case 0x3E: return L"F4";
        case 0x3F: return L"F5";
        case 0x40: return L"F6";
        case 0x41: return L"F7";
        case 0x42: return L"F8";
        case 0x43: return L"F9";
        case 0x44: return L"F10";
        case 0x57: return L"F11";
        case 0x58: return L"F12";
        }
    } else {
        switch (scan_code) {
        case 0x1C: return L"Num Enter";
        case 0x35: return L"Num /";
        case 0x47: return L"Home";
        case 0x48: return L"Up";
        case 0x49: return L"PageUp";
        case 0x4B: return L"Left";
        case 0x4D: return L"Right";
        case 0x4F: return L"End";
        case 0x50: return L"Down";
        case 0x51: return L"PageDown";
        case 0x52: return L"Insert";
        case 0x53: return L"Delete";
        }
    }
    return NULL;
}

/* As RegisterHotKey reads them: AltGr is Ctrl+Alt. */
static UINT sui_shortcut_key_modifiers(void) {
    UINT mods = 0;
    if ((GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_LCONTROL) | GetAsyncKeyState(VK_RCONTROL)) & 0x8000)
        mods |= MOD_CONTROL;
    if ((GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_LMENU) | GetAsyncKeyState(VK_RMENU)) & 0x8000)
        mods |= MOD_ALT;
    if ((GetAsyncKeyState(VK_SHIFT) | GetAsyncKeyState(VK_LSHIFT) | GetAsyncKeyState(VK_RSHIFT)) & 0x8000)
        mods |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000)
        mods |= MOD_WIN;
    return mods;
}

static const wchar_t* sui_shortcut_mouse_label(UINT mouse_button) {
    switch (mouse_button) {
    case SUI_SHORTCUT_MOUSE_LEFT:   return L"MouseLeft";
    case SUI_SHORTCUT_MOUSE_RIGHT:  return L"MouseRight";
    case SUI_SHORTCUT_MOUSE_MIDDLE: return L"MouseMiddle";
    case SUI_SHORTCUT_MOUSE_X1:     return L"Mouse4";
    case SUI_SHORTCUT_MOUSE_X2:     return L"Mouse5";
    default:                        return NULL;
    }
}

static void sui_shortcut_key_format(UINT vk, UINT modifiers, UINT scan_code, int extended,
                                    UINT mouse_button, wchar_t* buf, int buf_len) {
    (void)buf_len;
    buf[0] = 0;
    if (vk == 0 && scan_code == 0 && mouse_button == SUI_SHORTCUT_MOUSE_NONE) {
        return;
    }
    if (modifiers & MOD_CONTROL) wcscat(buf, L"Ctrl+");
    if (modifiers & MOD_SHIFT)   wcscat(buf, L"Shift+");
    if (modifiers & MOD_ALT)     wcscat(buf, L"Alt+");
    if (modifiers & MOD_WIN)     wcscat(buf, L"Win+");
    if (mouse_button != SUI_SHORTCUT_MOUSE_NONE) {
        const wchar_t* mouse_label = sui_shortcut_mouse_label(mouse_button);
        if (mouse_label) {
            wcscat(buf, mouse_label);
        }
        return;
    }
    if (scan_code != 0) {
        const wchar_t* physical_label = sui_shortcut_key_physical_label(scan_code, extended);
        if (physical_label) {
            wcscat(buf, physical_label);
            return;
        }
    }
    {
        wchar_t keyName[64];
        LONG scanCode;
        keyName[0] = 0;
        scanCode = (LONG)(MapVirtualKeyW(vk, 0 /* MAPVK_VK_TO_VSC */) << 16);
        if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN ||
            vk == VK_HOME || vk == VK_END || vk == VK_INSERT || vk == VK_DELETE ||
            vk == VK_PRIOR || vk == VK_NEXT) {
            scanCode |= 0x01000000;
        }
        if (GetKeyNameTextW(scanCode, keyName, 64) > 0) {
            wcscat(buf, keyName);
        } else {
            wchar_t fallback[16];
            fallback[0] = L'V'; fallback[1] = L'K'; fallback[2] = L'_';
            fallback[3] = 0;
            {
                wchar_t digits[11];
                int i = 0, n = (int)vk, start;
                if (n == 0) { digits[i++] = L'0'; }
                while (n > 0 && i < 10) { digits[i++] = (wchar_t)(L'0' + (n % 10)); n /= 10; }
                digits[i] = 0;
                for (start = 0; start < i/2; start++) {
                    wchar_t t = digits[start];
                    digits[start] = digits[i - 1 - start];
                    digits[i - 1 - start] = t;
                }
                wcscat(fallback, digits);
            }
            wcscat(buf, fallback);
        }
    }
}

static void sui_shortcut_key_format_physical(UINT vk, UINT modifiers, UINT scan_code, int extended,
                                             wchar_t* buf, int buf_len) {
    sui_shortcut_key_format(vk, modifiers, scan_code, extended,
                            SUI_SHORTCUT_MOUSE_NONE, buf, buf_len);
}
