/* sui_uia_providers.h -- UIA provider implementations for Slim UI controls.
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION -- no include guard.
 * Requires SUI_FEATURE_CUSTOM_CONTROLS and sui_uia.h included before this.
 *
 * All UIA/OLE functions loaded dynamically -- no static link to uiautomationcore or oleaut32.
 */

/* ── Navigation directions (from UIAutomationCore.h) ─────────────────────── */
#define SUI_UIA_NavigateDirection_Parent          0
#define SUI_UIA_NavigateDirection_NextSibling     1
#define SUI_UIA_NavigateDirection_PreviousSibling 2
#define SUI_UIA_NavigateDirection_FirstChild      3
#define SUI_UIA_NavigateDirection_LastChild       4

/* ── UiaRootObjectId ─────────────────────────────────────────────────────── */
#define SUI_UIA_RootObjectId (-25)
#define SUI_UIA_E_ELEMENTNOTAVAILABLE ((HRESULT)0x80040201L)
#define SUI_UIA_ProviderOptions_ServerSideProvider 0x2
#define SUI_UIA_ProviderOptions_UseComThreading    0x20

/* ── Inline VariantInit replacement (avoids oleaut32 link dependency) ────── */
static void sui_uia_variant_init(VARIANT* v) {
    memset(v, 0, sizeof(*v));
    v->vt = VT_EMPTY;
}

/* ── Dynamic oleaut32 imports ────────────────────────────────────────────── */
typedef BSTR      (WINAPI *SuiSysAllocStringFn)(const OLECHAR*);
typedef void      (WINAPI *SuiSysFreeStringFn)(BSTR);
typedef SAFEARRAY*(WINAPI *SuiSafeArrayCreateFn)(VARTYPE vt, UINT cDims, SAFEARRAYBOUND* rgsabound);
typedef HRESULT   (WINAPI *SuiSafeArrayPutElementFn)(SAFEARRAY* psa, LONG* rgIndices, void* pv);

static HMODULE sui_uia_oleaut32(void) {
    static HMODULE h = NULL;
    if (!h) h = LoadLibraryW(L"oleaut32.dll");
    return h;
}

static SuiSysAllocStringFn sui_uia_sysalloc(void) {
    static SuiSysAllocStringFn fn = NULL;
    if (!fn) { HMODULE h = sui_uia_oleaut32(); if (h) fn = (SuiSysAllocStringFn)GetProcAddress(h, "SysAllocString"); }
    return fn;
}

static SuiSysFreeStringFn sui_uia_sysfree(void) {
    static SuiSysFreeStringFn fn = NULL;
    if (!fn) { HMODULE h = sui_uia_oleaut32(); if (h) fn = (SuiSysFreeStringFn)GetProcAddress(h, "SysFreeString"); }
    return fn;
}

static SuiSafeArrayCreateFn sui_uia_sa_create(void) {
    static SuiSafeArrayCreateFn fn = NULL;
    if (!fn) { HMODULE h = sui_uia_oleaut32(); if (h) fn = (SuiSafeArrayCreateFn)GetProcAddress(h, "SafeArrayCreate"); }
    return fn;
}

static SuiSafeArrayPutElementFn sui_uia_sa_put(void) {
    static SuiSafeArrayPutElementFn fn = NULL;
    if (!fn) { HMODULE h = sui_uia_oleaut32(); if (h) fn = (SuiSafeArrayPutElementFn)GetProcAddress(h, "SafeArrayPutElement"); }
    return fn;
}

/* ── Dynamic UiaHostProviderFromHwnd ─────────────────────────────────────── */
typedef HRESULT (WINAPI *SuiUiaHostProviderFromHwndFn)(HWND hwnd, void** out);

static SuiUiaHostProviderFromHwndFn sui_uia_get_host_provider_fn(void) {
    static SuiUiaHostProviderFromHwndFn fn = NULL;
    if (!fn) {
        HMODULE h = LoadLibraryW(L"uiautomationcore.dll");
        if (h) fn = (SuiUiaHostProviderFromHwndFn)
                        GetProcAddress(h, "UiaHostProviderFromHwnd");
    }
    return fn;
}

/* ── Dynamic UiaDisconnectProvider ──────────────────────────────────────── */
typedef HRESULT (WINAPI *SuiUiaDisconnectProviderFn)(void* pProvider);

static SuiUiaDisconnectProviderFn sui_uia_get_disconnect_fn(void) {
    static SuiUiaDisconnectProviderFn fn = NULL;
    if (!fn) {
        HMODULE h = LoadLibraryW(L"uiautomationcore.dll");
        if (h) fn = (SuiUiaDisconnectProviderFn)
                        GetProcAddress(h, "UiaDisconnectProvider");
    }
    return fn;
}

/* ── Dynamic UiaRaiseAutomationPropertyChangedEvent ──────────────────────── */
typedef HRESULT (WINAPI *SuiUiaRaisePropChangedFn)(
    void* pProvider, int propertyId, VARIANT oldValue, VARIANT newValue);

static SuiUiaRaisePropChangedFn sui_uia_get_raise_prop_fn(void) {
    static SuiUiaRaisePropChangedFn fn = NULL;
    if (!fn) {
        HMODULE h = LoadLibraryW(L"uiautomationcore.dll");
        if (h) fn = (SuiUiaRaisePropChangedFn)
                        GetProcAddress(h, "UiaRaiseAutomationPropertyChangedEvent");
    }
    return fn;
}

/* ── Host provider: full struct with 3 vtable pointers ───────────────────── */
typedef struct SuiUiaHostProviderFull {
    const SuiUiaProviderSimpleVtbl*       lpVtbl;
    const SuiUiaProviderFragmentVtbl*     lpFragVtbl;
    const SuiUiaProviderFragmentRootVtbl* lpFragRootVtbl;
    LONG            ref_count;
    SuiControlHost* host;
} SuiUiaHostProviderFull;

/* Forward declarations */
static void* sui_uia_create_control_provider(SuiControlHost* host, int ctrl_index);
static void* sui_uia_create_host_provider(SuiControlHost* host);
static void* sui_uia_get_ctrl_provider(SuiControlHost* host, int idx);

/* ── Pattern interface recovery macros ───────────────────────────────────── *
 * Pattern vtable pointers live at known offsets inside SuiUiaProvider.     *
 * When UIA calls a pattern method, `self` points to lpPattern1Vtbl or     *
 * lpPattern2Vtbl. These macros recover the containing SuiUiaProvider*.    */
#define SUI_UIA_FROM_PAT1(self) \
    ((SuiUiaProvider*)((char*)(self) - offsetof(SuiUiaProvider, lpPattern1Vtbl)))
#define SUI_UIA_FROM_PAT2(self) \
    ((SuiUiaProvider*)((char*)(self) - offsetof(SuiUiaProvider, lpPattern2Vtbl)))
