#ifndef SLIM_UI_H
#define SLIM_UI_H

#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#include "sui_vsync.h"

/* Font size defines (points) */
#define SUI_TITLE_PT    22.0f
#define SUI_SUBTITLE_PT 11.0f
#define SUI_CONTROL_PT  11.0f

/* Control geometry defaults (DIPs). Used by sui_ctrl_intrinsic_size and sui_decl.h. */
#define SUI_DEFAULT_CTRL_H     28.0f  /* standard input/button height */
#define SUI_DEFAULT_LABEL_H    18.0f  /* label/caption height */
#define SUI_DEFAULT_PROG_H      8.0f  /* progress bar height */
#define SUI_DEFAULT_TAB_H      34.0f  /* horizontal tab strip item height */
#define SUI_DEFAULT_TAB_GAP     4.0f  /* horizontal gap between adjacent tabs */
#define SUI_DEFAULT_TAB_CONTENT_GAP 6.0f /* vertical gap between tab strip and content */
#define SUI_TAB_CONTENT_SLIDE_DP 64.0f /* directional tab content slide distance cap */
#define SUI_DEFAULT_TOOLTIP_SIZE 18.0f /* compact info tooltip target */
#define SUI_TOOLTIP_DELAY_MS   550u   /* hover delay before drawing tooltip popup */
#define SUI_TOOLTIP_TIMER_ID   0x5553
#define SUI_BUTTON_HPAD        24.0f  /* horizontal padding (each side) in button intrinsic width */
#define SUI_CHECKBOX_BOX_W     16.0f  /* checkbox/radio box side length */
#define SUI_CHECKBOX_GAP        8.0f  /* gap between box and label text */
#define SUI_CHECKBOX_H         20.0f  /* checkbox/radio intrinsic height (differs from SUI_DEFAULT_CTRL_H) */
#define SUI_COMBO_ARROW_W      28.0f  /* combobox chevron (dropdown arrow) column width */
#define SUI_COMBO_TEXT_PAD     16.0f  /* combobox horizontal text padding (both sides combined) */
#define SUI_EDIT_TEXT_PAD      10.0f  /* edit text padding on each horizontal side */

/* Undo/redo stack depth for SUI_CTRL_EDIT. Stack occupies 2×depth pool entries
 * (first half = undo, second half = redo); heap-allocated in sui_ctrl_add_edit. */
#define SUI_EDIT_UNDO_DEPTH 8

/* Maximum wchar_t buffer size for SuiControl::text, including null terminator.
 * Fits labels, button captions, combo display text, and short edit fields.
 * Reduces SuiControl hot-path size: 64 controls × (256→128) = saves 16 KB. */
#define SUI_CTRL_TEXT_MAX 128
#define SUI_TOOLTIP_TEXT_MAX 512

/* Named-parameter structs for slider and spinner constructors.
 * Use with sui_ctrl_add_slider_p / sui_ctrl_add_spinner_p and C99
 * compound-literal + designated-initializer syntax:
 *   sui_ctrl_add_slider_p(&host, id, x, y, w, h,
 *       (SuiSliderParams){ .min=0, .max=100, .value=50, .step=1 }); */
typedef struct { float min; float max; float value; float step; } SuiSliderParams;
typedef struct { float min; float max; float value; float step; } SuiSpinnerParams;

typedef enum SuiShortcutKind {
    SUI_SHORTCUT_KEYBOARD = 0,
    SUI_SHORTCUT_GAMEPAD  = 1
} SuiShortcutKind;

#define SUI_SHORTCUT_GAMEPAD_LEFT_TRIGGER  0x00010000u
#define SUI_SHORTCUT_GAMEPAD_RIGHT_TRIGGER 0x00020000u
#define SUI_GAMEPAD_DPAD_UP        0x0001u
#define SUI_GAMEPAD_DPAD_DOWN      0x0002u
#define SUI_GAMEPAD_DPAD_LEFT      0x0004u
#define SUI_GAMEPAD_DPAD_RIGHT     0x0008u
#define SUI_GAMEPAD_START          0x0010u
#define SUI_GAMEPAD_BACK           0x0020u
#define SUI_GAMEPAD_LEFT_THUMB     0x0040u
#define SUI_GAMEPAD_RIGHT_THUMB    0x0080u
#define SUI_GAMEPAD_LEFT_SHOULDER  0x0100u
#define SUI_GAMEPAD_RIGHT_SHOULDER 0x0200u
#define SUI_GAMEPAD_GUIDE          0x0400u
#define SUI_GAMEPAD_A              0x1000u
#define SUI_GAMEPAD_B              0x2000u
#define SUI_GAMEPAD_X              0x4000u
#define SUI_GAMEPAD_Y              0x8000u

#define SUI_SHORTCUT_MOUSE_NONE    0u
#define SUI_SHORTCUT_MOUSE_LEFT    1u
#define SUI_SHORTCUT_MOUSE_RIGHT   2u
#define SUI_SHORTCUT_MOUSE_MIDDLE  3u
#define SUI_SHORTCUT_MOUSE_X1      4u
#define SUI_SHORTCUT_MOUSE_X2      5u

/* ---------------------------------------------------------------------------
 * Color scheme
 * All colors are packed 0xAARRGGBB (or 0x00RRGGBB — alpha ignored by GDI).
 * --------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------
 * Typography
 * Per-role font selection. Defaults captured below as `sui_typography_default`.
 * Apps register named SuiTypography style sets via the registry API and
 * apply them to a renderer via sui_renderer_apply_typography.
 *
 * Weight uses standard CSS / DWrite values (100..900). 400 = normal, 600 =
 * semibold, 700 = bold. Italic is 0 (normal) or 1 (italic).
 * --------------------------------------------------------------------------- */
typedef struct SuiTypographyRole {
    wchar_t family[64];
    float   size_pt;
    int     weight;
    int     italic;
} SuiTypographyRole;

typedef struct SuiTypography {
    SuiTypographyRole body;     /* default control + label text */
    SuiTypographyRole subtitle; /* small headings, secondary labels */
    SuiTypographyRole title;    /* large headings */
    SuiTypographyRole icon;     /* icon glyph font (e.g. Segoe Fluent Icons) */
} SuiTypography;

typedef struct SuiColorScheme {
    /* General */
    uint32_t background;
    uint32_t text;
    uint32_t text_secondary;

    /* Buttons */
    uint32_t btn_face;
    uint32_t btn_border;
    uint32_t btn_text;
    uint32_t btn_hover;
    uint32_t btn_press;
    uint32_t btn_default_border;

    /* Checkbox */
    uint32_t check_border;
    uint32_t check_fill;
    uint32_t check_mark;

    /* Edit / input */
    uint32_t input_bg;
    uint32_t input_border;
    uint32_t input_text;
    uint32_t input_focus_border;
    uint32_t input_selection;
    uint32_t input_placeholder;

    /* Slider */
    uint32_t slider_track;
    uint32_t slider_fill;
    uint32_t slider_thumb;

    /* Navigation list */
    uint32_t nav_bg;
    uint32_t nav_hover;
    uint32_t nav_selected;
    uint32_t nav_border;

    /* Progress */
    uint32_t progress_bg;
    uint32_t progress_fill;

    /* Button area panel */
    uint32_t btn_area;
    uint32_t btn_area_border;
} SuiColorScheme;

