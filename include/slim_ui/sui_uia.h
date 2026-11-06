/* sui_uia.h -- UIA COM vtable definitions for Slim UI accessibility providers.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 * Requires SUI_FEATURE_CUSTOM_CONTROLS.
 *
 * Pattern: same manual C vtable approach as sui_d2d.h.
 * Use void* for vtable slots we don't call; typed function pointers for ones we do.
 * No #pragma comment(lib) for uiautomationcore -- loaded dynamically.
 */

#include <oaidl.h>  /* VARIANT */

/* ── UIA Property IDs ──────────────────────────────────────────────────────── */
#define SUI_UIA_BoundingRectanglePropertyId       30001
#define SUI_UIA_ControlTypePropertyId             30003
#define SUI_UIA_NamePropertyId                    30005
#define SUI_UIA_HasKeyboardFocusPropertyId        30008
#define SUI_UIA_IsKeyboardFocusablePropertyId     30009
#define SUI_UIA_IsEnabledPropertyId               30010
#define SUI_UIA_IsOffscreenPropertyId             30022
#define SUI_UIA_AutomationIdPropertyId            30011
#define SUI_UIA_IsControlElementPropertyId        30016
#define SUI_UIA_IsContentElementPropertyId        30017
#define SUI_UIA_ValueValuePropertyId              30045
#define SUI_UIA_RangeValueValuePropertyId         30047

/* Sentinel for GetRuntimeId SAFEARRAY: [UiaAppendRuntimeId, unique_id].
 * UIA prepends the HWND-based segment; controls supply [3, id] to form
 * the full runtime ID = [hwnd_part..., 3, id]. */
#define SUI_UIA_AppendRuntimeId 3

/* ── UIA Control Type IDs ──────────────────────────────────────────────────── */
#define SUI_UIA_ButtonControlTypeId               50000
#define SUI_UIA_CheckBoxControlTypeId             50002
#define SUI_UIA_ComboBoxControlTypeId             50003
#define SUI_UIA_EditControlTypeId                 50004
#define SUI_UIA_ListControlTypeId                 50008
#define SUI_UIA_RadioButtonControlTypeId          50013
#define SUI_UIA_SliderControlTypeId               50015
#define SUI_UIA_SpinnerControlTypeId              50016
#define SUI_UIA_TabControlTypeId                  50018
#define SUI_UIA_TextControlTypeId                 50020
#define SUI_UIA_DocumentControlTypeId             50030

/* ── UIA Pattern IDs ───────────────────────────────────────────────────────── */
#define SUI_UIA_InvokePatternId                   10000
#define SUI_UIA_SelectionPatternId                10001
#define SUI_UIA_ValuePatternId                    10002
#define SUI_UIA_RangeValuePatternId               10003
#define SUI_UIA_ScrollPatternId                   10004
#define SUI_UIA_ExpandCollapsePatternId           10005
#define SUI_UIA_SelectionItemPatternId            10010
#define SUI_UIA_TogglePatternId                   10015

/* ── Interface GUIDs ───────────────────────────────────────────────────────── */
static const GUID SUI_IID_IRawElementProviderSimple =
    {0xd6dd68d1, 0x86fd, 0x4332, {0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c}};
static const GUID SUI_IID_IRawElementProviderFragment =
    {0xf7063da8, 0x8359, 0x439c, {0x92, 0x97, 0xbb, 0xc5, 0x29, 0x9a, 0x7d, 0x87}};
static const GUID SUI_IID_IRawElementProviderFragmentRoot =
    {0x620ce2a5, 0xab8f, 0x40a9, {0x86, 0xcb, 0xde, 0x3c, 0x75, 0x59, 0x9b, 0x58}};
static const GUID SUI_IID_IInvokeProvider =
    {0x54fcb24b, 0xe18e, 0x47a2, {0xb4, 0xd3, 0xec, 0xcb, 0xe7, 0x75, 0x99, 0xa2}};
static const GUID SUI_IID_IToggleProvider =
    {0x56d00bd0, 0xc4f4, 0x433c, {0xa8, 0x36, 0x1a, 0x52, 0xa5, 0x7e, 0x08, 0x92}};
static const GUID SUI_IID_IValueProvider =
    {0xc7935180, 0x6fb3, 0x4201, {0xb1, 0x74, 0x7d, 0xf7, 0x3a, 0xdb, 0xf6, 0x4a}};
static const GUID SUI_IID_IRangeValueProvider =
    {0x36dc7aef, 0x33e6, 0x4691, {0xaf, 0xe1, 0x2b, 0xe7, 0x27, 0x4b, 0x3d, 0x33}};
static const GUID SUI_IID_IExpandCollapseProvider =
    {0xd847d3a5, 0xcab0, 0x4a98, {0x8c, 0x32, 0xec, 0xb4, 0x5c, 0x59, 0xad, 0x24}};
static const GUID SUI_IID_ISelectionProvider =
    {0xfb8b03af, 0x3bdf, 0x48d4, {0xbd, 0x36, 0x1a, 0x65, 0x79, 0x3b, 0xe1, 0x68}};
