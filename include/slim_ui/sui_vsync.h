/* One interruptible frame source per UI thread, shared across translation units.
 * init prepares resources; attach/detach reuse them and never join a worker.
 * stop releases a host; the last host releases the clock. Owner-thread only.
 * Callbacks may attach, detach or stop. A stop inside the state's own dispatch
 * ends that dispatch at once: the state may be freed before the callback returns.
 */
#ifndef SUI_VSYNC_H
#define SUI_VSYNC_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <stdint.h>
#include <string.h>
#pragma comment(lib, "dwmapi.lib")
#ifndef WM_SUI_VSYNC
#define WM_SUI_VSYNC (WM_APP + 100)
#endif
#define SUI_VSYNC_MAX_TICKERS 8
#define SUI_VSYNC_MAX_HOSTS 64
#define SUI_CLOCK_FRAME (WM_APP + 1)
typedef void (*SuiVsyncTickFn)(void*,float);
typedef struct SuiVsyncTicker { SuiVsyncTickFn fn; void* user; uint64_t generation; } SuiVsyncTicker;
typedef struct SuiVsyncClock SuiVsyncClock;
typedef struct SuiVsyncState {
    HWND hwnd;
    HMONITOR monitor; /* last prepared display; moves trigger one reconfiguration */
    HMONITOR target_monitor; /* optional for off-screen/embedded message hosts */
    HANDLE thread; /* borrowed shared handle; NULL while unsubscribed */
    SuiVsyncClock* clock;
    SuiVsyncTicker tickers[SUI_VSYNC_MAX_TICKERS];
    int ticker_count,pending,dispatching;
    uint64_t generation;
    double last_seconds,frame_seconds;
    int* dispatch_live; /* while dispatching: cleared by a stop from a callback (appended: layout-compatible) */
    HMONITOR period_monitor; /* the display `period` was read for (appended) */
    LONG period; /* that display's refresh period in microseconds; 0 until read or after a display change */
} SuiVsyncState;
struct SuiVsyncClock {
    HWND hwnd;
    DWORD owner;
    HANDLE thread,control,timer;
    volatile LONG running,active,pending,period_us;
    LARGE_INTEGER frequency;
    SuiVsyncState* hosts[SUI_VSYNC_MAX_HOSTS]; /* owner thread only */
    int count;
    LONG display_reads; /* displays' modes read (EnumDisplaySettingsW allocates): once per host and display */
};
static double sui__vsync_now_seconds(void) {
    LARGE_INTEGER f,c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
    return (double)c.QuadPart/(double)f.QuadPart;
}
static uint64_t sui__vsync_now_ms(void) { return (uint64_t)(sui__vsync_now_seconds()*1000.0); }
static float sui_vsync_animation_frame_ms(void) {
    DWM_TIMING_INFO t; memset(&t,0,sizeof(t)); t.cbSize=sizeof(t);
    if(SUCCEEDED(DwmGetCompositionTimingInfo(NULL,&t)) && t.rateRefresh.uiNumerator && t.rateRefresh.uiDenominator)
        return 1000.0f*(float)t.rateRefresh.uiDenominator/(float)t.rateRefresh.uiNumerator;
    return 1000.0f/60.0f;
}
static DWORD sui__vsync_fallback_sleep_ms(void) {
    DWORD ms=(DWORD)(sui_vsync_animation_frame_ms()+0.5f); return ms ? ms : 1;
}
/* For existing dedicated rendering threads only; never under a compositor lock. */
static void sui_vsync_wait_frame(void) {
    if(FAILED(DwmFlush())) Sleep(sui__vsync_fallback_sleep_ms());
}
static DWORD WINAPI sui__vsync_thread_proc(void* context) {
    SuiVsyncClock* c=(SuiVsyncClock*)context;
    HANDLE waits[2]; double deadline=0;
    waits[0]=c->control; waits[1]=c->timer;
    while(InterlockedCompareExchange(&c->running,0,0)) {
        LARGE_INTEGER due; double now,period,seconds; DWORD result;
        if(!InterlockedCompareExchange(&c->active,0,0)) {
            CancelWaitableTimer(c->timer); WaitForSingleObject(c->control,INFINITE); deadline=0; continue;
        }
        now=sui__vsync_now_seconds();
        period=(double)InterlockedCompareExchange(&c->period_us,0,0)/1000000.0;
        if(!deadline || deadline<=now) {
            DWM_TIMING_INFO t; memset(&t,0,sizeof(t)); t.cbSize=sizeof(t);
            deadline=now+period;
            if(SUCCEEDED(DwmGetCompositionTimingInfo(NULL,&t)) && t.qpcVBlank) {
                double base=(double)t.qpcVBlank/(double)c->frequency.QuadPart;
                if(base<=now && now-base<1.0)
                    deadline=base+((double)(uint64_t)((now-base)/period)+1)*period;
            }
        }
        seconds=deadline-now; if(seconds<0.0001) seconds=0.0001;
        due.QuadPart=-(LONGLONG)(seconds*10000000.0);
        if(!SetWaitableTimer(c->timer,&due,0,NULL,NULL,FALSE)) {
            result=WaitForSingleObject(c->control,(DWORD)(period*1000.0+0.5));
            if(result==WAIT_OBJECT_0) { deadline=0; continue; }
        } else {
            result=WaitForMultipleObjects(2,waits,FALSE,INFINITE);
            if(result==WAIT_OBJECT_0) { deadline=0; continue; }
            if(result!=WAIT_OBJECT_0+1) break;
        }
        if(!InterlockedCompareExchange(&c->running,0,0)) break;
        if(InterlockedCompareExchange(&c->active,0,0) && !InterlockedExchange(&c->pending,1))
            if(!PostMessageW(c->hwnd,SUI_CLOCK_FRAME,0,0)) InterlockedExchange(&c->pending,0);
        deadline+=period;
    }
    return 0;
}
/* The clock's period from its ticking hosts' displays. A host's display period is read once per display (the
 * mode query allocates, and hosts start and stop ticking at every wake from idle) and again after a display or
 * settings change. */
