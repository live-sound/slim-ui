/* Slim UI Testbed — manual control gallery for visual QA.
 *
 * Build: run build.ps1 from this directory, or use the CMake project.
 * Run:   build\Release\sui_demo.exe
 *
 * Layout: left nav (sections) + right content area + bottom chrome bar.
 */

#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_FEATURE_PNG
#define SUI_FEATURE_DECL
#define SUI_FEATURE_VSYNC
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>

#include "shader_effect.h"

#define SUI_DEMO_LOGO_RCDATA 1001

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

/* ---- IDs ----------------------------------------------------------------- */
enum {
    ID_SHADER_IMAGE = 94,
    ID_SHADER_PAUSE = 95,
    ID_SHADER_SPEED = 96,
    ID_BTN_CLICK    = 101,
    ID_BTN_DEFAULT  = 102,
    ID_BTN_DISABLED = 103,
    ID_TOOLTIP_INFO = 104,
    ID_TOOLTIP_BUTTON = 105,
    ID_CONTAINER_TABS = 106,
    ID_CHK_A        = 111,
    ID_CHK_B        = 112,
    ID_RADIO_1      = 121,
    ID_RADIO_2      = 122,
    ID_RADIO_3      = 123,
    ID_EDIT         = 131,
    ID_HOTKEY       = 141,
    ID_GAMEPAD_SHORTCUT = 142,
    ID_COMBO_SHORT  = 151,
    ID_COMBO_LONG   = 152,
    ID_SLIDER       = 161,
    ID_SPINNER      = 171,
    ID_SWATCH       = 181,
    ID_COMBO_SCHEME = 192,
    ID_COMBO_TYPO   = 193,
    ID_LBL_SCHEME   = 194,
    ID_LBL_TYPO     = 195,
    ID_PROGRESS     = 201,
    ID_TEXTVIEW     = 221,
    ID_IMAGE        = 231,
    ID_NAV_CHROME   = 240,  /* left chrome nav (section selector) */
    ID_NAV_INNER    = 241,  /* nav_list inside Containers section demo */
    ID_CONTAINER    = 234,
    ID_SCROLLVIEW_DEMO = 251,
    ID_VSTACK_DEMO     = 252,
    ID_HSTACK_DEMO     = 253,
    ID_FLOW_DEMO       = 254,
    ID_BTN_HS_1        = 261,
    ID_BTN_HS_2        = 262,
    ID_BTN_HS_3        = 263,
    /* Flex section */
    ID_FLEX_HG_L       = 271,
    ID_FLEX_HG_M       = 272,
    ID_FLEX_HG_R       = 273,
    ID_FLEX_HM_1       = 274,
    ID_FLEX_HM_2       = 275,
    ID_FLEX_HM_3       = 276,
    ID_FLEX_HS_1       = 277,
    ID_FLEX_HS_2       = 278,
    ID_FLEX_HS_3       = 279,
    ID_FLEX_V_BODY     = 280,
    /* Declarative containers section */
    ID_DECL_SV     = 290,
    ID_DECL_CLIP   = 291,
    ID_DECL_BTN_1  = 292,
    ID_DECL_BTN_2  = 293,
    ID_DECL_BTN_3  = 294,
};

#define DEMO_TEXTVIEW_CAP 4096

/* Section indexes — drive content build */
enum {
    SEC_BASIC           = 0,
    SEC_CHOICES         = 1,
    SEC_TEXT_INPUT      = 2,
    SEC_CONTAINERS      = 3,
    SEC_FLEX            = 4,
    SEC_DECL_CONTAINERS = 5,
    SEC_SHADER          = 6,
    SEC_COUNT           = 7,
};

static const wchar_t* kDemoTextviewSeed =
    L"Multi-line text view with soft-wrap.\n\n"
    L"Try Shift+arrows to select, Ctrl+C/V/X for clipboard, Ctrl+Z to undo, "
    L"Ctrl+A to select all. Home/End/PageUp/PageDown navigate within the wrapped "
    L"layout. Right-click for the context menu.\n\n"
    L"Toggle the readonly attribute via sui_ctrl_textview_set_readonly() at runtime.";

/* ---- App-defined custom typography (demo: serif Cambria, larger body) --- */
static const SuiTypography demo_serif_typo = {
    /* body */     { L"Cambria",            13.0f, 400, 0 },
    /* subtitle */ { L"Cambria",            13.0f, 600, 0 },
    /* title */    { L"Cambria",            26.0f, 700, 0 },
    /* icon */     { L"Segoe Fluent Icons", 13.0f, 400, 0 },
};

/* ---- App-defined custom scheme (demo: dark mode with blue accent) -------- */
static const SuiColorScheme demo_blue_dark = {
    0x1E1E1E, 0xE0E0E0, 0x999999,
    0x3A3A3A, 0x555555, 0xE0E0E0, 0x444444, 0x333333, 0x4CC2FF,
    0x999999, 0x4CC2FF, 0xFFFFFF,
    0x2D2D2D, 0x555555, 0xE0E0E0, 0x4CC2FF, 0x264F78, 0x666666,
    0x555555, 0x4CC2FF, 0x4CC2FF,
    0x2A2A2A, 0x262626, 0x3A3A3A, 0x3D3D3D,
    0x404040, 0x5CBF60,
    0x2D2D2D, 0x3D3D3D,
};

/* ---- State --------------------------------------------------------------- */
struct TestbedData {
    SuiRenderer    renderer;
    SuiControlHost host;
    SuiColorScheme scheme;
    char           scheme_name[SUI_SCHEME_NAME_MAX] = "light";
    char           typo_name[SUI_SCHEME_NAME_MAX]   = "default";
    UINT           dpi    = 96;
    int            section = SEC_BASIC;
    int            container_tab = 0;
    SuiImageHandle logo    = {};
    DemoShader*    shader = nullptr;
    float          shader_seconds = 0.0f;
    float          shader_speed = 1.0f;
    int            shader_paused = 0;
    int            shader_running = 0;
};

/* Pull the embedded logo PNG out of the exe's resources. The .rc file points at
 * slim-ui/logo.png, the bitmap version of logo.svg. */
static SuiImageHandle LoadLogoFromResource(SuiRenderer* r) {
    HMODULE m = GetModuleHandleW(NULL);
    HRSRC   h = FindResourceW(m, MAKEINTRESOURCEW(SUI_DEMO_LOGO_RCDATA), RT_RCDATA);
    if (!h) return SuiImageHandle{};
    DWORD   sz = SizeofResource(m, h);
    HGLOBAL g  = LoadResource(m, h);
    if (!g || sz == 0) return SuiImageHandle{};
    const uint8_t* p = (const uint8_t*)LockResource(g);
    if (!p) return SuiImageHandle{};
    return sui_renderer_load_image(r, p, (size_t)sz);
}

/* ---- DPI helper ---------------------------------------------------------- */
static UINT GetDpiForWnd(HWND hwnd) {
    typedef UINT(WINAPI* Fn)(HWND);
    static auto fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && hwnd) return fn(hwnd);
    HDC dc = GetDC(hwnd);
    UINT d = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(hwnd, dc);
    return d ? d : 96;
}

/* Snapshot of editable controls. Captured before BuildControls() rebuilds the
 * host, restored after. Without this, every WM_SIZE / scheme change would wipe
 * what the user typed. */
struct EditSnapshot {
    wchar_t  edit_text[256];
    int      edit_cursor;
    wchar_t  tv_text[DEMO_TEXTVIEW_CAP];
    int      tv_cursor;
    int      tv_sel_start;
    int      tv_sel_end;
    float    tv_scroll;
    int      tv_valid;
    uint32_t swatch_color;
    int      swatch_valid;
};

static void SaveEdits(SuiControlHost* h, EditSnapshot* s) {
    s->edit_text[0] = L'\0';
    s->edit_cursor  = 0;
    s->tv_text[0]   = L'\0';
    s->tv_cursor    = 0;
    s->tv_sel_start = -1;
    s->tv_sel_end   = -1;
    s->tv_scroll    = 0.0f;
    s->tv_valid     = 0;
    s->swatch_color = 0;
    s->swatch_valid = 0;

    SuiControl* e = sui_ctrl_find(h, ID_EDIT);
    if (e) {
        sui__wstrncpy_z(s->edit_text, e->text,
                        sizeof(s->edit_text) / sizeof(s->edit_text[0]));
        s->edit_cursor = e->d.edit.cursor;
    }
    SuiControl* sw = sui_ctrl_find(h, ID_SWATCH);
    if (sw) { s->swatch_color = sw->d.swatch.color; s->swatch_valid = 1; }

    SuiControl* t = sui_ctrl_find(h, ID_TEXTVIEW);
    if (t && t->type == SUI_CTRL_TEXTVIEW && t->d.textview.buf) {
        int n = t->d.textview.len;
        if (n > DEMO_TEXTVIEW_CAP - 1) n = DEMO_TEXTVIEW_CAP - 1;
        if (n > 0) memcpy(s->tv_text, t->d.textview.buf, (size_t)n * sizeof(wchar_t));
        s->tv_text[n]   = L'\0';
        s->tv_cursor    = t->d.textview.cursor;
        s->tv_sel_start = t->d.textview.sel_start;
        s->tv_sel_end   = t->d.textview.sel_end;
        s->tv_scroll    = t->d.textview.scroll_offset;
        s->tv_valid     = 1;
    }
}