typedef enum SuiStyleProp {
    SUI_STYLE_TEXT_COLOR        = 1u << 0,
    SUI_STYLE_BG_COLOR          = 1u << 1,
    SUI_STYLE_BORDER_COLOR      = 1u << 2,
    SUI_STYLE_HOVER_COLOR       = 1u << 3,
    SUI_STYLE_PRESS_COLOR       = 1u << 4,
    SUI_STYLE_SELECTED_COLOR    = 1u << 5,
    SUI_STYLE_FILL_COLOR        = 1u << 6,
    SUI_STYLE_ACCENT_COLOR      = 1u << 7,
    SUI_STYLE_PLACEHOLDER_COLOR = 1u << 8,
    SUI_STYLE_SELECTION_COLOR   = 1u << 9,
    SUI_STYLE_FONT_FAMILY       = 1u << 10
} SuiStyleProp;

typedef struct SuiControlStyle {
    uint32_t mask;
    uint32_t text_color;
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t hover_color;
    uint32_t press_color;
    uint32_t selected_color;
    uint32_t fill_color;
    uint32_t accent_color;
    uint32_t placeholder_color;
    uint32_t selection_color;
    wchar_t  font_family[64];
} SuiControlStyle;

/* ---------------------------------------------------------------------------
 * Image handle (renderer-agnostic bitmap reference)
 * Defined here so SUI_CTRL_IMAGE controls can embed it in the union.
 * --------------------------------------------------------------------------- */
typedef struct SuiImageHandle {
    void *data;
    int   width;
    int   height;
} SuiImageHandle;

/* How an image is fit into its control rect. */
typedef enum SuiImageScale {
    SUI_IMG_FILL = 0,   /* stretch to fill — ignores aspect ratio (default) */
    SUI_IMG_FIT  = 1,   /* letterbox — preserve aspect, fit inside, center */
    SUI_IMG_CLIP = 2    /* cover — preserve aspect, fill rect, crop overflow, center */
} SuiImageScale;

/* ---------------------------------------------------------------------------
 * Custom controls (guarded — requires D2D)
 * --------------------------------------------------------------------------- */
#ifdef SUI_FEATURE_CUSTOM_CONTROLS

/* Control types */
typedef enum SuiCtrlType {
    SUI_CTRL_BUTTON        = 0,
    SUI_CTRL_CHECKBOX      = 1,
    SUI_CTRL_RADIO         = 4,
    SUI_CTRL_EDIT          = 5,
    SUI_CTRL_COMBOBOX      = 6,
    SUI_CTRL_SLIDER        = 7,
    SUI_CTRL_SPINNER       = 8,
    SUI_CTRL_COLOR_SWATCH  = 9,
    SUI_CTRL_LABEL         = 10,
    SUI_CTRL_NAV_LIST      = 11,
    SUI_CTRL_IMAGE         = 12,
    SUI_CTRL_PROGRESS      = 14,
    SUI_CTRL_CONTAINER     = 15,
    SUI_CTRL_TEXTVIEW      = 16,
    SUI_CTRL_SCROLLVIEW    = 17,
    SUI_CTRL_SHORTCUT_RECORDER = 18,
    SUI_CTRL_TOOLTIP       = 19,
    SUI_CTRL_TABS          = 20,
    SUI_CTRL_LIST          = 21
} SuiCtrlType;

/* State flags (combinable) */
typedef enum SuiCtrlState {
    SUI_CTRL_NORMAL   = 0,
    SUI_CTRL_HOVER    = 1,
    SUI_CTRL_PRESSED  = 2,
    SUI_CTRL_DISABLED = 4,
    SUI_CTRL_FOCUSED  = 8,
    SUI_CTRL_CHECKED  = 16,
    SUI_CTRL_HIDDEN   = 32,
    SUI_CTRL_DEFAULT  = 64,
    SUI_CTRL_BOLD     = 128,   /* labels: render bold (uses title typography when font_size > 1.5x body) */
    /* Anchor flags — purely advisory metadata for sui_ctrl_min_size.
     * They do NOT reposition the control; the consumer's layout code still
     * sets c->x / c->y on every WM_SIZE. The flags only let the auto-min
     * helper compute a true minimum-window-size for right/bottom-anchored
     * stacks (whose c->x or c->y track the host width/height). Pair with
     * c->right_margin / c->bottom_margin via sui_ctrl_set_anchor.
     *
     * STRETCH_X / STRETCH_Y are reserved for future flex layout-fill
     * semantics. They have no effect on sui_ctrl_min_size today — the
     * floor is now `max(intrinsic, c->min_w)` with c->min_w defaulting
     * to 0 (elastic) and atom-style controls (swatch/image/shortcut recorder) set
     * to c->w at add-time. To pin a fill control's width into the min,
     * set c->min_w explicitly. */
    SUI_CTRL_ANCHOR_RIGHT  = 256,
    SUI_CTRL_ANCHOR_BOTTOM = 512,
    /* 1024 / 2048 retired with Phase 2b — use SuiControl.grow / pref_w/pref_h
     * for flex-fill semantics (sui_decl HSTACK/VSTACK distribute on these). */
    /* Container flag: row min = max(child.effective_w) instead of summing
     * children across a Y band. Use on flow/wrap containers where child
     * positions encode the *current* width's column count and would
     * otherwise force the row min to N cells wide. The caller still
     * recomputes child positions on resize; this flag only changes how
     * the min-size scan reports the floor. */
    SUI_CTRL_FLOW          = 4096,
    SUI_CTRL_SEMIBOLD      = 8192,
    SUI_CTRL_MEDIUM        = 16384
} SuiCtrlState;

#ifndef SUI_MAX_CONTROLS
#define SUI_MAX_CONTROLS         64   /* per window; define it the same way in every file that includes this header */
#endif
#define SUI_MAX_ITEMS            32
#define SUI_MAX_CONTAINER_DEPTH  8
#define SUI_MAX_TAB_TRANSITIONS  8
#define SUI_TAB_INDICATOR_ANIM_MS 280.0f
#define SUI_TAB_CONTENT_ANIM_MS  200.0f
#define SUI_TAB_VERTICAL_CONTENT_ANIM_MS 160.0f
#define SUI_TAB_TEXT_HOVER_ANIM_MS 100.0f

/* SuiControlHost::min_size_strategy values. See struct field for semantics. */
#define SUI_MIN_SIZE_DEFAULT     0
#define SUI_MIN_SIZE_FLEX_ROWS   1
/* Phase 2b flex slot cap per container. Overflow children remain placed by
 * the legacy single-pass path (equal-share for HSTACK, intrinsic-stack for
 * VSTACK) — they don't get pref/grow distribution. 16 covers all current
 * demo + real-consumer pages with headroom. */
#define SUI_MAX_FLEX_SLOTS       16