static void sui__vsync_clock_refresh(SuiVsyncClock* c) {
    int i,active=0; LONG period_us=1000000;
    if(!c) return;
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) {
        SuiVsyncState* s=c->hosts[i];
        if(s && s->ticker_count) {
            ++active;
            s->monitor=s->target_monitor ? s->target_monitor : MonitorFromWindow(s->hwnd,MONITOR_DEFAULTTONEAREST);
            if(!s->period || s->period_monitor!=s->monitor) {
                MONITORINFOEXW monitor; DEVMODEW mode; LONG period;
                float ms=sui_vsync_animation_frame_ms();
                memset(&monitor,0,sizeof(monitor)); monitor.cbSize=sizeof(monitor);
                memset(&mode,0,sizeof(mode)); mode.dmSize=sizeof(mode);
                if(GetMonitorInfoW(s->monitor,(MONITORINFO*)&monitor)) {
                    ++c->display_reads;
                    if(EnumDisplaySettingsW(monitor.szDevice,ENUM_CURRENT_SETTINGS,&mode) &&
                        mode.dmDisplayFrequency>=24 && mode.dmDisplayFrequency<=1000)
                        ms=1000.0f/(float)mode.dmDisplayFrequency;
                }
                period=(LONG)(ms*1000.0f+0.5f); if(period<1000) period=1000;
                s->period=period; s->period_monitor=s->monitor;
            }
            if(s->period<period_us) period_us=s->period;
        }
    }
    InterlockedExchange(&c->period_us,period_us);
    InterlockedExchange(&c->active,active); SetEvent(c->control);
}
/* A display or its settings changed: every host's period is read again when it next ticks. */
static void sui__vsync_clock_displays_changed(SuiVsyncClock* c) {
    int i;
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) if(c->hosts[i]) c->hosts[i]->period=0;
    sui__vsync_clock_refresh(c);
}
static LRESULT CALLBACK sui__vsync_clock_wndproc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    SuiVsyncClock* c=(SuiVsyncClock*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_NCCREATE) {
        c=(SuiVsyncClock*)((CREATESTRUCTW*)l)->lpCreateParams;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)c);
        return DefWindowProcW(hwnd,msg,w,l);
    }
    if(c && msg==SUI_CLOCK_FRAME) {
        int i,moved=0; double now=sui__vsync_now_seconds(); InterlockedExchange(&c->pending,0);
        for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) {
            SuiVsyncState* s=c->hosts[i];
            if(s && s->ticker_count) {
                HMONITOR monitor=s->target_monitor ? s->target_monitor : MonitorFromWindow(s->hwnd,MONITOR_DEFAULTTONEAREST);
                if(s->monitor!=monitor) moved=1;
                s->frame_seconds=now;
                if(!s->pending) {
                    s->pending=1;
                    if(!PostMessageW(s->hwnd,WM_SUI_VSYNC,0,0)) s->pending=0;
                }
            }
        }
        if(moved) sui__vsync_clock_refresh(c);
        return 0;
    }
    if(c && (msg==WM_DISPLAYCHANGE || msg==WM_SETTINGCHANGE)) sui__vsync_clock_displays_changed(c);
    /* Creation failure is cleaned up by clock_get: hwnd is assigned only
     * after CreateWindowEx succeeds. Do not free its context twice. */
    if(c && msg==WM_DESTROY && c->hwnd) {
        /* Worker uses interruptible kernel waits; no DwmFlush/SendMessage/UI
         * callbacks or locks that depend on this owner thread. */
        InterlockedExchange(&c->running,0); SetEvent(c->control);
        if(c->thread) { WaitForSingleObject(c->thread,INFINITE); CloseHandle(c->thread); }
        CloseHandle(c->timer); CloseHandle(c->control);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); HeapFree(GetProcessHeap(),0,c); return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