static void RestoreEdits(SuiControlHost* h, const EditSnapshot* s) {
    SuiControl* e = sui_ctrl_find(h, ID_EDIT);
    if (e && s->edit_text[0]) {
        sui__wstrncpy_z(e->text, s->edit_text, SUI_CTRL_TEXT_MAX);
        e->d.edit.cursor = s->edit_cursor;
        e->d.edit.sel_start = -1;
        e->d.edit.sel_end = -1;
        sui_ctrl_edit_normalize_indices(e);
        e->d.edit.drag_sel_anchor = e->d.edit.cursor;
    }
    SuiControl* sw = sui_ctrl_find(h, ID_SWATCH);
    if (sw && s->swatch_valid) sw->d.swatch.color = s->swatch_color;
    if (s->tv_valid) {
        SuiControl* t = sui_ctrl_find(h, ID_TEXTVIEW);
        if (t && t->type == SUI_CTRL_TEXTVIEW) {
            sui_ctrl_textview_set_text(h, ID_TEXTVIEW, s->tv_text);
            if (s->tv_cursor >= 0 && s->tv_cursor <= t->d.textview.len)
                t->d.textview.cursor = s->tv_cursor;
            t->d.textview.sel_start    = s->tv_sel_start;
            t->d.textview.sel_end      = s->tv_sel_end;
            t->d.textview.scroll_offset = s->tv_scroll;
        }
    }
}

/* ---- Section builders (imperative, absolute coords inside content rect) -- */

/* Layout helper: emit "label + control" pair stacked vertically.
 * Returns y after the control + a vertical gap. */
struct SectionLayout {
    float x;
    float y;       /* current cursor */
    float w;       /* content column width */
    float s;       /* dpi scale */
    float lh;      /* label height */
    float ch;      /* control height (baseline) */
    float lbl_gap; /* label-to-control */
    float row_gap; /* between rows */
};

static SectionLayout MakeLayout(const TestbedData* d, float x, float y, float w) {
    SectionLayout L;
    L.x       = x;
    L.y       = y;
    L.w       = w;
    L.s       = (float)d->dpi / 96.0f;
    L.lh      = 18.0f * L.s;
    L.ch      = 28.0f * L.s;
    L.lbl_gap =  4.0f * L.s;
    L.row_gap = 14.0f * L.s;
    return L;
}

static void EmitLabel(SuiControlHost* h, SectionLayout* L, const wchar_t* text) {
    sui_ctrl_add_label(h, -1, L->x, L->y, L->w, L->lh, text);
    L->y += L->lh + L->lbl_gap;
}

static void AdvanceRow(SectionLayout* L, float control_h) {
    L->y += control_h + L->row_gap;
}

/* Big bold section heading — uses SUI_TITLE_PT and SUI_CTRL_BOLD so the
 * renderer picks the title typography role (Cambria 700 in the serif demo,
 * Segoe UI Bold otherwise). */
static void EmitSectionTitle(SuiControlHost* h, SectionLayout* L, const wchar_t* text) {
    const float title_h = 36.0f * L->s;
    int idx = sui_ctrl_add_label(h, -1, L->x, L->y, L->w, title_h, text);
    if (idx >= 0) {
        h->controls[idx].font_size = SUI_TITLE_PT;
        h->controls[idx].state    |= SUI_CTRL_BOLD;
    }
    L->y += title_h + L->row_gap;
}

/* ---- Section: Basic controls (label, buttons, progress) ----------------- */
static void BuildSectionBasic(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)ch_total; (void)cy;

    EmitSectionTitle(h, &L, L"Basic controls");

    /* Plain label (single line) */
    EmitLabel(h, &L, L"Label");
    sui_ctrl_add_label(h, -1, L.x, L.y, L.w, L.ch,
        L"Single-line label — picks up scheme.text color.");
    AdvanceRow(&L, L.ch);

    /* Buttons — pin min_w to declared btn_w so the row scan reflects the
     * fixed visual width, not the (smaller) text-derived intrinsic. Phase 1
     * verification probe: removing these min_w lines should re-clip "Disabled"
     * at narrow window sizes. */
    EmitLabel(h, &L, L"Buttons");
    const float btn_w = 110.0f * L.s;
    const float btn_gap = 8.0f * L.s;
    {
        sui_ctrl_add_button(h, ID_BTN_CLICK,    L.x + (btn_w + btn_gap) * 0.0f, L.y, btn_w, L.ch, L"Click me", 0);
        sui_ctrl_add_button(h, ID_BTN_DEFAULT,  L.x + (btn_w + btn_gap) * 1.0f, L.y, btn_w, L.ch, L"Default",  1);
        sui_ctrl_add_button(h, ID_BTN_DISABLED, L.x + (btn_w + btn_gap) * 2.0f, L.y, btn_w, L.ch, L"Disabled", 0);
        SuiControl* bc = sui_ctrl_find(h, ID_BTN_CLICK);    if (bc) { bc->min_w = btn_w; }
        SuiControl* bd = sui_ctrl_find(h, ID_BTN_DEFAULT);  if (bd) { bd->min_w = btn_w; }
        SuiControl* dis = sui_ctrl_find(h, ID_BTN_DISABLED);
        if (dis) { dis->min_w = btn_w; dis->state |= SUI_CTRL_DISABLED; }
    }
    AdvanceRow(&L, L.ch);

    EmitLabel(h, &L, L"Tooltips");
    {
        const float tip_size = SUI_DEFAULT_TOOLTIP_SIZE * L.s;
        sui_ctrl_add_tooltip(h, ID_TOOLTIP_INFO, L.x, L.y + 5.0f * L.s, tip_size, tip_size,
            L"A compact tooltip control for settings rows. Hover targets can be standalone or attached to any control.");
        sui_ctrl_add_label(h, -1, L.x + 28.0f * L.s, L.y, L.w - 28.0f * L.s, L.ch,
            L"Hover the ? icon or the button.");
    }
    AdvanceRow(&L, L.ch);

    {
        const float tip_btn_w = 160.0f * L.s;
        sui_ctrl_add_button(h, ID_TOOLTIP_BUTTON, L.x, L.y, tip_btn_w, L.ch, L"Button tooltip", 0);
        SuiControl* tb = sui_ctrl_find(h, ID_TOOLTIP_BUTTON);
        if (tb) { tb->min_w = tip_btn_w; }
        sui_ctrl_set_tooltip(h, ID_TOOLTIP_BUTTON,
            L"This tooltip is attached directly to the button via SuiControl.tooltip.");
    }
    AdvanceRow(&L, L.ch);

    /* Progress bar — 22 DIP high, as in an installer wizard. */
    EmitLabel(h, &L, L"Progress bar");
    {
        const float bar_h = 22.0f * L.s;
        sui_ctrl_add_progress(h, ID_PROGRESS, L.x, L.y, L.w, bar_h, 0.0f, 100.0f, 40.0f);
        AdvanceRow(&L, bar_h);
    }

    /* Slider — drives the progress bar above */
    EmitLabel(h, &L, L"Slider");
    sui_ctrl_add_slider_p(h, ID_SLIDER, L.x, L.y, L.w, L.ch,
        SuiSliderParams{ .min=0.0f, .max=100.0f, .value=40.0f, .step=1.0f });
    AdvanceRow(&L, L.ch);
}

