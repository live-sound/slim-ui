#include <slim_ui/sui_vsync.h>
#include "utest.h"
UTEST_STATE();
void test_vsync_other_init(SuiVsyncState*,HWND);
void test_vsync_other_stop(SuiVsyncState*);
int main(int argc,const char* const argv[]) { return utest_main(argc,argv); }
static void tick_counter(void* user,float delta) { (void)delta; ++*(int*)user; }
static HWND test_window(void) {
    return CreateWindowExW(0,L"STATIC",L"SlimUI test",0,0,0,1,1,HWND_MESSAGE,NULL,GetModuleHandleW(NULL),NULL);
}
UTEST(vsync, shares_clock_across_windows_and_translation_units) {
    HWND a=test_window(),b=test_window(); SuiVsyncState first,second; int ticks=0;
    ASSERT_NE((HWND)NULL,a); ASSERT_NE((HWND)NULL,b);
    sui_vsync_init(&first,a); test_vsync_other_init(&second,b);
    ASSERT_NE((SuiVsyncClock*)NULL,first.clock); ASSERT_EQ(first.clock,second.clock);
    ASSERT_EQ(2,first.clock->count); ASSERT_EQ(0,first.clock->active);
    ASSERT_TRUE(sui_vsync_attach(&first,tick_counter,&ticks));
    ASSERT_TRUE(sui_vsync_attach(&second,tick_counter,&ticks));
    ASSERT_EQ(first.thread,second.thread); ASSERT_EQ(2,first.clock->active);
    sui_vsync_detach(&first,tick_counter,&ticks); sui_vsync_detach(&second,tick_counter,&ticks);
    ASSERT_EQ(0,first.clock->active); ASSERT_EQ((HANDLE)NULL,first.thread);
    sui_vsync_stop(&first); ASSERT_EQ(1,second.clock->count);
    test_vsync_other_stop(&second); ASSERT_EQ((SuiVsyncClock*)NULL,second.clock);
    DestroyWindow(a); DestroyWindow(b);
}
typedef struct Mutation { SuiVsyncState* state; int a,b,c; } Mutation;
static void tick_new(void* p,float dt) { Mutation* m=p; (void)dt; ++m->c; }
static void tick_remove(void* p,float dt) { Mutation* m=p; (void)dt; ++m->b; }
static void tick_mutate(void* p,float dt) {
    Mutation* m=p; (void)dt; ++m->a;
    sui_vsync_detach(m->state,tick_mutate,p);
    sui_vsync_detach(m->state,tick_remove,p);
    sui_vsync_attach(m->state,tick_new,p);
    sui_vsync_dispatch(m->state);
}
UTEST(vsync, callback_mutation_and_coalesced_delivery) {
    HWND hwnd=test_window(); SuiVsyncState state; Mutation m; MSG msg; int queued=0;
    memset(&m,0,sizeof(m)); m.state=&state; sui_vsync_init(&state,hwnd);
    ASSERT_TRUE(sui_vsync_attach(&state,tick_mutate,&m));
    ASSERT_TRUE(sui_vsync_attach(&state,tick_remove,&m));
    /* A stalled owner receives one notification, even after many source frames. */
    sui__vsync_clock_wndproc(state.clock->hwnd,SUI_CLOCK_FRAME,0,0);
    sui_vsync_detach(&state,tick_mutate,&m);
    sui_vsync_detach(&state,tick_remove,&m);
    sui_vsync_attach(&state,tick_mutate,&m);
    sui_vsync_attach(&state,tick_remove,&m);
    sui__vsync_clock_wndproc(state.clock->hwnd,SUI_CLOCK_FRAME,0,0);
    while(PeekMessageW(&msg,hwnd,WM_SUI_VSYNC,WM_SUI_VSYNC,PM_REMOVE)) ++queued;
    ASSERT_EQ(1,queued);
    state.frame_seconds=state.last_seconds+0.016;
    sui_vsync_dispatch(&state);
    ASSERT_EQ(1,m.a); ASSERT_EQ(0,m.b); ASSERT_EQ(0,m.c);
    state.frame_seconds=state.last_seconds+0.016;
    sui_vsync_dispatch(&state); ASSERT_EQ(1,m.c);
    sui_vsync_stop(&state); DestroyWindow(hwnd);
}
typedef struct StopInTick { SuiVsyncState* state; int ticks,after; } StopInTick;
static void tick_stop_and_free(void* p,float dt) {
    StopInTick* m=p; (void)dt; ++m->ticks;
    sui_vsync_stop(m->state);                     /* the host is destroyed from its own frame */
    memset(m->state,0xCD,sizeof(*m->state)); HeapFree(GetProcessHeap(),0,m->state); m->state=NULL;
}
static void tick_after(void* p,float dt) { StopInTick* m=p; (void)dt; ++m->after; }
UTEST(vsync, stop_inside_dispatch_releases_the_host_and_ends_the_dispatch) {
    HWND a=test_window(),b=test_window(); SuiVsyncState other; SuiVsyncState* s; StopInTick m; SuiVsyncClock* clock;
    int i,ticks=0;
    memset(&m,0,sizeof(m));
    s=m.state=(SuiVsyncState*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(SuiVsyncState));
    ASSERT_NE((SuiVsyncState*)NULL,s);
    sui_vsync_init(s,a); sui_vsync_init(&other,b); clock=other.clock;
    ASSERT_NE((SuiVsyncClock*)NULL,clock); ASSERT_EQ(clock,s->clock); ASSERT_EQ(2,clock->count);
    ASSERT_TRUE(sui_vsync_attach(s,tick_stop_and_free,&m)); ASSERT_TRUE(sui_vsync_attach(s,tick_after,&m));
    ASSERT_TRUE(sui_vsync_attach(&other,tick_counter,&ticks));
    s->frame_seconds=s->last_seconds+0.016;
    sui_vsync_dispatch(s);                        /* returns without touching the freed state */
    ASSERT_EQ(1,m.ticks); ASSERT_EQ(0,m.after); ASSERT_EQ((SuiVsyncState*)NULL,m.state);
    ASSERT_EQ(1,clock->count);                    /* no dangling registration */
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) ASSERT_TRUE(clock->hosts[i]==NULL || clock->hosts[i]==&other);
    sui__vsync_clock_wndproc(clock->hwnd,SUI_CLOCK_FRAME,0,0); /* frames reach only live hosts */
    other.frame_seconds=other.last_seconds+0.016; sui_vsync_dispatch(&other); ASSERT_EQ(1,ticks);
    sui_vsync_stop(&other); ASSERT_EQ((SuiVsyncClock*)NULL,other.clock);
    DestroyWindow(a); DestroyWindow(b);
}
UTEST(vsync, repeated_shutdown_releases_handles) {
    HWND hwnd=test_window(); SuiVsyncState state; DWORD before,after; int i,ticks=0;
    sui_vsync_init(&state,hwnd); sui_vsync_attach(&state,tick_counter,&ticks); sui_vsync_stop(&state);
    ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(i=0;i<24;++i) {
        sui_vsync_init(&state,hwnd);
        ASSERT_TRUE(sui_vsync_attach(&state,tick_counter,&ticks));
        sui_vsync_stop(&state);
    }
    ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(),&after));
    ASSERT_LE(after,before+1); DestroyWindow(hwnd);
}
UTEST(vsync, bounded_hosts_and_display_reconfiguration) {
    HWND hwnd=test_window(); SuiVsyncState states[SUI_VSYNC_MAX_HOSTS+1]; int i,ticks=0;
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) {
        sui_vsync_init(&states[i],hwnd); ASSERT_NE((SuiVsyncClock*)NULL,states[i].clock);
    }
    sui_vsync_init(&states[SUI_VSYNC_MAX_HOSTS],hwnd);
    ASSERT_FALSE(sui_vsync_attach(&states[SUI_VSYNC_MAX_HOSTS],tick_counter,&ticks));
    ASSERT_TRUE(sui_vsync_attach(&states[0],tick_counter,&ticks));
    states[0].monitor=NULL;
    SendMessageW(states[0].clock->hwnd,SUI_CLOCK_FRAME,0,0);
    ASSERT_EQ(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),states[0].monitor);
    SendMessageW(states[0].clock->hwnd,WM_DISPLAYCHANGE,0,0);
    ASSERT_GT(states[0].clock->period_us,0);
    {
        POINT point={100000,100000};
        HMONITOR target=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);
        sui_vsync_set_monitor(&states[0],target);
        ASSERT_EQ(target,states[0].target_monitor); ASSERT_EQ(target,states[0].monitor);
        sui_vsync_set_monitor(&states[0],NULL);
        ASSERT_EQ((HMONITOR)NULL,states[0].target_monitor);
        ASSERT_EQ(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),states[0].monitor);
    }
    for(i=0;i<SUI_VSYNC_MAX_HOSTS;++i) sui_vsync_stop(&states[i]);
    DestroyWindow(hwnd);
}
/* A host's display period is read once per display: starting and stopping to tick (every wake from idle) reads nothing more, a display
 * change and a move to another display read it again, once. The mode query allocates in the system's library. */