/* Navigation list item */
typedef struct SuiNavItem {
    wchar_t  text[64];
    wchar_t  icon[4];
    uint32_t icon_color;
} SuiNavItem;

/* Horizontal tab item */
typedef struct SuiTabItem {
    wchar_t text[64];
    float   hover_line;
} SuiTabItem;

typedef enum SuiListRowKind {
    SUI_LIST_ROW_ITEM    = 0,
    SUI_LIST_ROW_SECTION = 1
} SuiListRowKind;

typedef enum SuiListOptions {
    SUI_LIST_OPTION_NONE                         = 0,
    SUI_LIST_OPTION_SINGLE_LINE_TAIL_ELLIPSIS   = 1u << 0
} SuiListOptions;

typedef struct SuiListRow {
    int     kind;
    int     item_index;
    wchar_t leading[16];
    wchar_t text[96];
    wchar_t detail[96];
} SuiListRow;

typedef struct SuiControl SuiControl;
typedef struct SuiControlHost SuiControlHost;

/* Draws one control in the toolkit's place: see sui_ctrl_set_paint. */
typedef void (*SuiCtrlPaintFn)(SuiControlHost* host, SuiControl* ctrl, void* user);

typedef void (*SuiTabContentPaintFn)(SuiControlHost* host,
                                     SuiControl* tabs,
                                     int tab_index,
                                     void* user);

typedef struct SuiComboItemView {
    wchar_t text[64];
    wchar_t icon_path[260];
    SuiImageHandle icon;
    int has_icon;
} SuiComboItemView;

typedef void (*SuiListVirtualGetRowFn)(SuiControl* ctrl, int row_index, SuiListRow* out_row, void* userdata);
typedef int  (*SuiListVirtualRowAtOffsetFn)(SuiControl* ctrl, float offset, float* out_row_top, void* userdata);
typedef int  (*SuiListVirtualRowForItemFn)(SuiControl* ctrl, int item_index, void* userdata);
typedef void (*SuiComboVirtualGetItemFn)(SuiControl* ctrl, int item_index, SuiComboItemView* out_item, void* userdata);

/* Control descriptor */
struct SuiControl {
    SuiCtrlType   type;
    int           id;
    uint32_t      state;
    int           z;         /* draw order: higher z draws on top of lower z; ties resolved by insertion order */
    int           parent_id; /* id of parent container, or -1 for top-level (direct child of host) */
    float         x, y, w, h;
    float         min_w, min_h; /* min-size floor for sui_ctrl_effective_size; 0 = use intrinsic only */
    /* Phase 2b flex sizing — read by sui_decl HSTACK/VSTACK during distribution.
     * Units match c->w / c->min_w: scaled physical pixels (declarative wrappers DPI-scale before writing).
     * pref_w/h: preferred size (0 = use intrinsic effective size).
     * max_w/h:  upper bound (0 = unbounded).
     * grow:     leftover-share factor (0 = no grow, default). >0 share leftover proportionally.
     * shrink:   shrink factor when overflow (default 1; sui_ctrl_add sets it). 0 = never shrink below pref. */
    float         pref_w, pref_h;
    float         max_w,  max_h;
    int           grow;
    int           shrink;
    float         font_size;
    SuiCtrlPaintFn paint;         /* optional: draws this control instead of the toolkit */
    void          *paint_user;
    SuiControlStyle *style;       /* optional per-control overrides; NULL = scheme/typography */
    wchar_t       text[SUI_CTRL_TEXT_MAX];
    wchar_t       tooltip_text[SUI_TOOLTIP_TEXT_MAX];
    const wchar_t *tooltip;
    float          hover_t;  /* 0.0 = rest, 1.0 = fully hovered; driven by SUI_HOVER_TIMER_ID */
    float          right_margin;   /* gap from right edge to c.x+c.w when SUI_CTRL_ANCHOR_RIGHT is set */
    float          bottom_margin;  /* gap from bottom edge to c.y+c.h when SUI_CTRL_ANCHOR_BOTTOM is set */

    union d {
        struct {
            uint32_t face;
            uint32_t border;
            uint32_t text;
            uint32_t hover;
            uint32_t press;
            int      use_custom_colors;
        } button;

        /* SUI_CTRL_RADIO */
        struct {
            int group_id;
        } radio;

        /* SUI_CTRL_EDIT */
        struct {
            int     cursor;
            int     sel_start;
            int     sel_end;
            int     drag_sel_anchor; /* anchor position for click-drag and Shift+click */
            float   scroll_x;
            int     max_len;
            int     numeric_only;
            wchar_t placeholder[64];
            /* Undo/redo ring-pool. Heap: 2×SUI_EDIT_UNDO_DEPTH entries of SUI_CTRL_TEXT_MAX wchar_t.
             * [0..D-1] = undo stack (bottom-up), [D..2D-1] = redo stack (bottom-up).
             * undo_top / redo_top count valid entries in each half. */
            wchar_t (*undo_pool)[SUI_CTRL_TEXT_MAX];
            int      undo_cur[2 * SUI_EDIT_UNDO_DEPTH];
            int      undo_top;
            int      redo_top;
        } edit;

        /* SUI_CTRL_COMBOBOX */
        struct {
            wchar_t (*items)[64]; /* heap: SUI_MAX_ITEMS × 64 wchar_t — freed by sui_ctrl_combo_release */
            wchar_t (*item_icon_paths)[260]; /* optional heap: popup renderer reloads per-item image files */
            SuiImageHandle *item_icons;      /* optional heap of host-renderer handles; images are caller-owned */
            int     item_count;
            int     virtual_count;
            void   *virtual_data;
            SuiComboVirtualGetItemFn virtual_get_item;
            int     selected;
            int     open;
            int     hover_item;
            float   dropdown_scroll;
            float   popup_width;     /* physical px; <= 0 follows the closed control width */
            float   popup_radius;
            HWND    popup_hwnd;
        } combo;

        /* SUI_CTRL_SLIDER */
        struct {
            float min;
            float max;
            float value;
            float step;
            int   dragging;
        } slider;

        /* SUI_CTRL_SPINNER */
        struct {
            float   min;
            float   max;
            float   value;
            float   step;
            int     hover_up;
            int     hover_down;
            wchar_t edit_buf[32];
            int     edit_len;
            int     editing;
            float   edit_saved;
            int     cursor;          /* caret position in edit_buf (0..edit_len) */
            int     sel_start;       /* selection anchor; -1 = none */
            int     sel_end;         /* selection end; equal to sel_start = no sel */
            int     drag_anchor;     /* mouse-drag anchor cursor pos */
        } spinner;

        /* SUI_CTRL_COLOR_SWATCH */
        struct {
            uint32_t color;
        } swatch;