/* ---- Section: Choices --------------------------------------------------- */
static void BuildSectionChoices(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)ch_total; (void)cy;

    static const wchar_t* short_items[] = { L"Red", L"Green", L"Blue", L"Yellow" };
    static const wchar_t* long_items[]  = {
        L"Aegean",           L"American Bobtail",   L"American Curl",
        L"American Ringtail",L"American Shorthair", L"American Wirehair",
        L"Aphrodite Giant",  L"Arabian Mau",        L"Asian cat",
        L"Asian Semi-longhair", L"Australian Mist", L"Bambino",
        L"Bengal",           L"Birman",             L"British Shorthair",
    };

    EmitSectionTitle(h, &L, L"Choices");

    /* Two columns: left = checkbox + radio, right = comboboxes + slider + spinner + swatch */
    const float col_gap   = 24.0f * L.s;
    const float col_w     = (L.w - col_gap) * 0.5f;
    const float col_left  = L.x;
    const float col_right = L.x + col_w + col_gap;
    float yL = L.y;
    float yR = L.y;

    /* Checkboxes */
    sui_ctrl_add_label(h, -1, col_left, yL, col_w, L.lh, L"Checkboxes");
    yL += L.lh + L.lbl_gap;
    sui_ctrl_add_checkbox(h, ID_CHK_A, col_left, yL,           col_w, L.ch, L"Option A",           0);
    yL += L.ch + 2.0f * L.s;
    sui_ctrl_add_checkbox(h, ID_CHK_B, col_left, yL,           col_w, L.ch, L"Option B (checked)", 1);
    yL += L.ch + L.row_gap;

    /* Radio group */
    sui_ctrl_add_label(h, -1, col_left, yL, col_w, L.lh, L"Radio group");
    yL += L.lh + L.lbl_gap;
    sui_ctrl_add_radio(h, ID_RADIO_1, col_left, yL, col_w, L.ch, L"Alpha", 1, 1); yL += L.ch + 2.0f * L.s;
    sui_ctrl_add_radio(h, ID_RADIO_2, col_left, yL, col_w, L.ch, L"Beta",  1, 0); yL += L.ch + 2.0f * L.s;
    sui_ctrl_add_radio(h, ID_RADIO_3, col_left, yL, col_w, L.ch, L"Gamma", 1, 0); yL += L.ch + L.row_gap;

    /* Comboboxes */
    sui_ctrl_add_label(h, -1, col_right, yR, col_w, L.lh, L"Combobox");
    yR += L.lh + L.lbl_gap;
    sui_ctrl_add_combobox(h, ID_COMBO_SHORT, col_right, yR, col_w, L.ch, short_items, 4, 1);
    yR += L.ch + L.row_gap;

    sui_ctrl_add_label(h, -1, col_right, yR, col_w, L.lh, L"Combobox (long — scroll)");
    yR += L.lh + L.lbl_gap;
    sui_ctrl_add_combobox(h, ID_COMBO_LONG, col_right, yR, col_w, L.ch, long_items, 15, 7);
    sui_ctrl_combo_set_popup_width(h, ID_COMBO_LONG, col_w * 1.5f);
    yR += L.ch + L.row_gap;

    /* Swatch — pin min_w to declared visual width (140dp): swatch has no
     * intrinsic, so without a pin the row scan would let it collapse to 0
     * and the window could shrink past the Choices column floor. */
    sui_ctrl_add_label(h, -1, col_right, yR, col_w, L.lh, L"🎨 Color swatch");
    yR += L.lh + L.lbl_gap;
    {
        const float sw_w = 140.0f * L.s;
        int sid = sui_ctrl_add_color_swatch(h, ID_SWATCH, col_right, yR, sw_w, L.ch, 0xFF0078D4);
        SuiControl* sc = (sid >= 0) ? &h->controls[sid] : NULL;
        if (sc) { sc->min_w = sw_w; }
    }
    yR += L.ch;

    /* Symmetric 2-col floor: the generic FLEX_ROWS slice scan groups by
     * Y-overlap and so misses cross-band column intrinsics — at the floor
     * cw, the layout forces col_w = (L.w - col_gap)/2 which can still be
     * below an individual column control's text intrinsic, clipping labels.
     * Pin min_client_w to the cw that makes col_w == max column ew. Skip
     * the section title (its w == L.w spans both columns) and any nav-area
     * controls (x < section x). Pad-detection adds the right pad on top. */
    {
        float max_col_ew = 0.0f;
        int i;
        for (i = 0; i < h->control_count; i++) {
            SuiControl* c = &h->controls[i];
            float ew = 0.0f, eh = 0.0f;
            if (c->parent_id != -1) continue;
            if (c->state & SUI_CTRL_HIDDEN) continue;
            if (c->x < cx - 0.5f) continue;
            if (c->w > col_w + 0.5f) continue;
            sui_ctrl_effective_size(h, c, &ew, &eh, 0);
            if (ew > max_col_ew) max_col_ew = ew;
        }
        h->min_client_w = cx + 2.0f * max_col_ew + col_gap;
    }
}

/* ---- Section: Text input ------------------------------------------------ */
static void BuildSectionTextInput(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    L.row_gap = 8.0f * L.s;

    EmitSectionTitle(h, &L, L"Text input");

    /* Edit */
    EmitLabel(h, &L, L"Edit");
    sui_ctrl_add_edit(h, ID_EDIT, L.x, L.y, L.w, L.ch, L"", L"Type something…", 0);
    AdvanceRow(&L, L.ch);

    /* Shortcut recorders */
    {
        const float col_gap = 12.0f * L.s;
        const float col_w = (L.w - col_gap) * 0.5f;
        const float right_x = L.x + col_w + col_gap;

        sui_ctrl_add_label(h, -1, L.x, L.y, col_w, L.lh, L"Keyboard shortcut");
        sui_ctrl_add_label(h, -1, right_x, L.y, col_w, L.lh, L"Gamepad shortcut");
        L.y += L.lh + L.lbl_gap;

        sui_ctrl_add_shortcut_recorder_keyboard(h, ID_HOTKEY, L.x, L.y, col_w, L.ch,
            VK_F2, 0, L"Press a key combination...");
        sui_ctrl_add_shortcut_recorder_gamepad(h, ID_GAMEPAD_SHORTCUT, right_x, L.y, col_w, L.ch,
            SUI_GAMEPAD_BACK | SUI_GAMEPAD_X,
            L"Press gamepad buttons...");
        AdvanceRow(&L, L.ch);
    }

    /* Spinner — numeric edit */
    EmitLabel(h, &L, L"Spinner");
    sui_ctrl_add_spinner_p(h, ID_SPINNER, L.x, L.y, 120.0f * L.s, L.ch,
        SuiSpinnerParams{ .min=0.0f, .max=999.0f, .value=42.0f, .step=1.0f });
    sui_ctrl_set_tooltip(h, ID_SPINNER, L"Hold an arrow to keep changing the value.");
    AdvanceRow(&L, L.ch);

    /* Textview — fills remaining space */
    EmitLabel(h, &L, L"Textview (multi-line, soft-wrap)");
    const float tv_min = 80.0f * L.s;
    {
        const float section_bottom = cy + ch_total;
        float tv_h = section_bottom - L.y;
        if (tv_h < tv_min) tv_h = tv_min;
        sui_ctrl_add_textview(h, ID_TEXTVIEW, L.x, L.y, L.w, tv_h,
            kDemoTextviewSeed, DEMO_TEXTVIEW_CAP, /*readonly=*/0);
        h->min_client_h = L.y + tv_min + L.lh + L.lbl_gap + L.ch + 24.0f * L.s;
    }
}

/* ---- Section: Shader ---------------------------------------------------- */
static void ShaderFrame(void* user, float dt_ms) {
    TestbedData* d = static_cast<TestbedData*>(user);
    d->shader_seconds += dt_ms * 0.001f * d->shader_speed;
    InvalidateRect(d->host.hwnd, NULL, FALSE);
}

/* The shader only runs while someone can see it, so it costs nothing on other pages or when minimized. */
static void UpdateShaderClock(TestbedData* d) {
    int wanted = d->section == SEC_SHADER && !d->shader_paused && !IsIconic(d->host.hwnd);
    if (wanted == d->shader_running) return;
    if (wanted) {
        d->shader_running = sui_vsync_attach(&d->host.vsync, ShaderFrame, d);
    } else {
        sui_vsync_detach(&d->host.vsync, ShaderFrame, d);
        d->shader_running = 0;
    }
}

/* A control draws itself into a bitmap the usual way, and its shader runs on the way to the screen. */
static void PaintWithShader(SuiControlHost* host, SuiControl* c, void* user) {
    TestbedData* d = static_cast<TestbedData*>(user);
    const int kind = c->type == SUI_CTRL_IMAGE ? DEMO_SHADER_RIPPLE : DEMO_SHADER_GLOW;
    /* The glow reaches past the edges of the text input. */
    const float margin = kind == DEMO_SHADER_GLOW ? (float)(int)(14.0f * (float)d->dpi / 96.0f) : 0.0f;
    void* screen = d->renderer.render_target;

    if (!d->shader) d->shader = demo_shader_create();
    void* bitmap = demo_shader_begin(d->shader, screen, kind, c->x, c->y, c->w, c->h, margin);
    if (!bitmap) {
        sui_ctrl_draw_self(host, c);
        return;
    }
    d->renderer.render_target = bitmap;
    sui__apply_text_style(&d->renderer, (ID2D1RenderTarget*)bitmap);
    sui_ctrl_draw_self(host, c);
    d->renderer.render_target = screen;
    demo_shader_end(d->shader, kind, d->shader_seconds);
}