UTEST(vsync, a_hosts_display_period_is_read_once_per_display) {
    HWND hwnd=test_window(); SuiVsyncState state; int i,ticks=0; LONG reads;
    sui_vsync_init(&state,hwnd); ASSERT_NE((SuiVsyncClock*)NULL,state.clock);
    ASSERT_TRUE(sui_vsync_attach(&state,tick_counter,&ticks));
    reads=state.clock->display_reads; ASSERT_EQ(1,reads);
    ASSERT_GT(state.period,0); ASSERT_EQ(state.monitor,state.period_monitor); ASSERT_EQ(state.period,state.clock->period_us);
    for(i=0;i<16;++i) {
        sui_vsync_detach(&state,tick_counter,&ticks); ASSERT_EQ(0,state.clock->active);
        ASSERT_TRUE(sui_vsync_attach(&state,tick_counter,&ticks)); ASSERT_EQ(1,state.clock->active);
    }
    ASSERT_EQ(reads,state.clock->display_reads);
    SendMessageW(state.clock->hwnd,WM_DISPLAYCHANGE,0,0);
    ASSERT_EQ(reads+1,state.clock->display_reads);
    sui_vsync_detach(&state,tick_counter,&ticks); ASSERT_TRUE(sui_vsync_attach(&state,tick_counter,&ticks));
    ASSERT_EQ(reads+1,state.clock->display_reads);
    {
        POINT point={100000,100000};
        HMONITOR target=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);
        const int moved=target!=state.monitor;
        sui_vsync_set_monitor(&state,target);
        ASSERT_EQ(target,state.period_monitor);
        ASSERT_EQ(reads+1+moved,state.clock->display_reads);
    }
    sui_vsync_stop(&state); DestroyWindow(hwnd);
}
typedef struct Cadence { double intervals[120],last; int count; } Cadence;
static void tick_cadence(void* user,float delta) {
    Cadence* sample=user; double now=sui__vsync_now_seconds(); (void)delta;
    if(sample->last && sample->count<120) sample->intervals[sample->count++]=(now-sample->last)*1000;
    sample->last=now;
}
UTEST(vsync, real_source_cadence_and_idle) {
    HWND hwnd=test_window(); SuiVsyncState state; Cadence sample={0}; MSG msg;
    double deadline=sui__vsync_now_seconds()+10; int i,j;
    sui_vsync_init(&state,hwnd); ASSERT_TRUE(sui_vsync_attach(&state,tick_cadence,&sample));
    while(sample.count<120 && sui__vsync_now_seconds()<deadline) {
        MsgWaitForMultipleObjectsEx(0,NULL,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) {
            if(msg.hwnd==hwnd && msg.message==WM_SUI_VSYNC) sui_vsync_dispatch(&state);
            else DispatchMessageW(&msg);
        }
    }
    ASSERT_EQ(120,sample.count);
    for(i=1;i<120;++i) { double v=sample.intervals[i]; j=i;
        while(j>0 && sample.intervals[j-1]>v) { sample.intervals[j]=sample.intervals[j-1]; --j; }
        sample.intervals[j]=v;
    }
    printf("  UI frame delivery: period=%.3f ms p50=%.3f ms p95=%.3f ms (not presentation latency)\n",
        state.clock->period_us/1000.0,sample.intervals[60],sample.intervals[114]);
    sui_vsync_detach(&state,tick_cadence,&sample);
    ASSERT_EQ(0,state.clock->active);
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) DispatchMessageW(&msg);
    MsgWaitForMultipleObjectsEx(0,NULL,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) {
        if(msg.hwnd==hwnd && msg.message==WM_SUI_VSYNC) sui_vsync_dispatch(&state);
        else DispatchMessageW(&msg);
    }
    ASSERT_EQ(120,sample.count); sui_vsync_stop(&state); DestroyWindow(hwnd);
}
