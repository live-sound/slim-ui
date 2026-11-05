#include "../src/slim_ui_net.cpp"
#include "../../../tests/utest.h"

struct ShaderBuild {
    int active = 0;
    int paused = 0;
    float speed = 1.0f;
};

static void SUI_NET_CALL BuildShaderTest(SuiNetWindow* window, void* user) {
    ShaderBuild* state = (ShaderBuild*)user;
    sui_net_clear_page(window);
    sui_net_image_at(window, 94, 20, 20, 300, 140, 0, 0);
    sui_net_edit_at(window, 131, 20, 180, 300, 28, L"keep this text", L"", 0);
    sui_sample_shader_configure(window, 94, 131, state->active, state->paused, state->speed);
}

UTEST(bridge, initial_size_and_shader_lifecycle) {
    ShaderBuild state;
    SuiNetWindow* window = sui_net_window_create(L"Slim UI bridge regression", 960, 720,
        BuildShaderTest, NULL, &state);
    ASSERT_NE((SuiNetWindow*)NULL, window);

    RECT bounds = {};
    GetWindowRect(window->hwnd, &bounds);
    EXPECT_EQ(MulDiv(960, (int)sui_get_window_dpi(NULL), 96), bounds.right - bounds.left);
    EXPECT_EQ(MulDiv(720, (int)sui_get_window_dpi(NULL), 96), bounds.bottom - bounds.top);
    EXPECT_FALSE(window->shader.running);

    state.active = 1;
    sui_net_window_rebuild(window);
    EXPECT_TRUE(window->shader.running);
    EXPECT_TRUE(sui_ctrl_find(&window->host, 94)->paint != NULL);
    EXPECT_TRUE(sui_ctrl_find(&window->host, 131)->paint != NULL);

    SampleShaderFrame(window, 100.0f);
    EXPECT_NEAR(0.1f, window->shader.seconds, 0.0001f);
    state.paused = 1;
    sui_net_window_rebuild(window);
    EXPECT_FALSE(window->shader.running);
    EXPECT_NEAR(0.1f, window->shader.seconds, 0.0001f);
    EXPECT_EQ(0, wcscmp(L"keep this text", sui_ctrl_find(&window->host, 131)->text));

    state.paused = 0;
    state.speed = 2.0f;
    sui_net_window_rebuild(window);
    SampleShaderFrame(window, 100.0f);
    EXPECT_NEAR(0.3f, window->shader.seconds, 0.0001f);

    state.active = 0;
    sui_net_window_rebuild(window);
    EXPECT_FALSE(window->shader.running);
    EXPECT_TRUE(sui_ctrl_find(&window->host, 131)->paint == NULL);

    state.active = 1;
    sui_net_window_rebuild(window);
    SuiNetRelease(window);
    EXPECT_FALSE(window->shader.running);
    EXPECT_TRUE(window->shader.effect == NULL);
    sui_net_window_destroy(window);
}

UTEST_MAIN();