static void BuildSectionShader(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)cy; (void)ch_total;

    EmitSectionTitle(h, &L, L"Shader");
    EmitLabel(h, &L, L"These controls draw as usual, then a pixel shader runs over their pixels.");
    L.y += L.row_gap - L.lbl_gap;

    /* Whole pixels, so the bitmaps a control draws into line up with the screen. */
    const float x = (float)(int)L.x;
    float image_w = 300.0f * L.s;
    float image_h = d->logo.width > 0 ? image_w * (float)d->logo.height / (float)d->logo.width : 140.0f * L.s;
    image_w = (float)(int)image_w;
    image_h = (float)(int)image_h;
    sui_ctrl_add_image(h, ID_SHADER_IMAGE, x, (float)(int)L.y, image_w, image_h, d->logo, /*circle_mask=*/0);
    sui_ctrl_set_paint(h, ID_SHADER_IMAGE, PaintWithShader, d);
    AdvanceRow(&L, image_h);

    /* Sharing the Text input page's ID keeps what's typed here through a rebuild. */
    EmitLabel(h, &L, L"Text input");
    sui_ctrl_add_edit(h, ID_EDIT, x, (float)(int)L.y, (float)(int)L.w, (float)(int)L.ch, L"", L"Type something\x2026", 0);
    sui_ctrl_set_paint(h, ID_EDIT, PaintWithShader, d);
    L.y += L.ch + L.row_gap * 1.5f;

    const float button_w = 110.0f * L.s, gap = 16.0f * L.s;
    sui_ctrl_add_button(h, ID_SHADER_PAUSE, L.x, L.y, button_w, L.ch, d->shader_paused ? L"Resume" : L"Pause", 0);
    sui_ctrl_add_slider_p(h, ID_SHADER_SPEED, L.x + button_w + gap, L.y, L.w - button_w - gap, L.ch,
        SuiSliderParams{ .min=0.0f, .max=300.0f, .value=d->shader_speed * 100.0f, .step=1.0f });
}

/* ---- Section: Containers ------------------------------------------------ */
static void BuildSectionContainers(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)cy;

    EmitSectionTitle(h, &L, L"Containers");

    /* Scrollview spans the rest of the section. Three sub-containers
     * (vertical stack, horizontal stack, flow) are parented inside it. */
    const float section_bottom = cy + ch_total;
    const float sv_y  = L.y;
    /* Match the toolkit's SCROLLVIEW intrinsic-min-h (sui_ctrl_intrinsic_size
     * returns 120*s). If sv_min > toolkit floor, the layout clamps higher
     * than the WM_GETMINMAXINFO floor allows and SV overlaps the bar gap
     * at min height. */
    const float sv_min = 120.0f * L.s;
    float sv_h = section_bottom - L.y;
    if (sv_h < sv_min) sv_h = sv_min;

    sui_ctrl_add_scrollview(h, ID_SCROLLVIEW_DEMO, L.x, L.y, L.w, sv_h, /*content_h=*/0.0f);

    auto parent_to = [&](int idx, int parent) {
        if (idx >= 0) h->controls[idx].parent_id = parent;
    };

    /* Inside-scrollview layout. Reserve right gutter for the scrollbar.
     * Match SUI_SCROLLVIEW_SB_HOT_W (12dp) so sui_ctrl_min_size's reservation
     * for SCROLLVIEW children agrees with the layout here. */
    const float gutter   = 12.0f * L.s;
    const float pad_left =  8.0f * L.s;
    const float child_x  = L.x + pad_left;
    const float child_w  = L.w - pad_left - gutter;
    float yC = L.y + 8.0f * L.s;
    const float subsection_title_h   = 22.0f * L.s;
    const float subsection_title_gap =  8.0f * L.s;
    const float subsection_gap       = 24.0f * L.s;

    auto emit_subsection_title = [&](const wchar_t* text) {
        int idx = sui_ctrl_add_label(h, -1, child_x, yC, child_w, subsection_title_h, text);
        if (idx >= 0) {
            h->controls[idx].font_size = 13.0f;
            h->controls[idx].state |= SUI_CTRL_BOLD;
        }
        parent_to(idx, ID_SCROLLVIEW_DEMO);
        yC += subsection_title_h + subsection_title_gap;
    };

    /* --- Tabs: tab strip with content hosted below it --- */
    emit_subsection_title(L"Tabs");
    {
        const wchar_t* tab_items[] = { L"Overview", L"Details", L"Output" };
        const float tabs_h = 118.0f * L.s;
        const float tab_h = SUI_DEFAULT_TAB_H * L.s;
        int idx = sui_ctrl_add_tabs(h, ID_CONTAINER_TABS, child_x, yC, child_w, tabs_h,
                                    tab_h, d->container_tab);
        SuiControl* tabs = (idx >= 0) ? &h->controls[idx] : NULL;
        SuiRect pane;
        parent_to(idx, ID_SCROLLVIEW_DEMO);
        sui_ctrl_tabs_set_spacing(h, ID_CONTAINER_TABS, 6.0f * L.s, 8.0f * L.s);
        for (int i = 0; i < 3; ++i)
            sui_ctrl_tabs_add_item(h, ID_CONTAINER_TABS, tab_items[i]);
        if (tabs && sui_ctrl_tabs_content_rect(tabs, &pane)) {
            const float px = pane.left + 14.0f * L.s;
            const float py = pane.top + 12.0f * L.s;
            const float pw = (pane.right - pane.left) - 28.0f * L.s;
            const wchar_t* line1 = L"Snapshot";
            const wchar_t* line2 = L"Three child controls are clipped to the selected tab pane.";
            if (d->container_tab == 1) {
                line1 = L"Container details";
                line2 = L"Tab content is parented to the tab control and drawn below the strip.";
            } else if (d->container_tab == 2) {
                line1 = L"Output";
                line2 = L"The panel uses the view background while tabs use nav highlights.";
            }
            int l1 = sui_ctrl_add_label(h, -1, px, py, pw, L.lh, line1);
            int l2 = sui_ctrl_add_label(h, -1, px, py + L.lh + 8.0f * L.s, pw, L.lh, line2);
            parent_to(l1, ID_CONTAINER_TABS);
            parent_to(l2, ID_CONTAINER_TABS);
        }
        yC += tabs_h + subsection_gap;
    }

    /* --- Vertical stack: 3 labels --- */
    emit_subsection_title(L"Vertical stack");
    {
        const float row_h    = L.ch + 4.0f * L.s;
        const float vstack_h = row_h * 3.0f;
        parent_to(sui_ctrl_add_container(h, ID_VSTACK_DEMO, child_x, yC, child_w, vstack_h),
                  ID_SCROLLVIEW_DEMO);
        const wchar_t* texts[3] = { L"First label", L"Second label", L"Third label" };
        float yV = yC;
        for (int i = 0; i < 3; ++i) {
            parent_to(sui_ctrl_add_label(h, -1, child_x, yV, child_w, L.ch, texts[i]),
                      ID_VSTACK_DEMO);
            yV += row_h;
        }
        yC += vstack_h + subsection_gap;
    }

    /* --- Horizontal stack: 3 buttons --- */
    emit_subsection_title(L"Horizontal stack");
    const float hstack_btn_w = 100.0f * L.s;
    const float hstack_gap   =   8.0f * L.s;
    const float hstack_w     = 3.0f * hstack_btn_w + 2.0f * hstack_gap;
    {
        const float hstack_bleed = 1.0f * L.s;
        parent_to(sui_ctrl_add_container(h, ID_HSTACK_DEMO,
                  child_x - hstack_bleed, yC - hstack_bleed,
                  child_w + hstack_bleed * 2.0f, L.ch + hstack_bleed * 2.0f),
                  ID_SCROLLVIEW_DEMO);
        const wchar_t* labels[3] = { L"One", L"Two", L"Three" };
        const int ids[3] = { ID_BTN_HS_1, ID_BTN_HS_2, ID_BTN_HS_3 };
        for (int i = 0; i < 3; ++i) {
            int bid = sui_ctrl_add_button(h, ids[i],
                child_x + (hstack_btn_w + hstack_gap) * (float)i, yC, hstack_btn_w, L.ch, labels[i], 0);
            parent_to(bid, ID_HSTACK_DEMO);
            /* Pin min_w to declared visual width: button intrinsic is
             * text-derived (~50dp for "One"/"Two"/"Three") and would let
             * the row scan underreport, so the rightmost button overruns
             * the container at narrow widths. */
            SuiControl* bc = (bid >= 0) ? &h->controls[bid] : NULL;
            if (bc) { bc->min_w = hstack_btn_w; }
        }
        yC += L.ch + subsection_gap;
    }

    /* --- Flow: 8 images, wrap into rows by available width --- */
    emit_subsection_title(L"Flow");
    {
        const float img_h    = 56.0f * L.s;
        const float img_gap  =  8.0f * L.s;
        /* Match the logo's native aspect ratio so the default FILL mode
         * draws it 1:1 instead of squishing it into a square. */
        float img_w = img_h;
        if (d->logo.width > 0 && d->logo.height > 0) {
            img_w = img_h * ((float)d->logo.width / (float)d->logo.height);
        }
        int per_row = (int)((child_w + img_gap) / (img_w + img_gap));
        if (per_row < 1) per_row = 1;
        const int count = 8;
        const int rows  = (count + per_row - 1) / per_row;
        const float flow_h = (float)rows * img_h + (float)(rows - 1) * img_gap;
        int flow_idx = sui_ctrl_add_container(h, ID_FLOW_DEMO, child_x, yC, child_w, flow_h);
        parent_to(flow_idx, ID_SCROLLVIEW_DEMO);
        /* Flow-wrap container: row min reports max(child_w) instead of
         * summing all 8 image positions across one Y band. The build
         * code above already recomputes per_row/rows on every WM_SIZE,
         * so the column count shrinks naturally as the window narrows. */
        if (flow_idx >= 0) h->controls[flow_idx].state |= SUI_CTRL_FLOW;
        for (int i = 0; i < count; ++i) {
            const int row = i / per_row;
            const int col = i % per_row;
            const float ix = child_x + (img_w + img_gap) * (float)col;
            const float iy = yC      + (img_h + img_gap) * (float)row;
            int iidx = sui_ctrl_add_image(h, -1, ix, iy, img_w, img_h,
                d->logo, /*circle_mask=*/0);
            parent_to(iidx, ID_FLOW_DEMO);
            /* Pin each image's min size so FLOW's max-of-children scan
             * sees a real cell width. Without it the image atom's
             * intrinsic is 0 and the container reports row min = 0. */
            SuiControl* ic = (iidx >= 0) ? &h->controls[iidx] : NULL;
            if (ic) { ic->min_w = img_w; ic->min_h = img_h; }
        }
        yC += flow_h + subsection_gap;
    }

    /* Total content height — tells the scrollview when to show its scrollbar. */
    sui_ctrl_set_scrollview_content_height(h, ID_SCROLLVIEW_DEMO, yC - L.y);
    (void)hstack_w; (void)sv_y; (void)sv_min;
}