static SuiVsyncClock* sui__vsync_clock_get(void) {
    const wchar_t* class_name=L"SlimUI.SharedFrameClock.v1";
    wchar_t name[17]; DWORD id=GetCurrentThreadId(); HWND hwnd; unsigned i;
    SuiVsyncClock* c; WNDCLASSEXW wc;
    for(i=0;i<8;++i) name[i]=L"0123456789ABCDEF"[(id>>(i*4))&15];
    id=GetCurrentProcessId();
    for(i=0;i<8;++i) name[i+8]=L"0123456789ABCDEF"[(id>>(i*4))&15]; name[16]=0;
    hwnd=FindWindowExW(NULL,NULL,class_name,name);
    if(hwnd) return (SuiVsyncClock*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    memset(&wc,0,sizeof(wc)); wc.cbSize=sizeof(wc); wc.lpfnWndProc=sui__vsync_clock_wndproc;
    wc.hInstance=GetModuleHandleW(NULL); wc.lpszClassName=class_name;
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return NULL;
    c=(SuiVsyncClock*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*c));
    if(!c) { UnregisterClassW(class_name,wc.hInstance); return NULL; }
    c->owner=GetCurrentThreadId(); c->period_us=16667; QueryPerformanceFrequency(&c->frequency);
    c->control=CreateEventW(NULL,FALSE,FALSE,NULL);
    /* High-resolution timer on Win10 1803+, runtime fallback for older systems. */
    c->timer=CreateWaitableTimerExW(NULL,NULL,0x00000002,TIMER_ALL_ACCESS);
    if(!c->timer) c->timer=CreateWaitableTimerW(NULL,FALSE,NULL);
    if(!c->control || !c->timer) {
        if(c->control) CloseHandle(c->control); if(c->timer) CloseHandle(c->timer);
        HeapFree(GetProcessHeap(),0,c); UnregisterClassW(class_name,wc.hInstance); return NULL;
    }
    /* Hidden top-level host receives display-change broadcasts; message-only
     * windows do not. It is never shown or activated. */
    c->hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,class_name,name,WS_POPUP,
        0,0,0,0,NULL,NULL,wc.hInstance,c);
    if(!c->hwnd) { CloseHandle(c->control); CloseHandle(c->timer); HeapFree(GetProcessHeap(),0,c); UnregisterClassW(class_name,wc.hInstance); return NULL; }
    c->running=1; c->thread=CreateThread(NULL,0,sui__vsync_thread_proc,c,0,NULL);
    if(!c->thread) {
        DestroyWindow(c->hwnd); UnregisterClassW(class_name,wc.hInstance); return NULL;
    } return c;
}
static int sui__vsync_prepare(SuiVsyncState* s) {
    int i; SuiVsyncClock* c;
    if(s->clock) return 1;
    if(!s->hwnd || GetWindowThreadProcessId(s->hwnd,NULL)!=GetCurrentThreadId()) return 0;
    c=sui__vsync_clock_get(); if(!c) return 0;
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) if(!c->hosts[i]) {
        c->hosts[i]=s; ++c->count; s->clock=c; return 1;
    }
    return 0;
}
static void sui_vsync_init(SuiVsyncState* s,HWND hwnd) {
    if(!s) return; memset(s,0,sizeof(*s)); s->hwnd=hwnd; sui__vsync_prepare(s);
}
/* A hidden message host may present on a different display. NULL restores HWND
 * tracking. Call when the presentation target changes, on the owner thread. */