static const GUID SUI_IID_ISelectionItemProvider =
    {0x2acad808, 0xb2d4, 0x452d, {0xa4, 0x07, 0x91, 0xff, 0x1a, 0xd1, 0x67, 0xb2}};
static const GUID SUI_IID_IScrollProvider =
    {0xb38b8077, 0x1fc3, 0x42a5, {0x8c, 0xae, 0xd4, 0x0c, 0x22, 0x15, 0x05, 0x5a}};

/* ── UIA Rect type (not to be confused with D2D rect) ─────────────────────── */
typedef struct SuiUiaRect {
    double left;
    double top;
    double width;
    double height;
} SuiUiaRect;

/* ── IRawElementProviderSimple vtable ──────────────────────────────────────── */
typedef struct SuiUiaProviderSimpleVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IRawElementProviderSimple */
    HRESULT (STDMETHODCALLTYPE *get_ProviderOptions)(void* self, int* out);
    HRESULT (STDMETHODCALLTYPE *GetPatternProvider)(void* self, int patternId, IUnknown** out);
    HRESULT (STDMETHODCALLTYPE *GetPropertyValue)(void* self, int propertyId, VARIANT* out);
    HRESULT (STDMETHODCALLTYPE *get_HostRawElementProvider)(void* self, void** out);
} SuiUiaProviderSimpleVtbl;

/* ── IRawElementProviderFragment vtable ────────────────────────────────────── */
typedef struct SuiUiaProviderFragmentVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IRawElementProviderFragment */
    HRESULT (STDMETHODCALLTYPE *Navigate)(void* self, int direction, void** out);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeId)(void* self, SAFEARRAY** out);
    HRESULT (STDMETHODCALLTYPE *get_BoundingRectangle)(void* self, SuiUiaRect* out);
    HRESULT (STDMETHODCALLTYPE *GetEmbeddedFragmentRoots)(void* self, SAFEARRAY** out);
    HRESULT (STDMETHODCALLTYPE *SetFocus)(void* self);
    HRESULT (STDMETHODCALLTYPE *get_FragmentRoot)(void* self, void** out);
} SuiUiaProviderFragmentVtbl;

/* ── IRawElementProviderFragmentRoot vtable ────────────────────────────────── */
typedef struct SuiUiaProviderFragmentRootVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IRawElementProviderFragmentRoot */
    HRESULT (STDMETHODCALLTYPE *ElementProviderFromPoint)(void* self, double x, double y, void** out);
    HRESULT (STDMETHODCALLTYPE *GetFocus)(void* self, void** out);
} SuiUiaProviderFragmentRootVtbl;

/* ── IInvokeProvider vtable ────────────────────────────────────────────────── */
typedef struct SuiUiaInvokeProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IInvokeProvider */
    HRESULT (STDMETHODCALLTYPE *Invoke)(void* self);
} SuiUiaInvokeProviderVtbl;

/* ── IToggleProvider vtable ────────────────────────────────────────────────── */
typedef struct SuiUiaToggleProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IToggleProvider */
    HRESULT (STDMETHODCALLTYPE *Toggle)(void* self);
    HRESULT (STDMETHODCALLTYPE *get_ToggleState)(void* self, int* out);
} SuiUiaToggleProviderVtbl;

/* ── IValueProvider vtable ─────────────────────────────────────────────────── */
typedef struct SuiUiaValueProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IValueProvider */
    HRESULT (STDMETHODCALLTYPE *SetValue)(void* self, LPCWSTR val);
    HRESULT (STDMETHODCALLTYPE *get_Value)(void* self, BSTR* out);
    HRESULT (STDMETHODCALLTYPE *get_IsReadOnly)(void* self, BOOL* out);
} SuiUiaValueProviderVtbl;

/* ── IRangeValueProvider vtable ────────────────────────────────────────────── */
typedef struct SuiUiaRangeValueProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IRangeValueProvider */
    HRESULT (STDMETHODCALLTYPE *SetValue)(void* self, double val);
    HRESULT (STDMETHODCALLTYPE *get_Value)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_IsReadOnly)(void* self, BOOL* out);
    HRESULT (STDMETHODCALLTYPE *get_Maximum)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_Minimum)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_LargeChange)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_SmallChange)(void* self, double* out);
} SuiUiaRangeValueProviderVtbl;

/* ── IExpandCollapseProvider vtable ────────────────────────────────────────── */
typedef struct SuiUiaExpandCollapseProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IExpandCollapseProvider */
    HRESULT (STDMETHODCALLTYPE *Expand)(void* self);
    HRESULT (STDMETHODCALLTYPE *Collapse)(void* self);
    HRESULT (STDMETHODCALLTYPE *get_ExpandCollapseState)(void* self, int* out);
} SuiUiaExpandCollapseProviderVtbl;

/* ── ISelectionProvider vtable ─────────────────────────────────────────────── */
typedef struct SuiUiaSelectionProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* ISelectionProvider */
    HRESULT (STDMETHODCALLTYPE *GetSelection)(void* self, SAFEARRAY** out);
    HRESULT (STDMETHODCALLTYPE *get_CanSelectMultiple)(void* self, BOOL* out);
    HRESULT (STDMETHODCALLTYPE *get_IsSelectionRequired)(void* self, BOOL* out);
} SuiUiaSelectionProviderVtbl;