/* ---- Section: Flex ------------------------------------------------------ */
/* Exercises grow / shrink / pref / max via the declarative API and the
 * sui_last_* hint helpers. Three demos:
 *   1. HSTACK with one growing middle button (sides have explicit pref_w).
 *   2. HSTACK with three grow=1 buttons each capped at max_w (cap stops growth).
 *   3. VSTACK with grow=1 on the middle row (textview fills remaining height).
 *
 * BuildControls already populated nav + sets up bottom bar after this returns,
 * so we cannot call sui_begin (it would wipe host->control_count). Instead we
 * push two ROOT containers manually, bracketing the horizontal demos and the
 * vertical demo. The vertical demo's ROOT carries an explicit `h` so its
 * inner VSTACK inherits a non-zero height (sui__decl_distribute_vstack
 * requires v->h > 0). */
static void BuildSectionFlex(TestbedData* d, float cx, float cy, float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)cy;

    EmitSectionTitle(h, &L, L"Flex");

    /* ── Horizontal demos: ROOT h=0 (vertical distribute won't fire). */
    SuiRect r1_px = {L.x, L.y, L.x + L.w, L.y};  /* bottom == top → h=0 */
    SuiContainer* root1 = sui_begin_px(h, r1_px, L.row_gap, SUI_CLEAR_NONE);
    if (!root1) return;

    sui_label(h, L"Horizontal grow — middle button absorbs leftover width:");
    sui_hstack(h, 3, 8.0f); {
        sui_button(h, ID_FLEX_HG_L, L"Left (pref=120)", 0);
        sui_last_pref(h, 120.0f, 0.0f);
        sui_button(h, ID_FLEX_HG_M, L"Middle (grow=1)", 0);
        sui_last_grow(h, 1);
        sui_button(h, ID_FLEX_HG_R, L"Right (pref=120)", 0);
        sui_last_pref(h, 120.0f, 0.0f);
    } sui_end(h);

    sui_label(h, L"Horizontal cap — three grow=1 buttons, each capped at max=180:");
    sui_hstack(h, 3, 8.0f); {
        sui_button(h, ID_FLEX_HM_1, L"A", 0);
        sui_last_grow(h, 1); sui_last_max(h, 180.0f, 0.0f);
        sui_button(h, ID_FLEX_HM_2, L"B", 0);
        sui_last_grow(h, 1); sui_last_max(h, 180.0f, 0.0f);
        sui_button(h, ID_FLEX_HM_3, L"C", 0);
        sui_last_grow(h, 1); sui_last_max(h, 180.0f, 0.0f);
    } sui_end(h);

    sui_label(h, L"Horizontal shrink — pref 200/400/600 overflow; each loses width in proportion to its basis:");
    sui_hstack(h, 3, 8.0f); {
        sui_button(h, ID_FLEX_HS_1, L"Pref 200", 0);
        sui_last_pref(h, 200.0f, 0.0f);
        sui_button(h, ID_FLEX_HS_2, L"Pref 400", 0);
        sui_last_pref(h, 400.0f, 0.0f);
        sui_button(h, ID_FLEX_HS_3, L"Pref 600", 0);
        sui_last_pref(h, 600.0f, 0.0f);
    } sui_end(h);

    /* Read consumed_h before popping (memory still valid until next push). */
    float consumed_h1 = root1->cursor_y - root1->y;
    if (consumed_h1 > 0.0f) consumed_h1 -= root1->gap;
    if (consumed_h1 < 0.0f) consumed_h1 = 0.0f;
    h->container_depth--;

    /* ── Vertical demo caption (imperative — outside any container). */
    float caption_y = L.y + consumed_h1 + L.row_gap;
    sui_ctrl_add_label(h, -1, L.x, caption_y, L.w, L.lh,
        L"Vertical grow — middle row (edit) fills remaining height:");
    float v_y = caption_y + L.lh + L.lbl_gap;
    float v_remaining = (cy + ch_total) - v_y;
    if (v_remaining < 0.0f) v_remaining = 0.0f;

    /* ── Vertical demo: ROOT with explicit h triggers VSTACK distribute. */
    SuiRect r2_px = {L.x, v_y, L.x + L.w, v_y + v_remaining};
    if (!sui_begin_px(h, r2_px, 8.0f * L.s, SUI_CLEAR_NONE)) return;

    sui_vstack(h); {
        sui_label(h, L"— Header (intrinsic height) —");
        /* Canonical declarative textview-with-grow pattern: full container
         * width, default min height = 3 lines, grow=1 absorbs leftover
         * vertical space. Replaces the open-coded `h = parent_h - sibling_h
         * - gaps` math that imperative consumers used to write. */
        sui_textview(h, ID_FLEX_V_BODY,
            L"Textview, grow=1: absorbs all leftover vertical space.\n"
            L"Try resizing the window — height tracks the section, but\n"
            L"the minimum stays at ~3 lines (SUI_DEFAULT_TEXTVIEW_H).",
            512, /*readonly=*/0);
        sui_last_grow(h, 1);
        sui_label(h, L"— Footer (intrinsic height) —");
    } sui_end(h);

    h->container_depth--;

    /* Pin section vertical floor. Auto-scan can't supply this for the
     * Flex section: at large H the vstack is in the *grow* branch
     * (edit absorbs leftover), so c->h of the edit row is ≈ H and
     * sui_ctrl_group_min_size's top_h = max(c->y + c->h) tracks the
     * current window height — yielding a floor of ≈ H that blocks any
     * vertical shrink. The intrinsic floor is the vstack's effective
     * sum (header lh + edit effective_h + footer lh + 2 gaps) plus the
     * fixed section above it and the bottom-bar reserve below it. We
     * pin that explicitly; the dispatch in sui_handle_wm_getminmaxinfo
     * merges per-axis with max(scan, pin), so the W axis still uses
     * scan (caption labels widen the floor) while H is stable. */
    {
        const float vstack_gap = 8.0f * L.s;
        /* vstack rows = header label + textview + footer label. Textview min
         * is SUI_DEFAULT_TEXTVIEW_H (~80dp), not L.ch (~28dp) — using L.ch
         * here under-counted by ~50dp and let the window shrink past the
         * vstack's natural floor, colliding the bottom bar with the footer. */
        const float vstack_min_h = 2.0f * L.lh
                                 + SUI_DEFAULT_TEXTVIEW_H * L.s
                                 + 2.0f * vstack_gap;
        const float section_min_h =
            36.0f * L.s                /* title */
            + L.row_gap
            + consumed_h1              /* H demos — height-stable */
            + L.row_gap
            + L.lh + L.lbl_gap         /* V demo caption */
            + vstack_min_h;
        /* Bottom-bar reserve matches the imperative bar layout at L708:
         * bar_h = ch + lh + lbl_gap, sat at bar_y = H - bar_h - 10*s,
         * with a 14*s gap above. */
        const float bar_h_local = L.ch + L.lh + L.lbl_gap;
        const float bar_reserve = 14.0f * L.s + bar_h_local + 10.0f * L.s;
        h->min_client_h = cy + section_min_h + bar_reserve;
    }
}

