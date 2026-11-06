/* The toolkit with every optional control left out, as a small installer builds it: what stays still draws and takes input. */
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_NO_COMBOBOX
#define SUI_NO_TABS
#define SUI_NO_LIST
#define SUI_NO_NAV_LIST
#define SUI_NO_SPINNER
#define SUI_NO_SLIDER
#define SUI_NO_COLOR_SWATCH
#define SUI_NO_IMAGE
#define SUI_NO_SHORTCUT_RECORDER
#define SUI_NO_TOOLTIP
#define SUI_NO_SCROLLVIEW
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>
#include <UIAutomationCore.h>
#include "utest.h"

UTEST_STATE();

int main(int argc, const char* const argv[]) { return utest_main(argc, argv); }

static void count_click(int id, void* data) {
    (void)id;
    ++*(int*)data;
}

UTEST(omit, patterns_query_and_call_through_windows_sdk_interfaces) {
    static SuiControlHost host;
    SuiRenderer renderer;
    SuiUiaProvider* p;
    IUnknown *pattern = NULL, *identity = NULL;
    IValueProvider* value = NULL;
    IToggleProvider* toggle = NULL;
    ISelectionItemProvider* radio = NULL;
    BSTR text = NULL;
    enum ToggleState state;
    BOOL selected = FALSE;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ASSERT_TRUE(hwnd != NULL);
    sui_renderer_init_fields(&renderer);
    sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, 96);
    sui_ctrl_add_edit(&host, 1, 0, 0, 100, 24, L"before", NULL, 0);
    sui_ctrl_add_checkbox(&host, 2, 0, 30, 100, 24, L"Accept", 0);
    sui_ctrl_add_radio(&host, 3, 0, 60, 100, 24, L"First", 1, 1);
    sui_ctrl_add_radio(&host, 4, 0, 90, 100, 24, L"Second", 1, 0);

    p = (SuiUiaProvider*)sui_uia_create_control_provider(&host, 0);
    ASSERT_EQ(S_OK, p->lpVtbl->GetPatternProvider(p, SUI_UIA_ValuePatternId, &pattern));
    ASSERT_EQ(S_OK, pattern->lpVtbl->QueryInterface(pattern, &IID_IValueProvider, (void**)&value));
    ASSERT_EQ(S_OK, value->lpVtbl->SetValue(value, L"after"));
    ASSERT_EQ(S_OK, value->lpVtbl->get_Value(value, &text));
    ASSERT_EQ(0, lstrcmpW(text, L"after"));
    SysFreeString(text);
    ASSERT_EQ(S_OK, value->lpVtbl->QueryInterface(value, &IID_IUnknown, (void**)&identity));
    ASSERT_EQ((void*)p, (void*)identity);
    identity->lpVtbl->Release(identity);
    value->lpVtbl->Release(value);
    pattern->lpVtbl->Release(pattern);
    p->lpVtbl->Release(p);

    p = (SuiUiaProvider*)sui_uia_create_control_provider(&host, 1);
    ASSERT_EQ(S_OK, p->lpVtbl->GetPatternProvider(p, SUI_UIA_TogglePatternId, &pattern));
    ASSERT_EQ(S_OK, pattern->lpVtbl->QueryInterface(pattern, &IID_IToggleProvider, (void**)&toggle));
    ASSERT_EQ(S_OK, toggle->lpVtbl->Toggle(toggle));
    ASSERT_EQ(S_OK, toggle->lpVtbl->get_ToggleState(toggle, &state));
    ASSERT_EQ(ToggleState_On, state);
    toggle->lpVtbl->Release(toggle);
    pattern->lpVtbl->Release(pattern);
    p->lpVtbl->Release(p);

    p = (SuiUiaProvider*)sui_uia_create_control_provider(&host, 3);
    ASSERT_EQ(S_OK, p->lpVtbl->GetPatternProvider(p, SUI_UIA_SelectionItemPatternId, &pattern));
    ASSERT_EQ(S_OK, pattern->lpVtbl->QueryInterface(pattern, &IID_ISelectionItemProvider, (void**)&radio));
    ASSERT_EQ(S_OK, radio->lpVtbl->Select(radio));
    ASSERT_EQ(S_OK, radio->lpVtbl->get_IsSelected(radio, &selected));
    ASSERT_TRUE(selected);
    ASSERT_FALSE(host.controls[2].state & SUI_CTRL_CHECKED);
    radio->lpVtbl->Release(radio);
    pattern->lpVtbl->Release(pattern);
    p->lpVtbl->Release(p);
    sui_ctrl_host_release(&host);
    DestroyWindow(hwnd);
}

