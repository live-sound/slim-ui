/* Private support for the gallery; not part of the managed wrapper API. */
#include "../../../../demo/shader_effect.h"

struct SampleShaderState {
    DemoShader* effect = nullptr;
    float seconds = 0.0f;
    float speed = 1.0f;
    bool active = false;
    bool paused = false;
    bool running = false;
};