/* ---- Section: Declarative containers ----------------------------------- */
/* Side-by-side counterpart to BuildSectionContainers (imperative). Same
 * scrollview overflow demo, but composed purely with the declarative
 * wrappers: `sui_scrollview` + `sui_clipbox` push containers whose
 * descendants are reparented automatically via parent_ctrl_id. No manual
 * `parent_id` plumbing, no manual cursor math.
 *
 * Like BuildSectionFlex, this runs after sui_ctrl_add_nav_list has already
 * been pushed into the host, so we can't use `sui_begin` (it would clear
 * control_count). Instead we hand-push a ROOT container around the
 * section rect, mirroring the Flex section's pattern. */
static void BuildSectionDeclContainers(TestbedData* d, float cx, float cy,
                                       float cw, float ch_total) {
    SuiControlHost* h = &d->host;
    SectionLayout L = MakeLayout(d, cx, cy, cw);
    (void)cy;

    EmitSectionTitle(h, &L, L"Declarative containers");

    const float section_bottom = cy + ch_total;
    const float remaining_y    = L.y;
    float       remaining_h    = section_bottom - remaining_y;
    if (remaining_h < 0.0f) remaining_h = 0.0f;

    if (h->container_depth >= SUI_MAX_CONTAINER_DEPTH) return;
    SuiContainer* root = &h->container_stack[h->container_depth++];
    memset(root, 0, sizeof(*root));
    root->parent_ctrl_id = -1;
    root->type     = SUI_CONT_ROOT;
    root->x        = L.x;
    root->y        = remaining_y;
    root->w        = L.w;
    root->h        = remaining_h;
    root->cursor_x = root->x;
    root->cursor_y = root->y;
    root->gap      = 8.0f * L.s;

    /* Scrollview takes ~62% of remaining height. Width spans the section.
     * Convert physical → DIP because the declarative API scales DIPs. */
    const float sv_h_phys   = remaining_h * 0.62f;
    const float clip_h_phys = remaining_h * 0.30f;
    const float w_dip       = L.w / L.s;
    const float sv_h_dip    = sv_h_phys   / L.s;
    const float clip_h_dip  = clip_h_phys / L.s;

    sui_scrollview(h, ID_DECL_SV, w_dip, sv_h_dip, 6.0f); {
        /* Labels are single-line + clip-on-overflow (see sui_ctrl_draw_label).
         * Keep header copy short so the scrollview demo remains readable at
         * the section's min width. Long-form commentary belongs in comments,
         * not on-screen. */
        sui_label(h, L"Scrollview demo");
        sui_label(h, L"Resize narrower for scrollbar.");
        /* Inner hstack to show that nested containers reparent correctly. */
        sui_hstack(h, 3, 8.0f); {
            sui_button(h, ID_DECL_BTN_1, L"One",   0);
            sui_button(h, ID_DECL_BTN_2, L"Two",   0);
            sui_button(h, ID_DECL_BTN_3, L"Three", 0);
        } sui_end(h);
        /* Padding rows so content easily overflows even at large window heights. */
        for (int i = 0; i < 24; ++i) {
            wchar_t buf[64];
            swprintf_s(buf, 64, L"Row %d", i + 1);
            sui_label(h, buf);
        }
    } sui_end(h);

    /* Clipbox below: same content but no scrollbar, overflow simply clipped. */
    sui_clipbox(h, ID_DECL_CLIP, w_dip, clip_h_dip, 6.0f); {
        sui_label(h, L"Clipbox demo");
        sui_label(h, L"Overflow is clipped (no scrollbar).");
        for (int i = 0; i < 8; ++i) {
            wchar_t buf[64];
            swprintf_s(buf, 64, L"Clipped row %d", i + 1);
            sui_label(h, buf);
        }
    } sui_end(h);

    h->container_depth--; /* pop manually-pushed ROOT (same as Flex section) */

    /* Pin section vertical floor. Scrollview and clipbox heights are
     * proportional to `remaining_h`, so the clipbox's c->y scales with the
     * current window height. The default Y-band/FLEX_ROWS scan reports
     * `top_h = max(c->y + c->h)`, which therefore tracks current H and
     * blocks vertical shrink. Pin a stable floor (sv_min + clip_min + chrome)
     * so the per-axis OVERRIDE in sui_handle_wm_getminmaxinfo uses this
     * instead of the W-coupled scan. */
    {
        const float sv_min_h   = 120.0f * L.s;   /* scrollview viewport floor */
        const float clip_min_h = 100.0f * L.s;   /* clipbox: a few rows worth */
        const float inter_gap  = 8.0f * L.s;
        const float section_min_h =
            36.0f * L.s                /* title */
            + L.row_gap
            + sv_min_h
            + inter_gap
            + clip_min_h;
        const float bar_h_local = L.ch + L.lh + L.lbl_gap;
        const float bar_reserve = 14.0f * L.s + bar_h_local + 10.0f * L.s;
        h->min_client_h = cy + section_min_h + bar_reserve;
    }

    /* Pin section horizontal floor. sui_scrollview / sui_clipbox call
     * sui__decl_place(host, w, h, w, ...) with intrinsic_w_in = w = L.w (the
     * full section width), which sui__decl_bubble_root mirrors into
     * host->intrinsic_min_w each frame. Per the per-axis OVERRIDE in
     * sui_handle_wm_getminmaxinfo: with min_client_w == 0 the bubble becomes
     * the floor → previous frame's L.w → window can't shrink W. Pin a stable
     * content floor (3 buttons + gaps + scrollbar gutter) so the OVERRIDE
     * uses this instead of the lagging bubble. Right margin is picked up
     * dynamically by the pad-detect pass; no need to reserve it here. */
    {
        /* Floor sized to fit the section title "Declarative containers" at
         * SUI_TITLE_PT (~22pt bold ≈ 280dp text width) plus a little breathing
         * room, then the scrollbar gutter. Labels inside the scrollview/clipbox
         * are clip-on-overflow by contract; keep header copy short enough that
         * the demo stays readable at this floor (see the sui_label calls above). */
        const float section_min_w = 340.0f * L.s
                                  + SUI_SCROLLVIEW_SB_HOT_W * L.s;
        h->min_client_w = cx + section_min_w;
    }
}