#define SUI_CTRL_PROV_FROM_FRAG(self) \
    ((SuiUiaProvider*)((char*)(self) - offsetof(SuiUiaProvider, lpFragVtbl)))

/* Providers can be retained by UI Automation after the HWND/host storage is
 * gone. Every method that touches a control resolves through this guard; host
 * teardown clears p->host before releasing its cached COM reference. */
static HRESULT sui_uia_resolve_control(SuiUiaProvider* p,
                                       SuiControlHost** out_host,
                                       SuiControl** out_ctrl) {
    SuiControlHost* host;
    if (!p || !p->host) return SUI_UIA_E_ELEMENTNOTAVAILABLE;
    host = p->host;
    if (p->control_index < 0 || p->control_index >= host->control_count)
        return SUI_UIA_E_ELEMENTNOTAVAILABLE;
    if (out_host) *out_host = host;
    if (out_ctrl) *out_ctrl = &host->controls[p->control_index];
    return S_OK;
}

static HRESULT sui_uia_resolve_host(SuiUiaHostProviderFull* h,
                                    SuiControlHost** out_host) {
    if (!h || !h->host) return SUI_UIA_E_ELEMENTNOTAVAILABLE;
    if (out_host) *out_host = h->host;
    return S_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
   PER-CONTROL PROVIDER (IRawElementProviderSimple)
   ══════════════════════════════════════════════════════════════════════════ */

/* ── Control provider: IUnknown ──────────────────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_get_pattern(void* self, int patternId, IUnknown** out);

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_qi(void* self, REFIID riid, void** out) {
    SuiUiaProvider* p = (SuiUiaProvider*)self;
    if (!out) return E_POINTER;
    if (IsEqualGUID(riid, SUI_IID_REF(IID_IUnknown)) ||
        IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderSimple))) {
        *out = self;
        InterlockedIncrement(&p->ref_count);
        return S_OK;
    }
    if (IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderFragment))) {
        *out = &p->lpFragVtbl;
        InterlockedIncrement(&p->ref_count);
        return S_OK;
    }
    /* UIA queries the returned IUnknown for the actual pattern interface. */
#define SUI_UIA_QUERY_PATTERN(name) \
    if (IsEqualGUID(riid, SUI_IID_REF(SUI_IID_I##name##Provider))) \
        return sui_uia_ctrl_get_pattern(self, SUI_UIA_##name##PatternId, (IUnknown**)out)
    SUI_UIA_QUERY_PATTERN(Invoke);
    SUI_UIA_QUERY_PATTERN(Toggle);
    SUI_UIA_QUERY_PATTERN(Value);
    SUI_UIA_QUERY_PATTERN(RangeValue);
    SUI_UIA_QUERY_PATTERN(ExpandCollapse);
    SUI_UIA_QUERY_PATTERN(Selection);
    SUI_UIA_QUERY_PATTERN(SelectionItem);
    SUI_UIA_QUERY_PATTERN(Scroll);
#undef SUI_UIA_QUERY_PATTERN
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE sui_uia_ctrl_addref(void* self) {
    return (ULONG)InterlockedIncrement(&((SuiUiaProvider*)self)->ref_count);
}

static ULONG STDMETHODCALLTYPE sui_uia_ctrl_release(void* self) {
    LONG rc = InterlockedDecrement(&((SuiUiaProvider*)self)->ref_count);
    if (rc == 0) sui_free(self);
    return (ULONG)rc;
}

/* ── Pattern IUnknown (delegates to base provider via offsetof) ──────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_pat1_qi(void* self, REFIID riid, void** out) {
    return sui_uia_ctrl_qi(SUI_UIA_FROM_PAT1(self), riid, out);
}
static ULONG STDMETHODCALLTYPE sui_uia_pat1_addref(void* self) {
    return (ULONG)InterlockedIncrement(&SUI_UIA_FROM_PAT1(self)->ref_count);
}
static ULONG STDMETHODCALLTYPE sui_uia_pat1_release(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    LONG rc = InterlockedDecrement(&p->ref_count);
    if (rc == 0) sui_free(p);
    return (ULONG)rc;
}

static HRESULT STDMETHODCALLTYPE sui_uia_pat2_qi(void* self, REFIID riid, void** out) {
    return sui_uia_ctrl_qi(SUI_UIA_FROM_PAT2(self), riid, out);
}
static ULONG STDMETHODCALLTYPE sui_uia_pat2_addref(void* self) {
    return (ULONG)InterlockedIncrement(&SUI_UIA_FROM_PAT2(self)->ref_count);
}
static ULONG STDMETHODCALLTYPE sui_uia_pat2_release(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT2(self);
    LONG rc = InterlockedDecrement(&p->ref_count);
    if (rc == 0) sui_free(p);
    return (ULONG)rc;
}

/* ── Control provider: get_ProviderOptions ───────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_get_provider_options(void* self, int* out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = SUI_UIA_ProviderOptions_ServerSideProvider |
           SUI_UIA_ProviderOptions_UseComThreading;
    return S_OK;
}

/* ── Control type mapping ────────────────────────────────────────────────── */
static int sui_uia_map_control_type(SuiCtrlType type) {
    switch (type) {
    case SUI_CTRL_BUTTON:       return SUI_UIA_ButtonControlTypeId;
    case SUI_CTRL_CHECKBOX:     return SUI_UIA_CheckBoxControlTypeId;
    case SUI_CTRL_RADIO:        return SUI_UIA_RadioButtonControlTypeId;
    case SUI_CTRL_EDIT:         return SUI_UIA_EditControlTypeId;
    case SUI_CTRL_COMBOBOX:     return SUI_UIA_ComboBoxControlTypeId;
    case SUI_CTRL_SLIDER:       return SUI_UIA_SliderControlTypeId;
    case SUI_CTRL_SPINNER:      return SUI_UIA_SpinnerControlTypeId;
    case SUI_CTRL_LABEL:        return SUI_UIA_TextControlTypeId;
    case SUI_CTRL_TOOLTIP:      return SUI_UIA_TextControlTypeId;
    case SUI_CTRL_NAV_LIST:     return SUI_UIA_ListControlTypeId;
    case SUI_CTRL_LIST:         return SUI_UIA_ListControlTypeId;
    case SUI_CTRL_TABS:         return SUI_UIA_TabControlTypeId;
    case SUI_CTRL_COLOR_SWATCH: return SUI_UIA_ButtonControlTypeId;
    case SUI_CTRL_IMAGE:        return SUI_UIA_TextControlTypeId; /* decorative */
    case SUI_CTRL_SHORTCUT_RECORDER: return SUI_UIA_ButtonControlTypeId;
    case SUI_CTRL_TEXTVIEW:     return SUI_UIA_EditControlTypeId;
    default:                    return SUI_UIA_TextControlTypeId;
    }
}

/* ── Is focusable? ───────────────────────────────────────────────────────── */
static int sui_uia_is_focusable(SuiCtrlType type) {
    return type != SUI_CTRL_LABEL
        && type != SUI_CTRL_TOOLTIP
        && type != SUI_CTRL_IMAGE;
}

/* ── Control provider: GetPropertyValue ──────────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_get_property(void* self, int propertyId, VARIANT* out) {
    SuiUiaProvider* p = (SuiUiaProvider*)self;
    SuiControlHost* host;
    SuiControl* c;
    SuiSysAllocStringFn alloc_fn;
    HRESULT hr;

    if (!out) return E_POINTER;
    sui_uia_variant_init(out);

    hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;

    switch (propertyId) {
    case SUI_UIA_NamePropertyId: {
        const wchar_t* text = c->type == SUI_CTRL_TEXTVIEW && c->d.textview.buf ? c->d.textview.buf : c->text;
        alloc_fn = sui_uia_sysalloc();
        if (alloc_fn && text[0]) {
            out->vt = VT_BSTR;
            out->bstrVal = alloc_fn(text);
        }
        break;
    }

    case SUI_UIA_ControlTypePropertyId:
        out->vt = VT_I4;
        out->lVal = sui_uia_map_control_type(c->type);
        break;

    case SUI_UIA_BoundingRectanglePropertyId: {
        POINT pt = {0, 0};
        double bounds[4];
        SAFEARRAY* sa;
        SAFEARRAYBOUND sab;
        SuiSafeArrayCreateFn sa_create_fn = sui_uia_sa_create();
        SuiSafeArrayPutElementFn sa_put_fn = sui_uia_sa_put();
        MapWindowPoints(host->hwnd, NULL, &pt, 1);
        bounds[0] = (double)(c->x + pt.x);
        bounds[1] = (double)(c->y + pt.y);
        bounds[2] = (double)c->w;
        bounds[3] = (double)c->h;
        sab.lLbound = 0;
        sab.cElements = 4;
        sa = sa_create_fn ? sa_create_fn(VT_R8, 1, &sab) : NULL;
        if (sa && sa_put_fn) {
            int i;
            for (i = 0; i < 4; i++) {
                long idx = (long)i;
                sa_put_fn(sa, &idx, &bounds[i]);
            }
            out->vt = VT_R8 | VT_ARRAY;
            out->parray = sa;
        }
        break;
    }

    case SUI_UIA_IsKeyboardFocusablePropertyId:
        out->vt = VT_BOOL;
        out->boolVal = sui_uia_is_focusable(c->type) ? VARIANT_TRUE : VARIANT_FALSE;
        break;

    case SUI_UIA_HasKeyboardFocusPropertyId:
        out->vt = VT_BOOL;
        out->boolVal = (c->state & SUI_CTRL_FOCUSED) ? VARIANT_TRUE : VARIANT_FALSE;
        break;

    case SUI_UIA_AutomationIdPropertyId: {
        wchar_t buf[32];
        alloc_fn = sui_uia_sysalloc();
        if (alloc_fn) {
            wsprintfW(buf, L"ctrl_%d", c->id);
            out->vt = VT_BSTR;
            out->bstrVal = alloc_fn(buf);
        }
        break;
    }

    case SUI_UIA_IsControlElementPropertyId:
    case SUI_UIA_IsContentElementPropertyId:
        out->vt = VT_BOOL;
        out->boolVal = (c->state & SUI_CTRL_HIDDEN) ? VARIANT_FALSE : VARIANT_TRUE;
        break;

    case SUI_UIA_IsEnabledPropertyId:
        out->vt = VT_BOOL;
        out->boolVal = !(c->state & SUI_CTRL_DISABLED) && IsWindowEnabled(host->hwnd) ? VARIANT_TRUE : VARIANT_FALSE;
        break;
    case SUI_UIA_IsOffscreenPropertyId:
        out->vt = VT_BOOL;
        out->boolVal = (c->state & SUI_CTRL_HIDDEN) || !IsWindowVisible(host->hwnd) ? VARIANT_TRUE : VARIANT_FALSE;
        break;

    default:
        out->vt = VT_EMPTY;
        break;
    }
    return S_OK;
}

/* ══════════════════════════════════════════════════════════════════════════
   PATTERN IMPLEMENTATIONS (all use pattern1 slot unless noted)
   ══════════════════════════════════════════════════════════════════════════ */

/* ── IInvokeProvider (button, color swatch) -- pattern slot 1 ────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_invoke(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    return S_OK;
}

static const SuiUiaInvokeProviderVtbl sui_uia_invoke_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_invoke
};

/* ── IToggleProvider (checkbox) -- pattern slot 1 ────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_toggle(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    c->state ^= SUI_CTRL_CHECKED;
    sui_uia_notify_checked_changed(host, p->control_index, !(c->state & SUI_CTRL_CHECKED));
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    if (p->host == host) InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_toggle_get_state(void* self, int* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = (c->state & SUI_CTRL_CHECKED) ? 1 /* ToggleState_On */ : 0 /* ToggleState_Off */;
    return S_OK;
}

static const SuiUiaToggleProviderVtbl sui_uia_toggle_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_toggle, sui_uia_toggle_get_state
};

/* Radio buttons select one item in their group; selection cannot be cleared. */
static HRESULT STDMETHODCALLTYPE sui_uia_radio_select(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if ((c->state & SUI_CTRL_DISABLED) || !IsWindowEnabled(host->hwnd))
        return (HRESULT)0x80040200L; /* UIA_E_ELEMENTNOTENABLED */
    sui_ctrl_radio_select(host, p->control_index);
    if (host->on_click) host->on_click(c->id, host->on_click_data);
    if (p->host == host) InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_radio_remove(void* self) {
    HRESULT hr = sui_uia_resolve_control(SUI_UIA_FROM_PAT1(self), NULL, NULL);
    return FAILED(hr) ? hr : (HRESULT)0x80131509L;
}

static HRESULT STDMETHODCALLTYPE sui_uia_radio_selected(void* self, BOOL* out) {
    int checked;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_toggle_get_state(self, &checked);
    if (SUCCEEDED(hr)) *out = checked != 0;
    return hr;
}

static HRESULT STDMETHODCALLTYPE sui_uia_radio_container(void* self, void** out) {
    if (!out) return E_POINTER;
    *out = NULL; /* The host does not implement ISelectionProvider. */
    return sui_uia_resolve_control(SUI_UIA_FROM_PAT1(self), NULL, NULL);
}

static const SuiUiaSelectionItemProviderVtbl sui_uia_radio_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_radio_select, sui_uia_radio_select, sui_uia_radio_remove,
    sui_uia_radio_selected, sui_uia_radio_container
};

/* ── IValueProvider (edit, path display) -- pattern slot 1 ───────────────── */
/* Also used as pattern slot 2 for combobox (read-only value = selected text) */
static HRESULT STDMETHODCALLTYPE sui_uia_value_set_pat1(void* self, LPCWSTR val) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if ((SUI__HAS_COMBOBOX && c->type == SUI_CTRL_COMBOBOX))
        return 0x80131509L; /* UIA_E_NOTSUPPORTED */
    if (val) {
        int len;
        sui__wstrncpy_z(c->text, val, SUI_CTRL_TEXT_MAX);
        len = (int)wcslen(c->text);
        c->d.edit.cursor = len;
        c->d.edit.sel_start = -1;
        c->d.edit.sel_end = -1;
        c->d.edit.drag_sel_anchor = len;
    }
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_value_get_pat1(void* self, BSTR* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    SuiSysAllocStringFn fn;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    fn = sui_uia_sysalloc();
    *out = fn ? fn(c->text) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_value_get_readonly_pat1(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = ((SUI__HAS_COMBOBOX && c->type == SUI_CTRL_COMBOBOX)) ? TRUE : FALSE;
    return S_OK;
}

static const SuiUiaValueProviderVtbl sui_uia_value_vtbl_pat1 = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_value_set_pat1, sui_uia_value_get_pat1, sui_uia_value_get_readonly_pat1
};

/* Value provider in pattern slot 2 (for combobox) */
static HRESULT STDMETHODCALLTYPE sui_uia_value_set_pat2(void* self, LPCWSTR val) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT2(self);
    HRESULT hr = sui_uia_resolve_control(p, NULL, NULL);
    (void)val;
    if (FAILED(hr)) return hr;
    return 0x80131509L; /* UIA_E_NOTSUPPORTED -- combobox value is read-only */
}

static HRESULT STDMETHODCALLTYPE sui_uia_value_get_pat2(void* self, BSTR* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT2(self);
    SuiControl* c;
    SuiSysAllocStringFn fn;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    fn = sui_uia_sysalloc();
    *out = fn ? fn(c->text) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_value_get_readonly_pat2(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT2(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = TRUE;
    return S_OK;
}

static const SuiUiaValueProviderVtbl sui_uia_value_vtbl_pat2 = {
    sui_uia_pat2_qi, sui_uia_pat2_addref, sui_uia_pat2_release,
    sui_uia_value_set_pat2, sui_uia_value_get_pat2, sui_uia_value_get_readonly_pat2
};

/* ── IRangeValueProvider (slider, spinner) -- pattern slot 1 ─────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_range_set(void* self, double val) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER)) {
        c->d.slider.value = (float)val;
        if (c->d.slider.value < c->d.slider.min) c->d.slider.value = c->d.slider.min;
        if (c->d.slider.value > c->d.slider.max) c->d.slider.value = c->d.slider.max;
    } else if ((SUI__HAS_SPINNER && c->type == SUI_CTRL_SPINNER)) {
        c->d.spinner.value = (float)val;
        sui_ctrl_spinner_clamp(c);
    }
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_value(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER)) ? (double)c->d.slider.value : (double)c->d.spinner.value;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_readonly(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_max(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER)) ? (double)c->d.slider.max : (double)c->d.spinner.max;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_min(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER)) ? (double)c->d.slider.min : (double)c->d.spinner.min;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_large_change(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    double step;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    if ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER)) {
        step = (c->d.slider.step > 0.0f) ? (double)c->d.slider.step
            : (double)(c->d.slider.max - c->d.slider.min) / 10.0;
    } else {
        step = (double)c->d.spinner.step;
    }
    *out = step * 10.0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_range_get_small_change(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    if ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER))
        *out = (c->d.slider.step > 0.0f) ? (double)c->d.slider.step
            : (double)(c->d.slider.max - c->d.slider.min) / 10.0;
    else
        *out = (double)c->d.spinner.step;
    return S_OK;
}

static const SuiUiaRangeValueProviderVtbl sui_uia_range_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_range_set, sui_uia_range_get_value, sui_uia_range_get_readonly,
    sui_uia_range_get_max, sui_uia_range_get_min,
    sui_uia_range_get_large_change, sui_uia_range_get_small_change
};

/* ── IExpandCollapseProvider (combobox) -- pattern slot 1 ────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_expand(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if (!c->d.combo.open) sui_combo_popup_open(host, p->control_index);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_collapse(void* self) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    if (c->d.combo.open) sui_combo_popup_close(host);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_expand_get_state(void* self, int* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = c->d.combo.open ? 1 /* Expanded */ : 0 /* Collapsed */;
    return S_OK;
}

static const SuiUiaExpandCollapseProviderVtbl sui_uia_expand_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_expand, sui_uia_collapse, sui_uia_expand_get_state
};

/* ── ISelectionProvider (nav list) -- pattern slot 1 ─────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_selection_get(void* self, SAFEARRAY** out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiSafeArrayCreateFn fn;
    SAFEARRAYBOUND sab;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    /* Return empty array -- full SelectionItem support omitted for size */
    sab.lLbound = 0;
    sab.cElements = 0;
    fn = sui_uia_sa_create();
    *out = fn ? fn(VT_UNKNOWN, 1, &sab) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_selection_can_multi(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_selection_required(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = TRUE;
    return S_OK;
}

static const SuiUiaSelectionProviderVtbl sui_uia_selection_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_selection_get, sui_uia_selection_can_multi, sui_uia_selection_required
};

/* ── IScrollProvider -- pattern slot 1 ─────────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_scroll(void* self, int hAmt, int vAmt) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr = sui_uia_resolve_control(p, NULL, NULL);
    (void)hAmt; (void)vAmt;
    if (FAILED(hr)) return hr;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_set_percent(void* self, double hPct, double vPct) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControlHost* host;
    SuiControl* c;
    float visible_h, max_scroll;
    float* scroll_offset = NULL;
    float content_h = 0.0f;
    HRESULT hr = sui_uia_resolve_control(p, &host, &c);
    (void)hPct;
    if (FAILED(hr)) return hr;
    if (c->type == SUI_CTRL_TEXTVIEW) {
        scroll_offset = &c->d.textview.scroll_offset;
        content_h    = c->d.textview.content_height;
    } else {
        return S_OK;
    }
    visible_h = c->h - 16.0f;
    max_scroll = content_h - visible_h;
    if (max_scroll > 0 && vPct >= 0.0 && scroll_offset) {
        *scroll_offset = (float)(vPct / 100.0 * max_scroll);
        if (*scroll_offset < 0) *scroll_offset = 0;
        if (*scroll_offset > max_scroll) *scroll_offset = max_scroll;
    }
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_h_pct(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = -1.0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_v_pct(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    float visible_h, max_scroll, scroll_offset, content_h;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    if (c->type == SUI_CTRL_TEXTVIEW) {
        scroll_offset = c->d.textview.scroll_offset;
        content_h    = c->d.textview.content_height;
    } else {
        *out = -1.0; return S_OK;
    }
    visible_h = c->h - 16.0f;
    max_scroll = content_h - visible_h;
    if (max_scroll <= 0) { *out = -1.0; return S_OK; }
    *out = (double)scroll_offset / (double)max_scroll * 100.0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_h_view(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = 100.0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_v_view(void* self, double* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    float content_h = 0.0f;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    if (c->type == SUI_CTRL_TEXTVIEW) content_h = c->d.textview.content_height;
    if (content_h <= 0.0f) {
        *out = 100.0;
        return S_OK;
    }
    *out = (double)c->h / (double)content_h * 100.0;
    if (*out > 100.0) *out = 100.0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_h_scrollable(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    *out = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_scroll_get_v_scrollable(void* self, BOOL* out) {
    SuiUiaProvider* p = SUI_UIA_FROM_PAT1(self);
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;
    *out = (c->type == SUI_CTRL_TEXTVIEW && c->d.textview.content_height > c->h)
        ? TRUE : FALSE;
    return S_OK;
}

static const SuiUiaScrollProviderVtbl sui_uia_scroll_vtbl = {
    sui_uia_pat1_qi, sui_uia_pat1_addref, sui_uia_pat1_release,
    sui_uia_scroll, sui_uia_scroll_set_percent,
    sui_uia_scroll_get_h_pct, sui_uia_scroll_get_v_pct,
    sui_uia_scroll_get_h_view, sui_uia_scroll_get_v_view,
    sui_uia_scroll_get_h_scrollable, sui_uia_scroll_get_v_scrollable
};

/* ══════════════════════════════════════════════════════════════════════════
   CONTROL PROVIDER: GetPatternProvider
   ══════════════════════════════════════════════════════════════════════════ */

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_get_pattern(void* self, int patternId, IUnknown** out) {
    SuiUiaProvider* p = (SuiUiaProvider*)self;
    SuiControl* c;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;

    hr = sui_uia_resolve_control(p, NULL, &c);
    if (FAILED(hr)) return hr;

    switch (patternId) {
    case SUI_UIA_InvokePatternId:
        if (c->type == SUI_CTRL_BUTTON || (SUI__HAS_COLOR_SWATCH && c->type == SUI_CTRL_COLOR_SWATCH)) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_TogglePatternId:
        if (c->type == SUI_CTRL_CHECKBOX) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_SelectionItemPatternId:
        if (c->type == SUI_CTRL_RADIO) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_ValuePatternId:
        if (c->type == SUI_CTRL_EDIT) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        if ((SUI__HAS_COMBOBOX && c->type == SUI_CTRL_COMBOBOX)) {
            /* Combobox: ExpandCollapse is slot 1, Value is slot 2 */
            *out = (IUnknown*)&p->lpPattern2Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_RangeValuePatternId:
        if ((SUI__HAS_SLIDER && c->type == SUI_CTRL_SLIDER) || (SUI__HAS_SPINNER && c->type == SUI_CTRL_SPINNER)) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_ExpandCollapsePatternId:
        if ((SUI__HAS_COMBOBOX && c->type == SUI_CTRL_COMBOBOX)) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_SelectionPatternId:
        if ((SUI__HAS_NAV_LIST && c->type == SUI_CTRL_NAV_LIST) || (SUI__HAS_TABS && c->type == SUI_CTRL_TABS)) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;

    case SUI_UIA_ScrollPatternId:
        if (c->type == SUI_CTRL_TEXTVIEW) {
            *out = (IUnknown*)&p->lpPattern1Vtbl;
            InterlockedIncrement(&p->ref_count);
            return S_OK;
        }
        break;
    }
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_get_host(void* self, void** out) {
    SuiUiaProvider* p = (SuiUiaProvider*)self;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    return S_OK;
}

/* ── Control provider vtable ─────────────────────────────────────────────── */
static const SuiUiaProviderSimpleVtbl sui_uia_ctrl_vtbl = {
    sui_uia_ctrl_qi, sui_uia_ctrl_addref, sui_uia_ctrl_release,
    sui_uia_ctrl_get_provider_options,
    sui_uia_ctrl_get_pattern,
    sui_uia_ctrl_get_property,
    sui_uia_ctrl_get_host
};

/* ══════════════════════════════════════════════════════════════════════════
   PER-CONTROL PROVIDER (IRawElementProviderFragment)
   ══════════════════════════════════════════════════════════════════════════ */

/* Fragment IUnknown delegates back to the base Simple QI (master QI handles all IIDs) */
static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_qi(void* self, REFIID riid, void** out) {
    return sui_uia_ctrl_qi(SUI_CTRL_PROV_FROM_FRAG(self), riid, out);
}
static ULONG STDMETHODCALLTYPE sui_uia_ctrl_frag_addref(void* self) {
    return (ULONG)InterlockedIncrement(&SUI_CTRL_PROV_FROM_FRAG(self)->ref_count);
}
static ULONG STDMETHODCALLTYPE sui_uia_ctrl_frag_release(void* self) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    LONG rc = InterlockedDecrement(&p->ref_count);
    if (rc == 0) sui_free(p);
    return (ULONG)rc;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_navigate(void* self, int direction, void** out) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    SuiControlHost* host;
    SuiControl* ctrl;
    int idx = p->control_index;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_control(p, &host, &ctrl);
    if (FAILED(hr)) return hr;
    switch (direction) {
    case SUI_UIA_NavigateDirection_Parent:
        if (host->uia_host_provider) {
            SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)host->uia_host_provider;
            *out = &h->lpFragVtbl;
            InterlockedIncrement(&h->ref_count);
        }
        break;
    case SUI_UIA_NavigateDirection_PreviousSibling: {
        int pid = ctrl->parent_id;
        int i = idx - 1;
        while (i >= 0 && host->controls[i].parent_id != pid) i--;
        if (i >= 0) *out = sui_uia_get_ctrl_provider(host, i);
        break;
    }
    case SUI_UIA_NavigateDirection_NextSibling: {
        int pid = ctrl->parent_id;
        int i = idx + 1;
        while (i < host->control_count && host->controls[i].parent_id != pid) i++;
        if (i < host->control_count) *out = sui_uia_get_ctrl_provider(host, i);
        break;
    }
    default: /* FirstChild / LastChild: controls are leaves */ break;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_get_runtime_id(void* self, SAFEARRAY** out) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    SuiControl* ctrl;
    SuiSafeArrayCreateFn sa_create_fn;
    SuiSafeArrayPutElementFn sa_put_fn;
    SAFEARRAYBOUND sab;
    SAFEARRAY* sa;
    long i4;
    int id_pair[2];
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_control(p, NULL, &ctrl);
    if (FAILED(hr)) return hr;

    sa_create_fn = sui_uia_sa_create();
    sa_put_fn    = sui_uia_sa_put();
    if (!sa_create_fn || !sa_put_fn) return S_OK;

    sab.lLbound   = 0;
    sab.cElements = 2;
    sa = sa_create_fn(VT_I4, 1, &sab);
    if (!sa) return E_OUTOFMEMORY;

    id_pair[0] = SUI_UIA_AppendRuntimeId;
    id_pair[1] = ctrl->id;
    i4 = 0; sa_put_fn(sa, &i4, &id_pair[0]);
    i4 = 1; sa_put_fn(sa, &i4, &id_pair[1]);
    *out = sa;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_get_bounds(void* self, SuiUiaRect* out) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    SuiControlHost* host;
    SuiControl* c;
    POINT pt;
    HRESULT hr;
    if (!out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    hr = sui_uia_resolve_control(p, &host, &c);
    if (FAILED(hr)) return hr;
    pt.x = (LONG)c->x;
    pt.y = (LONG)c->y;
    MapWindowPoints(host->hwnd, NULL, &pt, 1);
    out->left   = (double)pt.x;
    out->top    = (double)pt.y;
    out->width  = (double)c->w;
    out->height = (double)c->h;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_get_embedded(void* self, SAFEARRAY** out) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_control(p, NULL, NULL);
    if (FAILED(hr)) return hr;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_set_focus(void* self) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    SuiControlHost* host;
    HRESULT hr = sui_uia_resolve_control(p, &host, NULL);
    if (FAILED(hr)) return hr;
    /* SetFocus gives Win32 focus to the host window, then sui_ctrl_set_focus
     * moves the slim-ui focused_control to this specific element. Both are
     * required: AT tools expect the element to be keyboard-reachable after
     * this call, and the UIA HasKeyboardFocus property-changed events fire
     * from sui_ctrl_set_focus → sui_uia_notify_focus_changed. */
    SetFocus(host->hwnd);
    sui_ctrl_set_focus(host, p->control_index);
    InvalidateRect(host->hwnd, NULL, FALSE);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_ctrl_frag_get_root(void* self, void** out) {
    SuiUiaProvider* p = SUI_CTRL_PROV_FROM_FRAG(self);
    SuiControlHost* host;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_control(p, &host, NULL);
    if (FAILED(hr)) return hr;
    if (host->uia_host_provider) {
        SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)host->uia_host_provider;
        *out = &h->lpFragRootVtbl;
        InterlockedIncrement(&h->ref_count);
    }
    return S_OK;
}

static const SuiUiaProviderFragmentVtbl sui_uia_ctrl_frag_vtbl = {
    sui_uia_ctrl_frag_qi, sui_uia_ctrl_frag_addref, sui_uia_ctrl_frag_release,
    sui_uia_ctrl_frag_navigate, sui_uia_ctrl_frag_get_runtime_id,
    sui_uia_ctrl_frag_get_bounds, sui_uia_ctrl_frag_get_embedded,
    sui_uia_ctrl_frag_set_focus, sui_uia_ctrl_frag_get_root
};

/* ══════════════════════════════════════════════════════════════════════════
   HOST PROVIDER (IRawElementProviderSimple + Fragment + FragmentRoot)
   ══════════════════════════════════════════════════════════════════════════ */

/* Recover host base from Fragment/FragmentRoot vtable pointers */
#define SUI_HOST_FROM_FRAG(self) \
    ((SuiUiaHostProviderFull*)((char*)(self) - offsetof(SuiUiaHostProviderFull, lpFragVtbl)))
#define SUI_HOST_FROM_FRAG_ROOT(self) \
    ((SuiUiaHostProviderFull*)((char*)(self) - offsetof(SuiUiaHostProviderFull, lpFragRootVtbl)))

/* ── Host IUnknown ───────────────────────────────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_host_qi(void* self, REFIID riid, void** out) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)self;
    if (!out) return E_POINTER;
    if (IsEqualGUID(riid, SUI_IID_REF(IID_IUnknown)) ||
        IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderSimple))) {
        *out = &h->lpVtbl;
        InterlockedIncrement(&h->ref_count);
        return S_OK;
    }
    if (IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderFragment))) {
        *out = &h->lpFragVtbl;
        InterlockedIncrement(&h->ref_count);
        return S_OK;
    }
    if (IsEqualGUID(riid, SUI_IID_REF(SUI_IID_IRawElementProviderFragmentRoot))) {
        *out = &h->lpFragRootVtbl;
        InterlockedIncrement(&h->ref_count);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE sui_uia_host_addref(void* self) {
    return (ULONG)InterlockedIncrement(&((SuiUiaHostProviderFull*)self)->ref_count);
}

static ULONG STDMETHODCALLTYPE sui_uia_host_release(void* self) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)self;
    LONG rc = InterlockedDecrement(&h->ref_count);
    if (rc == 0) sui_free(h);
    return (ULONG)rc;
}

/* Fragment/FragmentRoot IUnknown delegates back to host base */
static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_qi(void* self, REFIID riid, void** out) {
    return sui_uia_host_qi(SUI_HOST_FROM_FRAG(self), riid, out);
}
static ULONG STDMETHODCALLTYPE sui_uia_host_frag_addref(void* self) {
    return sui_uia_host_addref(SUI_HOST_FROM_FRAG(self));
}
static ULONG STDMETHODCALLTYPE sui_uia_host_frag_release(void* self) {
    return sui_uia_host_release(SUI_HOST_FROM_FRAG(self));
}
static HRESULT STDMETHODCALLTYPE sui_uia_host_fragroot_qi(void* self, REFIID riid, void** out) {
    return sui_uia_host_qi(SUI_HOST_FROM_FRAG_ROOT(self), riid, out);
}
static ULONG STDMETHODCALLTYPE sui_uia_host_fragroot_addref(void* self) {
    return sui_uia_host_addref(SUI_HOST_FROM_FRAG_ROOT(self));
}
static ULONG STDMETHODCALLTYPE sui_uia_host_fragroot_release(void* self) {
    return sui_uia_host_release(SUI_HOST_FROM_FRAG_ROOT(self));
}

/* ── Host: IRawElementProviderSimple methods ─────────────────────────────── */
static HRESULT STDMETHODCALLTYPE sui_uia_host_get_provider_options(void* self, int* out) {
    (void)self;
    if (!out) return E_POINTER;
    *out = SUI_UIA_ProviderOptions_ServerSideProvider |
           SUI_UIA_ProviderOptions_UseComThreading;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_get_pattern(void* self, int patternId, IUnknown** out) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)self;
    HRESULT hr;
    (void)patternId;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, NULL);
    if (FAILED(hr)) return hr;
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_get_property(void* self, int propertyId, VARIANT* out) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)self;
    HRESULT hr;
    if (!out) return E_POINTER;
    sui_uia_variant_init(out);
    hr = sui_uia_resolve_host(h, NULL);
    if (FAILED(hr)) return hr;
    switch (propertyId) {
    case SUI_UIA_IsControlElementPropertyId:
    case SUI_UIA_IsContentElementPropertyId:
        out->vt = VT_BOOL;
        out->boolVal = VARIANT_TRUE;
        break;
    default:
        out->vt = VT_EMPTY;
        break;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_get_host_provider(void* self, void** out) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)self;
    SuiControlHost* host;
    SuiUiaHostProviderFromHwndFn fn;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    fn = sui_uia_get_host_provider_fn();
    if (fn) return fn(host->hwnd, out);
    return S_OK;
}

static const SuiUiaProviderSimpleVtbl sui_uia_host_simple_vtbl = {
    sui_uia_host_qi, sui_uia_host_addref, sui_uia_host_release,
    sui_uia_host_get_provider_options, sui_uia_host_get_pattern,
    sui_uia_host_get_property, sui_uia_host_get_host_provider
};

/* ══════════════════════════════════════════════════════════════════════════
   HOST: IRawElementProviderFragment
   ══════════════════════════════════════════════════════════════════════════ */

/* Helper: get or create a cached per-control provider */
static void* sui_uia_get_ctrl_provider(SuiControlHost* host, int idx) {
    if (!host || idx < 0 || idx >= host->control_count) return NULL;
    if (!host->uia_providers[idx])
        host->uia_providers[idx] = sui_uia_create_control_provider(host, idx);
    if (host->uia_providers[idx]) {
        SuiUiaProvider* p = (SuiUiaProvider*)host->uia_providers[idx];
        InterlockedIncrement(&p->ref_count);
        /* Navigation, point lookup and focus return Fragment interfaces. */
        return &p->lpFragVtbl;
    }
    return NULL;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_navigate(void* self, int direction, void** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    SuiControlHost* host;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    switch (direction) {
    case SUI_UIA_NavigateDirection_FirstChild:
        if (host->control_count > 0)
            *out = sui_uia_get_ctrl_provider(host, 0);
        break;
    case SUI_UIA_NavigateDirection_LastChild:
        if (host->control_count > 0)
            *out = sui_uia_get_ctrl_provider(host, host->control_count - 1);
        break;
    default:
        break;
    }
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_get_runtime_id(void* self, SAFEARRAY** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, NULL);
    if (FAILED(hr)) return hr;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_get_bounds(void* self, SuiUiaRect* out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    SuiControlHost* host;
    RECT rc;
    HRESULT hr;
    if (!out) return E_POINTER;
    memset(out, 0, sizeof(*out));
    hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    GetWindowRect(host->hwnd, &rc);
    out->left   = (double)rc.left;
    out->top    = (double)rc.top;
    out->width  = (double)(rc.right - rc.left);
    out->height = (double)(rc.bottom - rc.top);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_get_embedded(void* self, SAFEARRAY** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, NULL);
    if (FAILED(hr)) return hr;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_set_focus(void* self) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    SuiControlHost* host;
    HRESULT hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    SetFocus(host->hwnd);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_frag_get_root(void* self, void** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG(self);
    HRESULT hr;
    if (!out) return E_POINTER;
    hr = sui_uia_resolve_host(h, NULL);
    if (FAILED(hr)) { *out = NULL; return hr; }
    *out = &h->lpFragRootVtbl;
    InterlockedIncrement(&h->ref_count);
    return S_OK;
}

static const SuiUiaProviderFragmentVtbl sui_uia_host_frag_vtbl = {
    sui_uia_host_frag_qi, sui_uia_host_frag_addref, sui_uia_host_frag_release,
    sui_uia_host_frag_navigate, sui_uia_host_frag_get_runtime_id,
    sui_uia_host_frag_get_bounds, sui_uia_host_frag_get_embedded,
    sui_uia_host_frag_set_focus, sui_uia_host_frag_get_root
};

/* ══════════════════════════════════════════════════════════════════════════
   HOST: IRawElementProviderFragmentRoot
   ══════════════════════════════════════════════════════════════════════════ */

static HRESULT STDMETHODCALLTYPE sui_uia_host_element_from_point(void* self, double x, double y, void** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG_ROOT(self);
    SuiControlHost* host;
    POINT pt;
    int hit;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    pt.x = (LONG)x;
    pt.y = (LONG)y;
    ScreenToClient(host->hwnd, &pt);
    hit = sui_ctrl_hit_test(host, (float)pt.x, (float)pt.y);
    if (hit >= 0)
        *out = sui_uia_get_ctrl_provider(host, hit);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE sui_uia_host_get_focus(void* self, void** out) {
    SuiUiaHostProviderFull* h = SUI_HOST_FROM_FRAG_ROOT(self);
    SuiControlHost* host;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    hr = sui_uia_resolve_host(h, &host);
    if (FAILED(hr)) return hr;
    if (host->focused_control >= 0 && host->focused_control < host->control_count)
        *out = sui_uia_get_ctrl_provider(host, host->focused_control);
    return S_OK;
}

static const SuiUiaProviderFragmentRootVtbl sui_uia_host_fragroot_vtbl = {
    sui_uia_host_fragroot_qi, sui_uia_host_fragroot_addref, sui_uia_host_fragroot_release,
    sui_uia_host_element_from_point, sui_uia_host_get_focus
};

/* ══════════════════════════════════════════════════════════════════════════
   UIA NOTIFICATION HELPERS (called via forward declarations from sui_controls.h)
   ══════════════════════════════════════════════════════════════════════════ */

/* Disconnect + release per-control UIA providers in [from_idx, to_idx).
 * Used by sui_ctrl_clear_page and sui_ctrl_host_release. */
static void sui_uia_disconnect_ctrl_providers(SuiControlHost* host, int from_idx, int to_idx) {
    SuiUiaDisconnectProviderFn disc_fn = sui_uia_get_disconnect_fn();
    int i;
    if (!host) return;
    if (from_idx < 0) from_idx = 0;
    if (to_idx > SUI_MAX_CONTROLS) to_idx = SUI_MAX_CONTROLS;
    for (i = from_idx; i < to_idx; i++) {
        if (host->uia_providers[i]) {
            typedef ULONG (STDMETHODCALLTYPE *RelFn)(void*);
            SuiUiaProvider* provider = (SuiUiaProvider*)host->uia_providers[i];
            void** vtbl = *(void***)provider;
            if (disc_fn) disc_fn(provider);
            provider->host = NULL;
            provider->control_index = -1;
            host->uia_providers[i] = NULL;
            ((RelFn)vtbl[2])(provider);
        }
    }
}

/* Host providers are also independently ref-counted and can outlive the
 * SuiControlHost. Disconnect and sever their raw host pointer before Release. */
static void sui_uia_disconnect_host_provider(SuiControlHost* host) {
    SuiUiaDisconnectProviderFn disc_fn;
    SuiUiaHostProviderFull* provider;
    typedef ULONG (STDMETHODCALLTYPE *RelFn)(void*);
    void** vtbl;
    if (!host || !host->uia_host_provider) return;
    provider = (SuiUiaHostProviderFull*)host->uia_host_provider;
    vtbl = *(void***)provider;
    disc_fn = sui_uia_get_disconnect_fn();
    if (disc_fn) disc_fn(provider);
    provider->host = NULL;
    host->uia_host_provider = NULL;
    ((RelFn)vtbl[2])(provider);
}

/* Fire HasKeyboardFocus property-changed events for old and new focus indices. */
static void sui_uia_raise_event(void* provider, int event_id) {
    typedef HRESULT (WINAPI *RaiseEventFn)(void*, int);
    RaiseEventFn fn = (RaiseEventFn)GetProcAddress(GetModuleHandleW(L"uiautomationcore.dll"), "UiaRaiseAutomationEvent");
    if (fn) fn(provider, event_id);
}

static void sui_uia_notify_checked_changed(SuiControlHost* host, int idx, int old_checked) {
    SuiControl* c = &host->controls[idx];
    int checked = (c->state & SUI_CTRL_CHECKED) != 0;
    SuiUiaRaisePropChangedFn fn;
    VARIANT before, after;
    if (old_checked == checked || !host->uia_providers[idx]) return;
    fn = sui_uia_get_raise_prop_fn();
    if (!fn) return;
    sui_uia_variant_init(&before);
    sui_uia_variant_init(&after);
    if (c->type == SUI_CTRL_RADIO) {
        before.vt = after.vt = VT_BOOL;
        before.boolVal = old_checked ? VARIANT_TRUE : VARIANT_FALSE;
        after.boolVal = checked ? VARIANT_TRUE : VARIANT_FALSE;
        fn(host->uia_providers[idx], 30079 /* SelectionItemIsSelected */, before, after);
        if (checked) sui_uia_raise_event(host->uia_providers[idx], 20012 /* ElementSelected */);
    } else {
        before.vt = after.vt = VT_I4;
        before.lVal = old_checked; after.lVal = checked;
        fn(host->uia_providers[idx], 30086 /* ToggleToggleState */, before, after);
    }
}

static void sui_uia_notify_focus_changed(SuiControlHost* host, int old_idx, int new_idx) {
    SuiUiaRaisePropChangedFn fn;
    VARIANT vfalse, vtrue;
    if (!host->uia_host_provider) return;
    fn = sui_uia_get_raise_prop_fn();
    if (!fn) return;
    sui_uia_variant_init(&vfalse); vfalse.vt = VT_BOOL; vfalse.boolVal = VARIANT_FALSE;
    sui_uia_variant_init(&vtrue);  vtrue.vt  = VT_BOOL; vtrue.boolVal  = VARIANT_TRUE;
    if (old_idx >= 0 && old_idx < SUI_MAX_CONTROLS && host->uia_providers[old_idx])
        fn(host->uia_providers[old_idx], SUI_UIA_HasKeyboardFocusPropertyId, vtrue, vfalse);
    if (new_idx >= 0 && new_idx < host->control_count) {
        if (!host->uia_providers[new_idx])
            host->uia_providers[new_idx] = sui_uia_create_control_provider(host, new_idx);
        if (!host->uia_providers[new_idx]) return;
        fn(host->uia_providers[new_idx], SUI_UIA_HasKeyboardFocusPropertyId, vfalse, vtrue);
        sui_uia_raise_event(host->uia_providers[new_idx], 20005 /* AutomationFocusChanged */);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   PROVIDER CREATION
   ══════════════════════════════════════════════════════════════════════════ */

/* Set pattern vtable pointers based on control type */
static void sui_uia_set_pattern_vtbls(SuiUiaProvider* p, SuiCtrlType type) {
    p->lpPattern1Vtbl = NULL;
    p->lpPattern2Vtbl = NULL;
    switch (type) {
    case SUI_CTRL_BUTTON:
    case SUI_CTRL_COLOR_SWATCH:
        p->lpPattern1Vtbl = &sui_uia_invoke_vtbl;
        break;
    case SUI_CTRL_CHECKBOX:
        p->lpPattern1Vtbl = &sui_uia_toggle_vtbl;
        break;
    case SUI_CTRL_RADIO:
        p->lpPattern1Vtbl = &sui_uia_radio_vtbl;
        break;
    case SUI_CTRL_EDIT:
        p->lpPattern1Vtbl = &sui_uia_value_vtbl_pat1;
        break;
#if SUI__HAS_COMBOBOX
    case SUI_CTRL_COMBOBOX:
        p->lpPattern1Vtbl = &sui_uia_expand_vtbl;
        p->lpPattern2Vtbl = &sui_uia_value_vtbl_pat2;
        break;
#endif
#if SUI__HAS_SLIDER || SUI__HAS_SPINNER
    case SUI_CTRL_SLIDER:
    case SUI_CTRL_SPINNER:
        p->lpPattern1Vtbl = &sui_uia_range_vtbl;
        break;
#endif
#if SUI__HAS_NAV_LIST || SUI__HAS_TABS
    case SUI_CTRL_NAV_LIST:
    case SUI_CTRL_TABS:
        p->lpPattern1Vtbl = &sui_uia_selection_vtbl;
        break;
#endif
    case SUI_CTRL_TEXTVIEW:
        p->lpPattern1Vtbl = &sui_uia_scroll_vtbl;
        break;
    default:
        break;
    }
}

static void* sui_uia_create_control_provider(SuiControlHost* host, int ctrl_index) {
    SuiUiaProvider* p = (SuiUiaProvider*)sui_malloc(sizeof(SuiUiaProvider));
    if (!p) return NULL;
    memset(p, 0, sizeof(*p));
    p->lpVtbl    = &sui_uia_ctrl_vtbl;
    p->lpFragVtbl = &sui_uia_ctrl_frag_vtbl;
    p->ref_count = 1;
    p->host = host;
    p->control_index = ctrl_index;
    sui_uia_set_pattern_vtbls(p, host->controls[ctrl_index].type);
    return p;
}

static void* sui_uia_create_host_provider(SuiControlHost* host) {
    SuiUiaHostProviderFull* h = (SuiUiaHostProviderFull*)sui_malloc(sizeof(SuiUiaHostProviderFull));
    if (!h) return NULL;
    memset(h, 0, sizeof(*h));
    h->lpVtbl = &sui_uia_host_simple_vtbl;
    h->lpFragVtbl = &sui_uia_host_frag_vtbl;
    h->lpFragRootVtbl = &sui_uia_host_fragroot_vtbl;
    h->ref_count = 1;
    h->host = host;
    return h;
}

/* ══════════════════════════════════════════════════════════════════════════
   WM_GETOBJECT HANDLER
   ══════════════════════════════════════════════════════════════════════════ */

static LRESULT sui_uia_handle_getobject(SuiControlHost* host, WPARAM wParam, LPARAM lParam) {
    SuiUiaReturnRawElementProviderFn fn;
    if ((LONG)lParam != SUI_UIA_RootObjectId) return 0;
    fn = sui_uia_get_return_fn();
    if (!fn) return 0;
    if (!host->uia_host_provider)
        host->uia_host_provider = sui_uia_create_host_provider(host);
    if (!host->uia_host_provider) return 0;
    return fn(host->hwnd, wParam, lParam, host->uia_host_provider);
}
