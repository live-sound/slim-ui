#define SUI_FEATURE_VSYNC
#define SUI_FEATURE_CUSTOM_CONTROLS
#define SUI_IMPLEMENTATION
#include <slim_ui/slim_ui.h>
#include "utest.h"
UTEST_STATE();
int main(int argc,const char* const argv[]) { return utest_main(argc,argv); }
static HWND popup_test_window(void) {
    return CreateWindowExW(0,L"STATIC",L"SlimUI test",0,0,0,1,1,
        HWND_MESSAGE,NULL,GetModuleHandleW(NULL),NULL);
}
UTEST(popup_clock, scrollbar_uses_elapsed_shared_time_then_sleeps) {
    SuiControlHost host={0}; SuiComboPopup popup={0};
    host.hwnd=popup_test_window(); popup.hwnd=popup_test_window();
    host.combo_popup=&popup; popup.owner_host=&host;
    sui_vsync_init(&host.vsync,host.hwnd);
    sui_combo_popup_reveal_scrollbar(&popup);
    ASSERT_EQ(1,host.vsync.ticker_count);
    host.vsync.frame_seconds=(popup.scrollbar_start_ms+500)/1000;
    sui_vsync_dispatch(&host.vsync);
    ASSERT_NEAR(.5,popup.sb_reveal,.001);
    host.vsync.frame_seconds=(popup.scrollbar_start_ms+1500)/1000;
    sui_vsync_dispatch(&host.vsync);
    ASSERT_EQ(0.f,popup.sb_reveal); ASSERT_EQ(0,host.vsync.ticker_count);
    ASSERT_EQ(0,host.vsync.clock->active);
    sui_vsync_stop(&host.vsync); DestroyWindow(popup.hwnd); DestroyWindow(host.hwnd);
}
UTEST(popup_clock, closing_during_dispatch_detaches_before_window_destruction) {
    SuiControlHost host={0}; SuiComboPopup popup={0};
    host.hwnd=popup_test_window(); popup.hwnd=popup_test_window();
    host.combo_popup=&popup; popup.owner_host=&host;
    sui_vsync_init(&host.vsync,host.hwnd);
    popup.anim_phase=SUI_COMBO_PHASE_CLOSE; popup.anim_t=.5f;
    popup.anim_last_ms=sui_combo_popup_now_ms(&popup);
    ASSERT_TRUE(sui_combo_popup_start_clock(&popup));
    host.vsync.frame_seconds=(popup.anim_last_ms+200)/1000;
    sui_vsync_dispatch(&host.vsync);
    ASSERT_EQ((HWND)NULL,popup.hwnd);
    ASSERT_EQ(0,host.vsync.ticker_count); ASSERT_EQ(0,host.vsync.dispatching);
    ASSERT_NE((SuiVsyncClock*)NULL,host.vsync.clock); /* prepared idle source reused */
    sui_vsync_stop(&host.vsync); DestroyWindow(host.hwnd);
}