/* ---- Build all controls -------------------------------------------------- */
static void BuildControls(TestbedData* d) {
    SuiControlHost* h = &d->host;
    /* clear_page (instead of raw control_count = 0) frees heap-backed
     * textview buffers in non-persistent slots so WM_SIZE rebuilds don't
     * leak. persistent_control_count is 0 here (no chrome is marked
     * persistent), so this resets everything cleanly. */
    sui_ctrl_clear_page(h);
    h->container_depth = 0;
    h->intrinsic_min_w = 0.0f;
    h->intrinsic_min_h = 0.0f;
    /* Reset per-section floor pin — section builder may set this if its
     * layout intent (e.g. symmetric N-col split) can't be reconstructed by
     * the generic min-size scan.  Resetting per rebuild prevents one
     * section's floor from leaking into another. */
    h->min_client_w = 0.0f;
    h->min_client_h = 0.0f;

    /* All sections position controls at W-scaled X (col_w = 0.5 *
     * content_w; flex stacks span content_w) and rebuild on WM_SIZE —
     * the default max(ex+ew) min-size scan would track W down with them.
     * Opt into sum-of-widths so min stays content-derived. The FLEX
     * section also benefits: a full-height nav merges every Y-band on the
     * default scan, which then tracks W via grown hstack child X
     * positions. FLEX_ROWS scans per-row, so the nav slice is independent
     * of demo rows. */
    h->min_size_strategy = SUI_MIN_SIZE_FLEX_ROWS;

    RECT cr; GetClientRect(h->hwnd, &cr);
    const float W = (float)cr.right;
    const float H = (float)cr.bottom;
    const float s = (float)d->dpi / 96.0f;

    const float pad     = 20.0f * s;
    const float gap     = 16.0f * s;
    const float nav_w   = 180.0f * s;
    const float ch      = 28.0f * s;
    const float lh      = 18.0f * s;
    const float lbl_gap =  4.0f * s;
    const float bar_h   = ch + lh + lbl_gap;
    const float bar_y   = H - bar_h - 10.0f * s;

    /* ---- Left chrome nav (section selector) — full window height ---- */
    const float nav_x = 0.0f;
    const float nav_y = 0.0f;
    const float nav_h = H;
    sui_ctrl_add_nav_list(h, ID_NAV_CHROME,
        nav_x, nav_y, nav_w, nav_h,
        /*item_height=*/44.0f * s, /*selected=*/d->section);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Basic controls", L"\xE71D", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Choices",        L"\xE73E", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Text input",     L"\xE932", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Containers",     L"\xE7C4", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Flex",           L"\xE8A9", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Decl containers",L"\xE8B7", 0xFF4CC2FF);
    sui_ctrl_nav_add_item(h, ID_NAV_CHROME, L"Shader",         L"\xE790", 0xFF4CC2FF);
    /* Nav is full-height: width pins the floor (so the chrome can't shrink
     * narrower than the nav), height stays elastic so the window can shrink
     * past nav.h. NAV_LIST has no text-derived intrinsic, so we set min_w
     * explicitly rather than rely on the atom-default branch. */
    {
        SuiControl* nav = sui_ctrl_find(h, ID_NAV_CHROME);
        if (nav) nav->min_w = nav->w;
    }

    /* ---- Right content area ---- */
    const float content_x = nav_x + nav_w + gap;
    const float content_y = pad;
    const float content_w = W - content_x - pad;
    const float content_h = bar_y - 14.0f * s - content_y;
    sui_ctrl_nav_set_content_wheel_region(h, ID_NAV_CHROME,
        content_x, content_y, content_w, content_h, 1);

    UpdateShaderClock(d);

    switch (d->section) {
        case SEC_BASIC:           BuildSectionBasic(d,           content_x, content_y, content_w, content_h); break;
        case SEC_CHOICES:         BuildSectionChoices(d,         content_x, content_y, content_w, content_h); break;
        case SEC_TEXT_INPUT:      BuildSectionTextInput(d,       content_x, content_y, content_w, content_h); break;
        case SEC_CONTAINERS:      BuildSectionContainers(d,      content_x, content_y, content_w, content_h); break;
        case SEC_FLEX:            BuildSectionFlex(d,            content_x, content_y, content_w, content_h); break;
        case SEC_DECL_CONTAINERS: BuildSectionDeclContainers(d,  content_x, content_y, content_w, content_h); break;
        case SEC_SHADER:          BuildSectionShader(d,          content_x, content_y, content_w, content_h); break;
    }

    /* ---- Bottom bar — scheme + typography pickers ---- */
    const float combo_w     = 140.0f * s;
    const float gap_b       =   8.0f * s;
    const float right_total = combo_w * 2.0f + gap_b;

    const char* scheme_names[SUI_MAX_SCHEMES];
    int n_schemes = sui_scheme_list(scheme_names, SUI_MAX_SCHEMES);
    static wchar_t scheme_w[SUI_MAX_SCHEMES][SUI_SCHEME_NAME_MAX];
    const wchar_t* scheme_w_ptrs[SUI_MAX_SCHEMES];
    int sel_scheme = 0;
    for (int i = 0; i < n_schemes; ++i) {
        MultiByteToWideChar(CP_UTF8, 0, scheme_names[i], -1, scheme_w[i], SUI_SCHEME_NAME_MAX);
        scheme_w_ptrs[i] = scheme_w[i];
        if (sui__streq(scheme_names[i], d->scheme_name)) sel_scheme = i;
    }

    const char* typo_names[SUI_MAX_TYPOGRAPHIES];
    int n_typos = sui_typography_list(typo_names, SUI_MAX_TYPOGRAPHIES);
    static wchar_t typo_w[SUI_MAX_TYPOGRAPHIES][SUI_SCHEME_NAME_MAX];
    const wchar_t* typo_w_ptrs[SUI_MAX_TYPOGRAPHIES];
    int sel_typo = 0;
    for (int i = 0; i < n_typos; ++i) {
        MultiByteToWideChar(CP_UTF8, 0, typo_names[i], -1, typo_w[i], SUI_SCHEME_NAME_MAX);
        typo_w_ptrs[i] = typo_w[i];
        if (sui__streq(typo_names[i], d->typo_name)) sel_typo = i;
    }

    const float right_x  = W - right_total - 10.0f * s;
    const float scheme_x = right_x;
    const float typo_x   = right_x + combo_w + gap_b;

    sui_ctrl_add_label(h, ID_LBL_SCHEME, scheme_x, bar_y, combo_w, lh, L"Scheme");
    sui_ctrl_add_combobox(h, ID_COMBO_SCHEME,
        scheme_x, bar_y + lh + lbl_gap, combo_w, ch,
        scheme_w_ptrs, n_schemes, sel_scheme);

    sui_ctrl_add_label(h, ID_LBL_TYPO, typo_x, bar_y, combo_w, lh, L"Typography");
    sui_ctrl_add_combobox(h, ID_COMBO_TYPO,
        typo_x, bar_y + lh + lbl_gap, combo_w, ch,
        typo_w_ptrs, n_typos, sel_typo);

    /* Anchor the bottom bar: combos pinned to bottom-right with 10*s margins;
     * labels sit one row higher (+ ch+lbl_gap on bottom_margin); the leading
     * pair (Scheme) sits left of the trailing pair so its right_margin sums
     * the trailing pair's column + gap_b + 10*s. */
    const uint32_t anchor = SUI_CTRL_ANCHOR_RIGHT | SUI_CTRL_ANCHOR_BOTTOM;
    const float    rm_lead  = combo_w + gap_b + 10.0f * s + gap;       /* + nav clearance */
    const float    rm_trail = 10.0f * s;
    const float    bm_lbl   = lbl_gap + ch + 10.0f * s + 14.0f * s;    /* + row gap above bar */
    const float    bm_combo = 10.0f * s + 14.0f * s;                   /* + row gap above bar */
    sui_ctrl_set_anchor(h, ID_LBL_SCHEME,   anchor, rm_lead,  bm_lbl);
    sui_ctrl_set_anchor(h, ID_COMBO_SCHEME, anchor, rm_lead,  bm_combo);
    sui_ctrl_set_anchor(h, ID_LBL_TYPO,     anchor, rm_trail, bm_lbl);
    sui_ctrl_set_anchor(h, ID_COMBO_TYPO,   anchor, rm_trail, bm_combo);
}

/* ---- Click handler ------------------------------------------------------- */
static void OnClick(int id, void* udata) {
    TestbedData* d = static_cast<TestbedData*>(udata);

    if (id == ID_NAV_CHROME) {
        SuiControl* nav = sui_ctrl_find(&d->host, ID_NAV_CHROME);
        if (!nav) return;
        int sel = nav->d.nav.selected;
        if (sel < 0 || sel >= SEC_COUNT) return;
        if (sel == d->section) return;
        d->section = sel;
        EditSnapshot snap{};
        SaveEdits(&d->host, &snap);
        BuildControls(d);
        RestoreEdits(&d->host, &snap);
        InvalidateRect(d->host.hwnd, NULL, FALSE);
        return;
    }

    if (id == ID_SHADER_PAUSE) {
        SuiControl* button = sui_ctrl_find(&d->host, ID_SHADER_PAUSE);
        d->shader_paused = !d->shader_paused;
        UpdateShaderClock(d);
        if (button) sui__wstrncpy_z(button->text, d->shader_paused ? L"Resume" : L"Pause", SUI_CTRL_TEXT_MAX);
        InvalidateRect(d->host.hwnd, NULL, FALSE);
        return;
    }

    if (id == ID_SHADER_SPEED) {
        SuiControl* slider = sui_ctrl_find(&d->host, ID_SHADER_SPEED);
        if (slider) d->shader_speed = slider->d.slider.value / 100.0f;
        return;
    }

    if (id == ID_COMBO_SCHEME) {
        SuiControl* cb = sui_ctrl_find(&d->host, ID_COMBO_SCHEME);
        if (!cb) return;
        const char* names[SUI_MAX_SCHEMES];
        int n = sui_scheme_list(names, SUI_MAX_SCHEMES);
        int idx = cb->d.combo.selected;
        if (idx < 0 || idx >= n) return;
        const SuiColorScheme* sc = sui_scheme_find(names[idx]);
        if (sc) {
            d->scheme = *sc;
            sui__strncpy_z(d->scheme_name, names[idx], SUI_SCHEME_NAME_MAX);
            d->host.scheme = &d->scheme;
            EditSnapshot snap{};
            SaveEdits(&d->host, &snap);
            BuildControls(d);
            RestoreEdits(&d->host, &snap);
            InvalidateRect(d->host.hwnd, NULL, FALSE);
        }
        return;
    }

    if (id == ID_COMBO_TYPO) {
        SuiControl* cb = sui_ctrl_find(&d->host, ID_COMBO_TYPO);
        if (!cb) return;
        const char* names[SUI_MAX_TYPOGRAPHIES];
        int n = sui_typography_list(names, SUI_MAX_TYPOGRAPHIES);
        int idx = cb->d.combo.selected;
        if (idx < 0 || idx >= n) return;
        const SuiTypography* t = sui_typography_find(names[idx]);
        if (t) {
            sui_renderer_apply_typography(&d->renderer, t);
            sui__strncpy_z(d->typo_name, names[idx], SUI_SCHEME_NAME_MAX);
            EditSnapshot snap{};
            SaveEdits(&d->host, &snap);
            BuildControls(d);
            RestoreEdits(&d->host, &snap);
            InvalidateRect(d->host.hwnd, NULL, FALSE);
        }
        return;
    }

    if (id == ID_CONTAINER_TABS) {
        SuiControl* tabs = sui_ctrl_find(&d->host, ID_CONTAINER_TABS);
        if (!tabs) return;
        int sel = tabs->d.tabs.selected;
        if (sel < 0 || sel >= 3) return;
        if (sel == d->container_tab) return;
        d->container_tab = sel;
        EditSnapshot snap{};
        SaveEdits(&d->host, &snap);
        BuildControls(d);
        RestoreEdits(&d->host, &snap);
        InvalidateRect(d->host.hwnd, NULL, FALSE);
        return;
    }

    if (id == ID_SLIDER) {
        SuiControl* sl = sui_ctrl_find(&d->host, ID_SLIDER);
        if (sl) sui_ctrl_progress_set(&d->host, ID_PROGRESS, sl->d.slider.value);
    }
    InvalidateRect(d->host.hwnd, NULL, FALSE);
}