UTEST(omit, the_controls_that_stay_draw_and_take_input) {
    static SuiControlHost host;
    SuiRenderer renderer;
    int clicks = 0;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 200, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ASSERT_TRUE(hwnd != NULL);
    sui_renderer_init_fields(&renderer);
    ASSERT_TRUE(sui_renderer_init(&renderer, hwnd));
    sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, 96);
    host.on_click = count_click;
    host.on_click_data = &clicks;
    ASSERT_EQ(0, sui_ctrl_add_button(&host, 1, 10, 10, 100, 30, L"Next", 1));
    ASSERT_EQ(1, sui_ctrl_add_checkbox(&host, 2, 10, 50, 100, 24, L"Option", 0));
    ASSERT_EQ(2, sui_ctrl_add_edit(&host, 3, 10, 80, 200, 30, L"C:\\Program Files", NULL, 0));
    ASSERT_EQ(3, sui_ctrl_add_textview(&host, 4, 10, 120, 200, 40, L"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight", 64, 1));

    sui_ctrl_on_mouse_move(&host, 20, 20);
    sui_ctrl_on_lbutton_down(&host, 20, 20);
    sui_ctrl_on_lbutton_up(&host, 20, 20);
    ASSERT_EQ(1, clicks);
    sui_ctrl_on_keydown(&host, VK_TAB);

    sui_renderer_begin(&renderer);
    sui_renderer_clear(&renderer, sui_scheme_light.background);
    sui_ctrl_draw_all(&host);
    sui_renderer_end(&renderer);

    sui_ctrl_on_mouse_wheel(&host, 40, 130, -120); /* a text view scrolls without the scroll view control */
    ASSERT_GT(host.controls[3].d.textview.scroll_offset, 0.0f);

    sui_ctrl_host_release(&host);
    sui_renderer_release(&renderer);
    DestroyWindow(hwnd);
}

UTEST(omit, retained_accessibility_properties_patterns_and_actions) {
    static SuiControlHost host;
    SuiRenderer renderer;
    SuiUiaProvider* providers[7] = {0};
    const SuiCtrlType types[] = { SUI_CTRL_BUTTON, SUI_CTRL_CHECKBOX, SUI_CTRL_EDIT, SUI_CTRL_RADIO, SUI_CTRL_LABEL, SUI_CTRL_PROGRESS };
    const wchar_t* names[] = { L"Next", L"Launch app", L"C:\\Target", L"Reinstall", L"Destination", L"Installing" };
    const int expected_types[] = { SUI_UIA_ButtonControlTypeId, SUI_UIA_CheckBoxControlTypeId, SUI_UIA_EditControlTypeId, SUI_UIA_RadioButtonControlTypeId, SUI_UIA_TextControlTypeId, SUI_UIA_TextControlTypeId };
    int clicks = 0, i, state;
    IUnknown* pattern = NULL;
    VARIANT value;
    BSTR text = NULL;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ASSERT_TRUE(hwnd != NULL);
    sui_renderer_init_fields(&renderer);
    sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, 96);
    host.on_click = count_click;
    host.on_click_data = &clicks;
    for (i=0; i<6; ++i) {
        ASSERT_EQ(i, sui_ctrl_add(&host, types[i], i+1, 0, (float)(i*30), 200, 24, names[i]));
        providers[i] = (SuiUiaProvider*)sui_uia_create_control_provider(&host, i);
        ASSERT_TRUE(providers[i] != NULL);
        ASSERT_EQ(S_OK, providers[i]->lpVtbl->GetPropertyValue(providers[i], SUI_UIA_NamePropertyId, &value));
        ASSERT_EQ(VT_BSTR, value.vt);
        ASSERT_EQ(0, lstrcmpW(names[i], value.bstrVal));
        VariantClear(&value);
        ASSERT_EQ(S_OK, providers[i]->lpVtbl->GetPropertyValue(providers[i], SUI_UIA_ControlTypePropertyId, &value));
        ASSERT_EQ(expected_types[i], value.lVal);
        ASSERT_EQ(S_OK, providers[i]->lpVtbl->GetPropertyValue(providers[i], SUI_UIA_IsEnabledPropertyId, &value));
        ASSERT_EQ(VARIANT_TRUE, value.boolVal);
        host.controls[i].state |= SUI_CTRL_DISABLED;
        ASSERT_EQ(S_OK, providers[i]->lpVtbl->GetPropertyValue(providers[i], SUI_UIA_IsEnabledPropertyId, &value));
        ASSERT_EQ(VARIANT_FALSE, value.boolVal);
        host.controls[i].state &= ~SUI_CTRL_DISABLED;
    }
    ASSERT_EQ(S_OK, providers[0]->lpVtbl->GetPatternProvider(providers[0], SUI_UIA_InvokePatternId, &pattern));
    ASSERT_TRUE(pattern != NULL);
    ASSERT_EQ(S_OK, ((const SuiUiaInvokeProviderVtbl*)providers[0]->lpPattern1Vtbl)->Invoke(pattern));
    pattern->lpVtbl->Release(pattern);
    ASSERT_EQ(1, clicks);
    ASSERT_EQ(S_OK, providers[1]->lpVtbl->GetPatternProvider(providers[1], SUI_UIA_TogglePatternId, &pattern));
    ASSERT_EQ(S_OK, ((const SuiUiaToggleProviderVtbl*)providers[1]->lpPattern1Vtbl)->Toggle(pattern));
    ASSERT_EQ(S_OK, ((const SuiUiaToggleProviderVtbl*)providers[1]->lpPattern1Vtbl)->get_ToggleState(pattern, &state));
    ASSERT_EQ(1, state);
    pattern->lpVtbl->Release(pattern);
    ASSERT_EQ(2, clicks);
    ASSERT_EQ(S_OK, providers[2]->lpVtbl->GetPatternProvider(providers[2], SUI_UIA_ValuePatternId, &pattern));
    ASSERT_EQ(S_OK, ((const SuiUiaValueProviderVtbl*)providers[2]->lpPattern1Vtbl)->SetValue(pattern, L"C:\\Changed"));
    ASSERT_EQ(S_OK, ((const SuiUiaValueProviderVtbl*)providers[2]->lpPattern1Vtbl)->get_Value(pattern, &text));
    ASSERT_EQ(0, lstrcmpW(L"C:\\Changed", text));
    SysFreeString(text);
    pattern->lpVtbl->Release(pattern);
    ASSERT_EQ(S_OK, providers[3]->lpFragVtbl->SetFocus(&providers[3]->lpFragVtbl));
    ASSERT_EQ(3, host.focused_control);
    ASSERT_EQ(6, sui_ctrl_add_textview(&host, 7, 0, 180, 200, 40, L"License terms", 64, 1));
    providers[6] = (SuiUiaProvider*)sui_uia_create_control_provider(&host, 6);
    ASSERT_EQ(S_OK, providers[6]->lpVtbl->GetPropertyValue(providers[6], SUI_UIA_NamePropertyId, &value));
    ASSERT_EQ(0, lstrcmpW(L"License terms", value.bstrVal));
    VariantClear(&value);
    ASSERT_EQ(S_OK, providers[6]->lpVtbl->GetPatternProvider(providers[6], SUI_UIA_ScrollPatternId, &pattern));
    ASSERT_TRUE(pattern != NULL);
    pattern->lpVtbl->Release(pattern);
    for(i=0; i<7; ++i) if(providers[i]) providers[i]->lpVtbl->Release(providers[i]);
    sui_ctrl_host_release(&host);
    DestroyWindow(hwnd);
}