        /* SUI_CTRL_NAV_LIST */
        struct {
            SuiNavItem *items;    /* heap: SUI_MAX_ITEMS × SuiNavItem — freed by sui_ctrl_nav_release */
            int         item_count;
            int         selected;
            int         hover_item;
            float       item_height;
            float       scroll_offset;
            /* Optional wheel-to-select region outside the nav itself. When
             * enabled, a full wheel detent over this rectangle selects the
             * adjacent item only if no visible vertical scroller in the
             * rectangle currently overflows. */
            int         content_wheel_enabled;
            int         content_wheel_delta;
            float       content_wheel_x;
            float       content_wheel_y;
            float       content_wheel_w;
            float       content_wheel_h;
        } nav;

        /* SUI_CTRL_TABS */
        struct {
            SuiTabItem *items;    /* heap: SUI_MAX_ITEMS × SuiTabItem — freed by sui_ctrl_tabs_release */
            int         item_count;
            int         selected;
            int         hover_item;
            float       tab_height;
            float       tab_gap;
            float       content_gap;
            float       item_min_width;
            float       item_hpad;
            float       indicator_rest_w;
            float       indicator_x;
            float       indicator_w;
            float       indicator_from_x;
            float       indicator_from_w;
            float       indicator_to_x;
            float       indicator_to_w;
            float       indicator_anim_t;
            int         indicator_ready;
            int         indicator_anim_active;
            void       *content_bitmap;
            float       content_bitmap_w;
            float       content_bitmap_h;
            int         content_anim_active;
            int         content_anim_dir;
            int         content_anim_vertical;
            int         content_anim_from;
            int         content_anim_to;
            float       content_anim_t;
            float       content_anim_travel;
        } tabs;

        /* SUI_CTRL_LIST */
        struct {
            SuiListRow *rows;
            int         row_count;
            int         row_capacity;
            int         selected_row;
            int         hover_row;
            float       item_height;
            float       section_height;
            float       item_indent;
            uint32_t    options;       /* SuiListOptions bitmask */
            float       scroll_offset;
            float       sb_reveal;      /* 0..1 — keyboard/wheel-driven scrollbar visibility */
            int         sb_dragging;
            float       sb_drag_anchor;
            float       sb_drag_start_scroll;
            int         near_end_px;
            int         virtual_row_count;
            float       virtual_content_height;
            void       *virtual_data;
            SuiListVirtualGetRowFn virtual_get_row;
            SuiListVirtualRowAtOffsetFn virtual_row_at_offset;
            SuiListVirtualRowForItemFn virtual_row_for_item;
        } list;

        /* SUI_CTRL_IMAGE */
        struct {
            SuiImageHandle img;
            int circle_mask;    /* 1 = circular clip (for avatars) — currently not implemented; use pre-masked image */
            int scale;          /* SuiImageScale: SUI_IMG_FILL/FIT/CLIP. Default SUI_IMG_FILL. */
        } image;

        /* SUI_CTRL_SHORTCUT_RECORDER */
        struct {
            int kind;            /* SuiShortcutKind */
            UINT vk;
            UINT modifiers;
            UINT scan_code;
            int  extended;
            UINT mouse_button;
            uint32_t gamepad_buttons;
            int recording;
            UINT candidate_vk;
            UINT candidate_modifiers;
            UINT candidate_scan_code;
            int  candidate_extended;
            UINT candidate_mouse_button;
            int  candidate_mouse_released;
            uint32_t candidate_gamepad_buttons;
            uint32_t initial_gamepad_buttons;
            wchar_t prompt[64];
        } shortcut;

        /* SUI_CTRL_PROGRESS */
        struct {
            float min;
            float max;
            float value;
        } progress;

        /* SUI_CTRL_CONTAINER */
        struct {
            int no_clip;  /* 0 (default) = clip children to container rect; 1 = pure z-scope */
        } container;

        /* SUI_CTRL_TEXTVIEW — multi-line, soft-wrap, optional editor.
         * `buf`, `undo_buf`, and `redo_buf` are heap-allocated via sui_malloc;
         * the host frees them on clear_page (discarded slots) and host_release.
         * undo_len / redo_len == -1 means no entry saved yet. */
        struct {
            wchar_t *buf;
            wchar_t *undo_buf;
            wchar_t *redo_buf;
            int      capacity;        /* in wchar_t count, includes null terminator */
            int      len;
            int      undo_len;        /* -1 = nothing saved */
            int      redo_len;        /* -1 = nothing saved */
            int      cursor;
            int      sel_start;
            int      sel_end;
            int      drag_sel_anchor;
            int      undo_cursor;
            int      redo_cursor;
            int      readonly;
            float    sb_reveal;      /* 0..1 — keyboard/wheel-driven scrollbar visibility */
            float    scroll_offset;
            float    content_height;
        } textview;

        /* SUI_CTRL_SCROLLVIEW — vertically scrolling content host.
         * Children (parent_id == this scrollview's id) are drawn with a
         * per-frame translate-Y transform of -scroll_y and clipped to the
         * scrollview rect. Thumb-drag state is per-control. */
        struct {
            float scroll_y;          /* current vertical scroll offset, clamped to [0, max] */
            float content_h;         /* logical content height in DIPs (set by user) */
            int   track_dragging;    /* 1 while the user is dragging the scrollbar thumb */
            float track_drag_anchor; /* mouse Y at drag start, in scrollview-local space */
            float drag_start_scroll; /* scroll_y at drag start */
        } scroll;
    } d;
};

#endif /* SUI_FEATURE_CUSTOM_CONTROLS */

/* ---------------------------------------------------------------------------
 * Perf caches — defined here so SuiRenderer / SuiControlHost can own them by
 * value; implementations live in sui_text_cache.h (SUI_IMPLEMENTATION chain).
 * --------------------------------------------------------------------------- */
#ifdef SUI_FEATURE_CUSTOM_CONTROLS

typedef struct SuiTextFormatCache {
    void   *fmt;         /* IDWriteTextFormat*; NULL = empty */
    wchar_t family[64];
    float   size_dip;    /* DIPs, not points — same pt at different DPIs differs */
    int     weight;
    int     italic;
    int     bold_title;  /* 1 = created from title role; 0 = body role */
} SuiTextFormatCache;

#define SUI_LAYOUT_CACHE_SLOTS 8
typedef struct SuiTextLayoutEntry {
    void  *layout;       /* IDWriteTextLayout*; NULL = empty */
    void  *fmt;          /* IDWriteTextFormat* used — for fmt-invalidation sweep */
    UINT32 hash;         /* FNV-1a hash of text */
    int    text_len;     /* wcslen of text — guards against FNV-32 collisions */
    float  max_w, max_h;
    int    tick;
} SuiTextLayoutEntry;
typedef struct SuiTextLayoutCache {
    SuiTextLayoutEntry slots[SUI_LAYOUT_CACHE_SLOTS];
    int tick;
} SuiTextLayoutCache;


#endif /* SUI_FEATURE_CUSTOM_CONTROLS */

/* The renderer's continuous rounded shapes (sui_renderer_fill_rounded_rect and the others): path geometry at the origin, keyed
 * by size, radius, border width and kind, the least recently used replaced. Factory resources: kept across target loss. */