/* ---- WndProc ------------------------------------------------------------- */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TestbedData* d = (TestbedData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        d = (TestbedData*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)d);

        d->dpi    = GetDpiForWnd(hwnd);
        d->scheme = sui_scheme_light;

        /* Register a custom app-defined scheme alongside the built-ins. */
        sui_scheme_register("blue-dark", &demo_blue_dark);
        /* Register a custom typography alongside the "default" built-in. */
        sui_typography_register("serif", &demo_serif_typo);

        /* WIC's CoCreateInstance needs COM initialized on the UI thread before
         * sui_renderer_load_image can succeed. */
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

        sui_renderer_init_fields(&d->renderer);
        if (!sui_renderer_init(&d->renderer, hwnd)) return -1;

        sui_ctrl_host_init(&d->host, hwnd, &d->renderer, &d->scheme, d->dpi);
        d->host.on_click      = OnClick;
        d->host.on_click_data = d;

        d->logo = LoadLogoFromResource(&d->renderer);

        BuildControls(d);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (d && sui_renderer_is_init(&d->renderer)) {
            sui_renderer_begin(&d->renderer);
            sui_renderer_clear(&d->renderer, d->scheme.background);
            sui_ctrl_draw_all(&d->host);
            sui_renderer_end(&d->renderer);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        if (d) sui_handle_wm_getminmaxinfo(&d->host, hwnd, lp);
        return 0;
    }

    case WM_SIZE: {
        if (d && sui_renderer_is_init(&d->renderer)) {
            RECT cr; GetClientRect(hwnd, &cr);
            EditSnapshot snap{};
            SaveEdits(&d->host, &snap);
            sui_renderer_resize(&d->renderer, cr.right, cr.bottom);
            BuildControls(d);
            RestoreEdits(&d->host, &snap);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        if (d) {
            d->dpi = HIWORD(wp);
            d->host.dpi = d->dpi;
            const RECT* nr = (const RECT*)lp;
            EditSnapshot snap{};
            SaveEdits(&d->host, &snap);
            SetWindowPos(hwnd, NULL, nr->left, nr->top,
                nr->right - nr->left, nr->bottom - nr->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            RECT cr; GetClientRect(hwnd, &cr);
            sui_renderer_resize(&d->renderer, cr.right, cr.bottom);
            BuildControls(d);
            RestoreEdits(&d->host, &snap);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (d) {
            sui_ctrl_on_mouse_move(&d->host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            HCURSOR cur = sui_ctrl_get_cursor(&d->host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            SetCursor(cur);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (d) sui_ctrl_on_mouse_leave(&d->host);
        return 0;

    case WM_LBUTTONDOWN:
        if (d) { SetFocus(hwnd); sui_ctrl_on_lbutton_down(&d->host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;

    case WM_LBUTTONDBLCLK:
        if (d) { SetFocus(hwnd); sui_ctrl_on_lbutton_dblclk(&d->host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); }
        return 0;

    case WM_LBUTTONUP:
        if (d) sui_ctrl_on_lbutton_up(&d->host, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;

    case WM_RBUTTONDOWN:
        if (d) {
            SetFocus(hwnd);
            if (sui_ctrl_on_shortcut_mouse_button_down(&d->host, SUI_SHORTCUT_MOUSE_RIGHT)) return 0;
        }
        break;

    case WM_RBUTTONUP:
        if (d && sui_ctrl_on_shortcut_mouse_button_up(&d->host, SUI_SHORTCUT_MOUSE_RIGHT)) return 0;
        break;

    case WM_MBUTTONDOWN:
        if (d) {
            SetFocus(hwnd);
            if (sui_ctrl_on_shortcut_mouse_button_down(&d->host, SUI_SHORTCUT_MOUSE_MIDDLE)) return 0;
        }
        break;

    case WM_MBUTTONUP:
        if (d && sui_ctrl_on_shortcut_mouse_button_up(&d->host, SUI_SHORTCUT_MOUSE_MIDDLE)) return 0;
        break;

    case WM_XBUTTONDOWN:
        if (d) {
            UINT mouseButton = (HIWORD(wp) == XBUTTON2) ? SUI_SHORTCUT_MOUSE_X2 : SUI_SHORTCUT_MOUSE_X1;
            SetFocus(hwnd);
            if (sui_ctrl_on_shortcut_mouse_button_down(&d->host, mouseButton)) return TRUE;
        }
        break;

    case WM_XBUTTONUP:
        if (d) {
            UINT mouseButton = (HIWORD(wp) == XBUTTON2) ? SUI_SHORTCUT_MOUSE_X2 : SUI_SHORTCUT_MOUSE_X1;
            if (sui_ctrl_on_shortcut_mouse_button_up(&d->host, mouseButton)) return TRUE;
        }
        break;

    case WM_CONTEXTMENU: {
        if (d) {
            if (d->host.focused_control >= 0 && d->host.focused_control < d->host.control_count) {
                SuiControl* c = &d->host.controls[d->host.focused_control];
                if (c->type == SUI_CTRL_SHORTCUT_RECORDER && c->d.shortcut.recording) return 0;
            }
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            sui_ctrl_on_rbutton_up(&d->host, pt.x, pt.y);
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (d) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            sui_ctrl_on_mouse_wheel(&d->host, pt.x, pt.y, GET_WHEEL_DELTA_WPARAM(wp));
        }
        return 0;
    }

    case WM_SETFOCUS:
        if (d) {
            d->host.window_has_focus = 1;
            d->host.caret_visible    = 1;
            SetTimer(hwnd, 1, 500, NULL);
        }
        return 0;

    case WM_KILLFOCUS:
        if (d) {
            d->host.window_has_focus = 0;
            KillTimer(hwnd, 1);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_TIMER:
        if (d) sui_ctrl_on_timer(&d->host, wp);
        if (d && wp == 1) {
            d->host.caret_visible = !d->host.caret_visible;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_INPUT:
        if (d) sui_ctrl_shortcut_on_raw_input(&d->host, (HRAWINPUT)lp);
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_KEYDOWN:
        if (d) sui_ctrl_on_keydown_ex(&d->host, wp, lp);
        return 0;

    case WM_SYSKEYDOWN:
        if (d) sui_ctrl_on_keydown_ex(&d->host, wp, lp);
        return 0;

    case WM_KEYUP:
        if (d) sui_ctrl_on_keyup(&d->host, wp);
        return 0;

    case WM_SYSKEYUP:
        if (d) sui_ctrl_on_keyup(&d->host, wp);
        return 0;

    case WM_CHAR:
        if (d) sui_ctrl_on_char(&d->host, (wchar_t)wp);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_SUI_VSYNC:
        if (d) sui_ctrl_on_vsync(&d->host);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_GETOBJECT:
        if (d) return sui_uia_handle_getobject(&d->host, wp, lp);
        break;

    case WM_DESTROY:
        if (d) {
            sui_renderer_free_image(&d->renderer, d->logo);
            d->logo = SuiImageHandle{};
            demo_shader_release(d->shader);
            d->shader = nullptr;
            sui_ctrl_host_release(&d->host);
            sui_renderer_release(&d->renderer);
        }
        CoUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---- Entry --------------------------------------------------------------- */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SuiTestbed";
    RegisterClassExW(&wc);

    UINT sysDpi = 96;
    {
        typedef UINT(WINAPI* FnDpi)();
        auto fnDpi = (FnDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem");
        if (fnDpi) sysDpi = fnDpi();
        else {
            HDC dc = GetDC(NULL);
            sysDpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
            ReleaseDC(NULL, dc);
        }
    }
    int ww = MulDiv(960, (int)sysDpi, 96);
    int wh = MulDiv(720, (int)sysDpi, 96);

    TestbedData data{};

    HWND hwnd = CreateWindowExW(
        0, L"SuiTestbed", L"Slim UI Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, ww, wh,
        NULL, NULL, hInstance, &data);

    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