UTEST(omit, edit_keeps_intrinsic_size_without_slider_or_spinner) {
    static SuiControlHost host;
    SuiRenderer renderer;
    float width = 0, height = 0;
    sui_renderer_init_fields(&renderer);
    sui_ctrl_host_init(&host, NULL, &renderer, &sui_scheme_light, 96);
    sui_ctrl_add_edit(&host, 1, 0, 0, 0, 0, L"Path", NULL, 0);
    sui_ctrl_intrinsic_size(&host, &host.controls[0], &width, &height, 0);
    ASSERT_GE(width, 80.0f);
    ASSERT_GT(height, 0.0f);
    sui_ctrl_host_release(&host);
}

UTEST(omit, disabled_controls_do_not_expose_accessibility_patterns) {
    static SuiControlHost host;
    SuiRenderer renderer;
    const SuiCtrlType types[] = { SUI_CTRL_SLIDER, SUI_CTRL_SPINNER, SUI_CTRL_COMBOBOX, SUI_CTRL_NAV_LIST, SUI_CTRL_TABS };
    const int patterns[] = { SUI_UIA_RangeValuePatternId, SUI_UIA_RangeValuePatternId, SUI_UIA_ExpandCollapsePatternId, SUI_UIA_SelectionPatternId, SUI_UIA_SelectionPatternId };
    int i;
    HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ASSERT_TRUE(hwnd != NULL);
    sui_renderer_init_fields(&renderer);
    sui_ctrl_host_init(&host, hwnd, &renderer, &sui_scheme_light, 96);
    for (i = 0; i < 5; ++i) {
        SuiUiaProvider* provider;
        IUnknown* pattern = NULL;
        /* The generic factory bypasses the omitted per-control constructors. */
        sui_ctrl_add(&host, types[i], i + 1, 0, 0, 100, 24, L"");
        provider = (SuiUiaProvider*)sui_uia_create_control_provider(&host, i);
        ASSERT_TRUE(provider != NULL);
        ASSERT_EQ((void*)NULL, provider->lpPattern1Vtbl);
        ASSERT_EQ(E_NOINTERFACE, provider->lpVtbl->GetPatternProvider(provider, patterns[i], &pattern));
        ASSERT_EQ((void*)NULL, pattern);
        provider->lpVtbl->Release(provider);
    }
    sui_ctrl_host_release(&host);
    DestroyWindow(hwnd);
}