#define SUI_SHAPE_CACHE_SLOTS 32
typedef struct SuiShapeCacheEntry {
    void *geometry;      /* ID2D1PathGeometry*; NULL = empty */
    float w, h, radius, ring;
    int   kind, tick;
} SuiShapeCacheEntry;
typedef struct SuiShapeCache {
    SuiShapeCacheEntry slots[SUI_SHAPE_CACHE_SLOTS];
    int tick;
    unsigned builds;     /* shapes built (tests and diagnostics) */
} SuiShapeCache;

/* ---------------------------------------------------------------------------
 * Renderer
 * --------------------------------------------------------------------------- */
typedef struct SuiRenderer {
    HWND    hwnd;
    /* Legacy mirror of typography.body — kept for backward compatibility.
     * Authoritative state lives in `typography`; updates flow through
     * sui_renderer_set_font / sui_renderer_apply_typography. */
    float   font_size;
    wchar_t font_family[64];

    SuiTypography typography;

    void *d2d_factory;
    void *render_target;
    void *dwrite_factory;
    void *text_params; /* IDWriteRenderingParams: the toolkit's text style */
    void *font_normal;
    void *font_bold;
    void *font_title;
    /* Scratch ID2D1SolidColorBrush — created lazily, recolored via SetColor
     * for every fill/draw call so the hot path doesn't allocate per frame. */
    void *scratch_brush;
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
    /* Single-slot IDWriteTextFormat cache and 8-slot ring-geometry LRU.
     * Eliminate per-frame DWrite allocations on text and ring draw paths. */
    SuiTextFormatCache format_cache;
#endif
    SuiShapeCache      shape_cache;   /* continuous rounded shapes, built on first use */
} SuiRenderer;

/* ---------------------------------------------------------------------------
 * Control host (requires D2D + CUSTOM_CONTROLS)
 * --------------------------------------------------------------------------- */
typedef void (*SuiCtrlClickFn)(int ctrl_id, void *user_data);

/* Focus change callback: fired when sui_ctrl_set_focus moves focus to a
 * different control. old_ctrl_id / new_ctrl_id are the control IDs (not
 * indices); either may be -1 when there is no focused control. */
typedef void (*SuiFocusChangeFn)(int old_ctrl_id, int new_ctrl_id, void *user_data);

/* Forward declaration for combo popup internal state */
struct SuiComboPopup;

#ifdef SUI_FEATURE_CUSTOM_CONTROLS

/* Axis-independent inset rect for sui_begin (all values in DIPs). */
typedef struct SuiRect {
    float left, top, right, bottom;
} SuiRect;

typedef struct SuiTabTransition {
    int   in_use;
    int   ctrl_id;
    void *bitmap;
    float bitmap_w;
    float bitmap_h;
    int   content_active;
    int   dir;
    int   vertical;
    int   from;
    int   to;
    float t;
    float travel;
    int   indicator_ready;
    int   indicator_anim_active;
    float indicator_x;
    float indicator_w;
    float indicator_from_x;
    float indicator_from_w;
    float indicator_to_x;
    float indicator_to_w;
    float indicator_anim_t;
} SuiTabTransition;

/* Create a symmetric SuiRect with the same value on all sides. */
static SuiRect sui_inset(float f) {
    SuiRect r; r.left = r.top = r.right = r.bottom = f; return r;
}

/* Controls what sui_begin does to the existing control list. */
typedef enum SuiClearMode {
    SUI_CLEAR_PAGE = 0,  /* call sui_ctrl_clear_page (safe: frees textview bufs, UIA disconnect) */
    SUI_CLEAR_NONE = 1   /* leave control list untouched (append to existing controls) */
} SuiClearMode;

/* Declarative container types — push/pop stack for vstack / hstack / flow.
 * Internal to the host; consumers interact via sui_begin/vstack/hstack/flow/end.
 * RADIO_GROUP behaves as a vstack with a stored group_id; sui_radio walks up
 * to find the enclosing group. */
typedef enum SuiContainerType {
    SUI_CONT_ROOT        = 0,  /* implicit, pushed by sui_begin */
    SUI_CONT_VSTACK      = 1,
    SUI_CONT_HSTACK      = 2,
    SUI_CONT_FLOW        = 3,
    SUI_CONT_RADIO_GROUP = 4,
    /* SCROLLVIEW and CLIPBOX both wrap a real control (SuiControl with
     * type SCROLLVIEW or CONTAINER) and host nested children that get
     * reparented to that control's id. Children stack vertically like a
     * VSTACK; their content_h is tracked. SCROLLVIEW commits the tracked
     * content_h to the underlying scrollview control on sui_end so the
     * scrollbar appears when content overflows. CLIPBOX just z-scopes /
     * clips. Neither participates in flex distribute_vstack (mixed
     * atom/container vstacks already fall back to legacy placement). */
    SUI_CONT_SCROLLVIEW  = 5,
    SUI_CONT_CLIPBOX     = 6
} SuiContainerType;

typedef struct SuiContainer {
    int   type;
    float x, y, w;          /* placement + available width (w may be 0 = unbounded) */
    float h;                /* available height (0 = unknown). Used by VSTACK two-pass; only set on ROOT and direct-child VSTACK in Phase 2b. */
    float cursor_x, cursor_y;
    float gap;              /* gap between children */
    float pad_l, pad_t, pad_r, pad_b;  /* root: outer inset in px (0 elsewhere) */
    /* hstack: */
    int   hstack_n;
    int   hstack_idx;
    float child_w;          /* per-child width = (w - (n-1)*gap) / n */
    float child_max;        /* max child intrinsic width — equal-share min = n * child_max + (n-1)*gap */
    /* flow: */
    float item_h;
    float row_max_h;
    /* intrinsic content width — bubbles via sui_end */
    float intrinsic_w;
    /* radio_group: shared group_id passed to sui_ctrl_add_radio */
    int   radio_group_id;
    /* Phase 2b flex slots — atom-only direct children for HSTACK/VSTACK
     * two-pass distribution. Recorded by control wrappers via
     * sui__decl_record_atom; consumed at sui_end. Containers and overflow
     * children are not recorded — they keep legacy placement. Distribute
     * sweeps slots for flex hints; no separate dirty flag.
     * `n_total_children` counts every direct child (atoms + containers) so
     * VSTACK distribute can refuse to run when a non-atom sibling exists
     * (would otherwise misplace the atoms). */
    int   flex_slot_ids[SUI_MAX_FLEX_SLOTS];
    int   flex_slot_count;
    int   n_total_children;
    /* Reparent target for descendant atoms — set by sui_scrollview /
     * sui_clipbox to their own control id; -1 means "no reparent" (default).
     * sui__decl_record_atom walks up the container stack and applies the
     * nearest non-(-1) value to every atom's parent_id, so nested layout
     * containers (vstack/hstack) inside a scrollview still bind their
     * leaves to the scrollview. */
    int   parent_ctrl_id;
} SuiContainer;