/* ── ISelectionItemProvider vtable ─────────────────────────────────────────── */
typedef struct SuiUiaSelectionItemProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* ISelectionItemProvider */
    HRESULT (STDMETHODCALLTYPE *Select)(void* self);
    HRESULT (STDMETHODCALLTYPE *AddToSelection)(void* self);
    HRESULT (STDMETHODCALLTYPE *RemoveFromSelection)(void* self);
    HRESULT (STDMETHODCALLTYPE *get_IsSelected)(void* self, BOOL* out);
    HRESULT (STDMETHODCALLTYPE *get_SelectionContainer)(void* self, void** out);
} SuiUiaSelectionItemProviderVtbl;

/* ── IScrollProvider vtable ────────────────────────────────────────────────── */
typedef struct SuiUiaScrollProviderVtbl {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* self, REFIID riid, void** out);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* self);
    ULONG   (STDMETHODCALLTYPE *Release)(void* self);
    /* IScrollProvider */
    HRESULT (STDMETHODCALLTYPE *Scroll)(void* self, int horizontalAmount, int verticalAmount);
    HRESULT (STDMETHODCALLTYPE *SetScrollPercent)(void* self, double horizontalPercent, double verticalPercent);
    HRESULT (STDMETHODCALLTYPE *get_HorizontalScrollPercent)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_VerticalScrollPercent)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_HorizontalViewSize)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_VerticalViewSize)(void* self, double* out);
    HRESULT (STDMETHODCALLTYPE *get_HorizontallyScrollable)(void* self, BOOL* out);
    HRESULT (STDMETHODCALLTYPE *get_VerticallyScrollable)(void* self, BOOL* out);
} SuiUiaScrollProviderVtbl;

/* ── Provider instance structs ─────────────────────────────────────────────── */

/* Per-control provider — implements Simple + Fragment + up to 2 pattern interfaces.
 * GetPatternProvider returns &lpPattern1Vtbl or &lpPattern2Vtbl.
 * QueryInterface for Fragment returns &lpFragVtbl.
 * Pattern/fragment methods recover the SuiUiaProvider* base via offsetof macros. */
typedef struct SuiUiaProvider {
    const SuiUiaProviderSimpleVtbl*   lpVtbl;
    const void*                        lpPattern1Vtbl;
    const void*                        lpPattern2Vtbl;
    const SuiUiaProviderFragmentVtbl* lpFragVtbl;
    LONG            ref_count;
    SuiControlHost* host;
    int             control_index;
} SuiUiaProvider;

/* Host (window-level) provider — implements Simple + Fragment + FragmentRoot */
typedef struct SuiUiaHostProvider {
    const SuiUiaProviderSimpleVtbl* lpVtbl;
    /* Fragment and FragmentRoot vtables accessed via QueryInterface */
    LONG            ref_count;
    SuiControlHost* host;
} SuiUiaHostProvider;

/* ── IUnknown boilerplate macro ────────────────────────────────────────────── *
 * Generates QueryInterface / AddRef / Release for a provider struct.          *
 * The QueryInterface here handles the simple provider IID only; actual        *
 * implementations in sui_uia_providers.h expand it to cover fragment IIDs.   */
#define SUI_UIA_IUNKNOWN_IMPL(name, type) \
static HRESULT STDMETHODCALLTYPE name##_QueryInterface(void* self, REFIID riid, void** out) { \
    if (!out) return E_POINTER; \
    if (IsEqualGUID(riid, SUI_IID_REF(IID_IUnknown)) || \
        IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderSimple))) { \
        *out = self; ((type*)self)->ref_count++; return S_OK; \
    } \
    *out = NULL; return E_NOINTERFACE; \
} \
static ULONG STDMETHODCALLTYPE name##_AddRef(void* self) { \
    return (ULONG)InterlockedIncrement(&((type*)self)->ref_count); \
} \
static ULONG STDMETHODCALLTYPE name##_Release(void* self) { \
    LONG rc = InterlockedDecrement(&((type*)self)->ref_count); \
    if (rc == 0) sui_free(self); \
    return (ULONG)rc; \
}

/* ── Dynamic import of UiaReturnRawElementProvider ────────────────────────── *
 * Loaded from uiautomationcore.dll on first use so the binary has no hard    *
 * import dependency on uiautomationcore.lib. A UIA-free host never loads it. */
typedef LRESULT (WINAPI *SuiUiaReturnRawElementProviderFn)(
    HWND hwnd, WPARAM wParam, LPARAM lParam, void* provider);

static SuiUiaReturnRawElementProviderFn sui_uia_get_return_fn(void) {
    static SuiUiaReturnRawElementProviderFn fn = NULL;
    if (!fn) {
        HMODULE h = LoadLibraryW(L"uiautomationcore.dll");
        if (h) fn = (SuiUiaReturnRawElementProviderFn)
                        GetProcAddress(h, "UiaReturnRawElementProvider");
    }
    return fn;
}