static void sui_vsync_set_monitor(SuiVsyncState* s,HMONITOR monitor) {
    if(!s || !s->hwnd || GetWindowThreadProcessId(s->hwnd,NULL)!=GetCurrentThreadId() ||
        s->target_monitor==monitor) return;
    s->target_monitor=monitor;
    if(s->clock && s->ticker_count) sui__vsync_clock_refresh(s->clock);
}
static int sui_vsync_attach(SuiVsyncState* s,SuiVsyncTickFn fn,void* user) {
    int i;
    if(!s || !fn || !sui__vsync_prepare(s) || s->clock->owner!=GetCurrentThreadId()) return 0;
    for(i=0;i<SUI_VSYNC_MAX_TICKERS;++i) if(s->tickers[i].fn==fn && s->tickers[i].user==user) return 1;
    if(s->generation==UINT64_MAX) return 0;
    for(i=0;i<SUI_VSYNC_MAX_TICKERS;++i) if(!s->tickers[i].fn) {
        if(!s->ticker_count) s->last_seconds=sui__vsync_now_seconds();
        s->tickers[i].fn=fn; s->tickers[i].user=user; s->tickers[i].generation=++s->generation;
        ++s->ticker_count; s->thread=s->clock->thread; sui__vsync_clock_refresh(s->clock); return 1;
    }
    return 0;
}
static void sui_vsync_detach(SuiVsyncState* s,SuiVsyncTickFn fn,void* user) {
    int i,removed=0;
    if(!s || !s->clock || s->clock->owner!=GetCurrentThreadId()) return;
    for(i=0;i<SUI_VSYNC_MAX_TICKERS;++i) if(s->tickers[i].fn==fn && s->tickers[i].user==user) {
        s->tickers[i].fn=NULL; s->tickers[i].user=NULL; --s->ticker_count; removed=1; break;
    }
    if(!removed) return;
    /* Keep an already queued notification coalesced across detach/reattach. */
    if(!s->ticker_count) s->thread=NULL;
    sui__vsync_clock_refresh(s->clock);
}
static void sui_vsync_stop(SuiVsyncState* s) {
    int i; SuiVsyncClock* c;
    if(!s || !(c=s->clock) || c->owner!=GetCurrentThreadId()) return;
    if(s->dispatching) { /* from a tick callback: the dispatch loop stops touching s */
        if(s->dispatch_live) *s->dispatch_live=0;
        s->dispatching=0; s->dispatch_live=NULL;
    }
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) if(c->hosts[i]==s) { c->hosts[i]=NULL; --c->count; break; }
    memset(s->tickers,0,sizeof(s->tickers)); s->ticker_count=0; s->clock=NULL; s->thread=NULL; s->pending=0;
    if(!c->count) {
        DestroyWindow(c->hwnd);
        /* Fails harmlessly if another UI thread still owns a clock. Avoid
         * leaving a stale WndProc after the final toolkit module is unloaded. */
        UnregisterClassW(L"SlimUI.SharedFrameClock.v1",GetModuleHandleW(NULL));
    } else sui__vsync_clock_refresh(c);
}
static void sui_vsync_dispatch(SuiVsyncState* s) {
    int i,live=1; double now; float dt; uint64_t generation;
    if(!s || !s->clock || s->clock->owner!=GetCurrentThreadId() || s->dispatching) return;
    s->pending=0; now=s->frame_seconds;
    if(now<=s->last_seconds || !s->ticker_count) return;
    dt=(float)((now-s->last_seconds)*1000.0); s->last_seconds=now;
    generation=s->generation; s->dispatching=1; s->dispatch_live=&live;
    /* Do not truncate elapsed time: stalls settle finite animations. */
    for(i=0;i<SUI_VSYNC_MAX_TICKERS;++i) {
        SuiVsyncTicker t=s->tickers[i];
        if(t.fn && t.generation<=generation) {
            t.fn(t.user,dt);
            if(!live) return; /* stopped (and possibly freed) by the callback */
        }
    }
    s->dispatching=0; s->dispatch_live=NULL;
}
#endif