typedef struct SuiControlHost {
    HWND                  hwnd;
    SuiRenderer          *renderer;
    const SuiColorScheme *scheme;
    UINT                  dpi;

    SuiControl controls[SUI_MAX_CONTROLS];
    int        control_count;
    int        focused_control;

    int   persistent_control_count;
    int   dragging_scrollbar;
    int   edit_dragging;
    float drag_start_y;
    float drag_start_scroll;
    int   hover_control;

    int   spinner_repeat_control; /* control index + 1; zero when idle */
    int   spinner_repeat_direction;
    int   spinner_repeat_fast;
    DWORD spinner_repeat_due;

    SuiCtrlClickFn on_click;
    void          *on_click_data;

    SuiFocusChangeFn on_focus_change;
    void            *on_focus_change_data;

    int caret_visible;
    int window_has_focus;
    int keyboard_focus;  /* 1 = last focus change was via keyboard (Tab); 0 = mouse click */

    int   tooltip_active_ctrl;    /* hovered control whose tooltip delay is running */
    int   tooltip_visible_ctrl;   /* control whose tooltip popup is currently shown */
    DWORD tooltip_hover_start_ms;
    HWND  tooltip_hwnd;
    SuiRenderer tooltip_renderer;
    const wchar_t *tooltip_text;

    struct SuiComboPopup *combo_popup;

    void *uia_host_provider;
    void *uia_providers[SUI_MAX_CONTROLS];

    SuiVsyncState vsync;
    int           hover_vsync_active;

    /* Optional explicit min client size (px). Per-axis OVERRIDE of
     * sui_handle_wm_getminmaxinfo's auto-scan and declarative bubble:
     * when set (> 0), this value replaces both for that axis; when 0,
     * the axis falls back to max(scan, bubble). Set only the axis whose
     * scan is unreliable (e.g. width when controls reposition with
     * content_w, or height when vstack grow rewrites c->h/c->y on
     * children). The other axis keeps its scan + bubble floor so the
     * no-overlap guarantee still holds. */
    float min_client_w;
    float min_client_h;

    /* Top-level min-width strategy for sui_ctrl_min_size (parent_id == -1).
     *
     * SUI_MIN_SIZE_DEFAULT (0): per Y-band row, width = max(ex+ew) for
     * non-anchored + max(ew + right_margin) for anchored. Correct when child
     * X positions are stable across resize (e.g. fixed at create time, or
     * captured-at-create installer pages).
     *
     * SUI_MIN_SIZE_FLEX_ROWS (1): per Y-band row, non-anchored part becomes
     * sum(ew) + (n-1)*4dp; anchored part unchanged. Required when child X
     * scales with W (e.g. col_w = 0.5 * content_w) and the consumer rebuilds
     * controls on WM_SIZE — without this, max_left tracks W down and the
     * window can shrink past intrinsic content. */
    int min_size_strategy;

    /* Declarative container stack. Populated by sui_begin / vstack / hstack /
     * flow; popped by sui_end. `intrinsic_min_w` / `intrinsic_min_h` accumulate
     * the root container's minimum content extent during layout and are
     * consumed by sui_handle_wm_getminmaxinfo so consumers don't have to
     * hard-code it. `intrinsic_min_h` is `consumed_h + 2*pad` from the root
     * sui_end — apps with imperative content beyond the tree (e.g. a bottom
     * bar) should add that reserve to `min_client_h`. */
    SuiContainer container_stack[SUI_MAX_CONTAINER_DEPTH];
    int          container_depth;
    float        intrinsic_min_w;
    float        intrinsic_min_h;

    /* 8-slot LRU IDWriteTextLayout cache. Eliminates per-keystroke
     * CreateTextLayout in edit / textview / spinner hit-test paths. */
    SuiTextLayoutCache layout_cache;

    SuiTabTransition tab_transitions[SUI_MAX_TAB_TRANSITIONS];
} SuiControlHost;

#endif /* SUI_FEATURE_CUSTOM_CONTROLS */

/* ---------------------------------------------------------------------------
 * Layout helper
 * --------------------------------------------------------------------------- */
typedef struct SuiLayout {
    float x;
    float y;
    float width;
    float height;
    float gap;
    int   dir;   /* 0 = col (advances Y), 1 = row (advances X) */
    float used;
} SuiLayout;

/* ---------------------------------------------------------------------------
 * Vsync animation pump (opt-in, standalone — no D2D dep)
 * --------------------------------------------------------------------------- */
#ifdef SUI_FEATURE_VSYNC
/* Included near the top of this header so SuiControlHost can own one vblank
 * pump per HWND. */
#endif

/* ---------------------------------------------------------------------------
 * Implementation (single-header pattern)
 * --------------------------------------------------------------------------- */
#ifdef SUI_IMPLEMENTATION

/* objbase.h provides IUnknown, CoCreateInstance, CLSCTX_INPROC_SERVER
 * (needed by sui_d2d.h and sui_renderer.h but not included by WIN32_LEAN_AND_MEAN) */
#include <objbase.h>
/* windowsx.h provides GET_X_LPARAM / GET_Y_LPARAM used by sui_default_msg. */
#include <windowsx.h>

/* REFIID is `const GUID* const` in C mode but `const GUID&` in C++ mode. The
 * slim-ui code is written in a C idiom where call sites pass `&IID_X` (a
 * pointer). To stay portable between C (e.g. an installer) and C++ (e.g.
 * settings window embedded inside a C++ TU), wrap the IID argument in
 * SUI_IID_REF(): in C it expands to `&(x)` (pointer), in C++ it expands to
 * `(x)` (reference). Applies to D2D1CreateFactory, DWriteCreateFactory,
 * CoCreateInstance, IsEqualGUID, etc. */
#ifdef __cplusplus
#  define SUI_IID_REF(x) (x)
#else
#  define SUI_IID_REF(x) (&(x))
#endif

/* ── Memory allocation ───────────────────────────────────────────────────── */
/* Override the allocator by defining SUI_MALLOC and SUI_FREE before including
 * slim_ui.h (define both or neither):
 *   #define SUI_MALLOC(n)  my_alloc(n)
 *   #define SUI_FREE(p)    my_free(p)
 * Without an override: SUI_NO_CRT → HeapAlloc/HeapFree; otherwise malloc/free. */
#ifdef SUI_NO_CRT
static HANDLE sui__heap(void) { static HANDLE h = NULL; if (!h) h = GetProcessHeap(); return h; }
#endif
#ifndef SUI_MALLOC
#  ifdef SUI_NO_CRT
#    define SUI_MALLOC(n) HeapAlloc(sui__heap(), 0, (n))
#    define SUI_FREE(p)   do { if (p) HeapFree(sui__heap(), 0, (p)); } while (0)
#  else
#    include <stdlib.h>
#    define SUI_MALLOC(n) malloc(n)
#    define SUI_FREE(p)   free(p)
#  endif
#endif
static void* sui_malloc(size_t n) { return SUI_MALLOC(n); }
static void  sui_free(void* p)    { SUI_FREE(p); }

/* ── DPI helper ──────────────────────────────────────────────────────────── */
static UINT sui_get_window_dpi(HWND hwnd) {
    typedef UINT (WINAPI *Fn)(HWND);
    Fn f = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (f) { UINT d = f(hwnd); if (d > 0) return d; }
    { HDC dc = GetDC(hwnd); UINT d = (UINT)GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(hwnd, dc); return d?d:96; }
}

/* ── Built-in color schemes ──────────────────────────────────────────────── */
static const SuiColorScheme sui_scheme_light = {
    0xFFFFFF, 0x000000, 0x444444,                           /* base — white */
    0xFBFBFB, 0xCCCCCC, 0x1A1A1A, 0xE8E8E8, 0xDDDDDD, 0x0078D4,  /* buttons */
    0x767676, 0x0078D4, 0xFFFFFF,                           /* checkbox */
    0xFFFFFF, 0xCCCCCC, 0x000000, 0x0078D4, 0xB4D6FA, 0x999999,  /* input */
    0xD2D2D2, 0x9A9A9A, 0x9A9A9A,                           /* slider */
    0xF3F3F3, 0xE8E8E8, 0xDFDFDF, 0xE0E0E0,               /* nav */
    0xE0E0E0, 0x4CAF50,                                     /* progress */
    0xF0F0F0, 0xDFDFDF,                                     /* btn_area */
};

static const SuiColorScheme sui_scheme_dark = {
    0x1E1E1E, 0xE0E0E0, 0x999999,
    0x3A3A3A, 0x555555, 0xE0E0E0, 0x444444, 0x333333, 0xCC6E1F,
    0x999999, 0xCC6E1F, 0xFFFFFF,
    0x2D2D2D, 0x555555, 0xE0E0E0, 0xCC6E1F, 0x4A2D14, 0x666666,
    0x555555, 0xCC6E1F, 0xCC6E1F,
    0x2A2A2A, 0x262626, 0x3A3A3A, 0x3D3D3D,
    0x404040, 0x5CBF60,
    0x2D2D2D, 0x3D3D3D,
};

/* ── Scheme registry ─────────────────────────────────────────────────────────
 * Apps register named SuiColorScheme variants and look them up at runtime.
 * Built-ins "light" and "dark" are auto-registered on first call. Apps may
 * register custom schemes (sui_scheme_register) and switch the host scheme
 * by name (sui_scheme_find). Registering a name that already exists overwrites
 * the entry, allowing apps to override the built-in defaults.
 *
 * Single-threaded — call only on the UI thread. Storage is per-binary.
 * --------------------------------------------------------------------------- */
#ifndef SUI_MAX_SCHEMES
#define SUI_MAX_SCHEMES 16
#endif
#define SUI_SCHEME_NAME_MAX 32

typedef struct SuiSchemeEntry {
    char           name[SUI_SCHEME_NAME_MAX];
    SuiColorScheme scheme;
    int            in_use;
} SuiSchemeEntry;

static SuiSchemeEntry sui__scheme_registry[SUI_MAX_SCHEMES];
static int            sui__scheme_registry_ready = 0;

static int sui__streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

static void sui__strncpy_z(char* dst, const char* src, int cap) {
    int i = 0;
    if (cap <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void sui__wstrncpy_z(wchar_t* dst, const wchar_t* src, int cap) {
    int i = 0;
    if (cap <= 0) return;
    if (!src) { dst[0] = L'\0'; return; }
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = L'\0';
}

/* UTF-16 codepoint-aware cursor movement. A UTF-16 surrogate pair encodes one
 * codepoint as a high surrogate (0xD800..0xDBFF) followed by a low surrogate
 * (0xDC00..0xDFFF). Cursor movement must skip both halves as a unit so the
 * caret never lands between them. */
static int sui__codepoint_left(const wchar_t* s, int pos) {
    if (pos <= 0) return 0;
    pos--;
    if (pos > 0
        && s[pos]   >= 0xDC00 && s[pos]   <= 0xDFFF
        && s[pos-1] >= 0xD800 && s[pos-1] <= 0xDBFF)
        pos--;
    return pos;
}

static int sui__codepoint_right(const wchar_t* s, int pos, int len) {
    if (pos >= len) return len;
    pos++;
    if (pos < len
        && s[pos-1] >= 0xD800 && s[pos-1] <= 0xDBFF
        && s[pos]   >= 0xDC00 && s[pos]   <= 0xDFFF)
        pos++;
    return pos;
}

static int sui__scheme_slot_for(const char* name, int* out_free) {
    int i, free_slot = -1;
    for (i = 0; i < SUI_MAX_SCHEMES; i++) {
        if (sui__scheme_registry[i].in_use) {
            if (sui__streq(sui__scheme_registry[i].name, name)) return i;
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }
    if (out_free) *out_free = free_slot;
    return -1;
}

static int sui_scheme_register(const char* name, const SuiColorScheme* scheme) {
    int slot, free_slot = -1;
    if (!name || !*name || !scheme) return -1;
    slot = sui__scheme_slot_for(name, &free_slot);
    if (slot < 0) {
        if (free_slot < 0) return -1;
        slot = free_slot;
        sui__strncpy_z(sui__scheme_registry[slot].name, name, SUI_SCHEME_NAME_MAX);
        sui__scheme_registry[slot].in_use = 1;
    }
    sui__scheme_registry[slot].scheme = *scheme;
    return slot;
}

static void sui__scheme_registry_init(void) {
    if (sui__scheme_registry_ready) return;
    sui__scheme_registry_ready = 1;
    sui_scheme_register("light", &sui_scheme_light);
    sui_scheme_register("dark",  &sui_scheme_dark);
}

static const SuiColorScheme* sui_scheme_find(const char* name) {
    int slot;
    sui__scheme_registry_init();
    if (!name) return NULL;
    slot = sui__scheme_slot_for(name, NULL);
    return slot < 0 ? NULL : &sui__scheme_registry[slot].scheme;
}

static int sui_scheme_list(const char** out_names, int max) {
    int i, n = 0;
    sui__scheme_registry_init();
    if (!out_names || max <= 0) return 0;
    for (i = 0; i < SUI_MAX_SCHEMES && n < max; i++) {
        if (sui__scheme_registry[i].in_use)
            out_names[n++] = sui__scheme_registry[i].name;
    }
    return n;
}

static int sui_scheme_unregister(const char* name) {
    int slot;
    if (!name) return 0;
    sui__scheme_registry_init();
    slot = sui__scheme_slot_for(name, NULL);
    if (slot < 0) return 0;
    sui__scheme_registry[slot].in_use = 0;
    sui__scheme_registry[slot].name[0] = '\0';
    return 1;
}

/* ── Typography registry — see SuiTypography typedef near top of header ─── */
static const SuiTypography sui_typography_default = {
    /* body */     { L"Segoe UI",                 11.0f, 400, 0 },
    /* subtitle */ { L"Segoe UI",                 11.0f, 400, 0 },
    /* title */    { L"Segoe UI Variable Display", 22.0f, 600, 0 },
    /* icon */     { L"Segoe Fluent Icons",       11.0f, 400, 0 },
};

#ifndef SUI_MAX_TYPOGRAPHIES
#define SUI_MAX_TYPOGRAPHIES 8
#endif

typedef struct SuiTypographyEntry {
    char          name[SUI_SCHEME_NAME_MAX];
    SuiTypography typography;
    int           in_use;
} SuiTypographyEntry;

static SuiTypographyEntry sui__typo_registry[SUI_MAX_TYPOGRAPHIES];
static int                sui__typo_registry_ready = 0;

static int sui__typo_slot_for(const char* name, int* out_free) {
    int i, free_slot = -1;
    for (i = 0; i < SUI_MAX_TYPOGRAPHIES; i++) {
        if (sui__typo_registry[i].in_use) {
            if (sui__streq(sui__typo_registry[i].name, name)) return i;
        } else if (free_slot < 0) {
            free_slot = i;
        }
    }
    if (out_free) *out_free = free_slot;
    return -1;
}

static int sui_typography_register(const char* name, const SuiTypography* typo) {
    int slot, free_slot = -1;
    if (!name || !*name || !typo) return -1;
    slot = sui__typo_slot_for(name, &free_slot);
    if (slot < 0) {
        if (free_slot < 0) return -1;
        slot = free_slot;
        sui__strncpy_z(sui__typo_registry[slot].name, name, SUI_SCHEME_NAME_MAX);
        sui__typo_registry[slot].in_use = 1;
    }
    sui__typo_registry[slot].typography = *typo;
    return slot;
}

static void sui__typo_registry_init(void) {
    if (sui__typo_registry_ready) return;
    sui__typo_registry_ready = 1;
    sui_typography_register("default", &sui_typography_default);
}

static const SuiTypography* sui_typography_find(const char* name) {
    int slot;
    sui__typo_registry_init();
    if (!name) return NULL;
    slot = sui__typo_slot_for(name, NULL);
    return slot < 0 ? NULL : &sui__typo_registry[slot].typography;
}

static int sui_typography_list(const char** out_names, int max) {
    int i, n = 0;
    sui__typo_registry_init();
    if (!out_names || max <= 0) return 0;
    for (i = 0; i < SUI_MAX_TYPOGRAPHIES && n < max; i++) {
        if (sui__typo_registry[i].in_use)
            out_names[n++] = sui__typo_registry[i].name;
    }
    return n;
}

static int sui_typography_unregister(const char* name) {
    int slot;
    if (!name) return 0;
    sui__typo_registry_init();
    slot = sui__typo_slot_for(name, NULL);
    if (slot < 0) return 0;
    sui__typo_registry[slot].in_use = 0;
    sui__typo_registry[slot].name[0] = '\0';
    return 1;
}

/* ── Control lookup helper ───────────────────────────────────────────────── */
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
static SuiControl* sui_ctrl_find(SuiControlHost* host, int id) {
    int i;
    for (i = 0; i < host->control_count; i++)
        if (host->controls[i].id == id) return &host->controls[i];
    return NULL;
}

static int sui_ctrl_style_has(const SuiControl* c, uint32_t prop) {
    return c && c->style && (c->style->mask & prop);
}

static uint32_t sui_ctrl_style_color(const SuiControl* c, uint32_t prop, uint32_t fallback) {
    if (!sui_ctrl_style_has(c, prop)) return fallback;
    switch (prop) {
    case SUI_STYLE_TEXT_COLOR:        return c->style->text_color;
    case SUI_STYLE_BG_COLOR:          return c->style->bg_color;
    case SUI_STYLE_BORDER_COLOR:      return c->style->border_color;
    case SUI_STYLE_HOVER_COLOR:       return c->style->hover_color;
    case SUI_STYLE_PRESS_COLOR:       return c->style->press_color;
    case SUI_STYLE_SELECTED_COLOR:    return c->style->selected_color;
    case SUI_STYLE_FILL_COLOR:        return c->style->fill_color;
    case SUI_STYLE_ACCENT_COLOR:      return c->style->accent_color;
    case SUI_STYLE_PLACEHOLDER_COLOR: return c->style->placeholder_color;
    case SUI_STYLE_SELECTION_COLOR:   return c->style->selection_color;
    default: return fallback;
    }
}

static const wchar_t* sui_ctrl_style_font_family(const SuiControl* c) {
    if (!sui_ctrl_style_has(c, SUI_STYLE_FONT_FAMILY)) return NULL;
    return c->style->font_family[0] ? c->style->font_family : NULL;
}

static float sui_ctrl_style_font_size(const SuiControl* c, float fallback) {
    return (c && c->font_size > 0.0f) ? c->font_size : fallback;
}

#endif

/* ── Include chain ───────────────────────────────────────────────────────── */
#include "sui_d2d.h"
#ifdef SUI_FEATURE_CUSTOM_CONTROLS
#include "sui_text_cache.h"
#endif
#include "sui_renderer.h"
#include "sui_layout.h"

#ifdef SUI_FEATURE_CUSTOM_CONTROLS
#include "sui_ctrl_button.h"
#include "sui_ctrl_checkbox.h"
#include "sui_ctrl_label.h"
#ifndef SUI_NO_TOOLTIP
#include "sui_ctrl_tooltip.h"
#endif
#include "sui_ctrl_radio.h"
#ifndef SUI_NO_COLOR_SWATCH
#include "sui_ctrl_color_swatch.h"
#endif
#ifndef SUI_NO_SLIDER
#include "sui_ctrl_slider.h"
#endif
#ifndef SUI_NO_SPINNER
#include "sui_ctrl_spinner.h"
#endif
#include "sui_ctrl_edit.h"
#ifndef SUI_NO_COMBOBOX
#include "sui_ctrl_combobox.h"
#endif
#ifndef SUI_NO_NAV_LIST
#include "sui_ctrl_nav_list.h"
#endif
#ifndef SUI_NO_LIST
#include "sui_ctrl_list.h"
#endif
#ifndef SUI_NO_TABS
#include "sui_ctrl_tabs.h"
#endif
#ifndef SUI_NO_IMAGE
#include "sui_ctrl_image.h"
#endif
#ifndef SUI_NO_SHORTCUT_RECORDER
#include "sui_shortcut_keys.h"
#include "sui_ctrl_shortcut_recorder.h"
#endif
#include "sui_ctrl_progress.h"
#include "sui_ctrl_container.h"
#include "sui_ctrl_textview.h"
#ifndef SUI_NO_SCROLLVIEW
#include "sui_ctrl_scrollview.h"
#endif
#include "sui_ctrl_omit.h"
#include "sui_controls.h"
/* Declarative container API (sui_begin / sui_vstack / sui_hstack / sui_end +
 * flex distribution) is opt-in via SUI_FEATURE_DECL. Imperative consumers
 * (an installer wizard, a settings window) place controls by hand and never
 * call into it; gating saves compile time on their TUs. The flex hint fields
 * on SuiControl and the container_stack on SuiControlHost stay regardless —
 * they're plain data and cost only a few hundred bytes per host. */
#ifdef SUI_FEATURE_DECL
#include "sui_decl.h"
#endif
#include "sui_uia.h"
#include "sui_uia_providers.h"
#endif

#endif /* SUI_IMPLEMENTATION */

#endif /* SLIM_UI_H */
