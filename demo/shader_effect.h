/* Pixel shaders for the demo's Shader page. A control draws itself into a bitmap between begin and end, and the shader runs on the way to
 * the screen.
 *
 * This has its own file because the real Direct2D headers can't share one with slim_ui.h, which declares the same interfaces for C. */
#ifndef SUI_DEMO_SHADER_EFFECT_H
#define SUI_DEMO_SHADER_EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

enum { DEMO_SHADER_DUSK, DEMO_SHADER_GLOW, DEMO_SHADER_KINDS };

typedef struct DemoShader DemoShader;

DemoShader* demo_shader_create(void);

/* Call while drawing, with SuiRenderer.render_target. Returns a target to draw the control on, or NULL if shaders can't run here. The bitmap
 * covers the control and `margin` pixels around it, for a shader that reaches past its edges. */
void* demo_shader_begin(DemoShader* shader, void* render_target, int kind, float x, float y, float w, float h, float margin);

/* Runs the shader over what was drawn and puts the result on the screen. */
void demo_shader_end(DemoShader* shader, int kind, float seconds);

void demo_shader_release(DemoShader* shader);

#ifdef __cplusplus
}
#endif

#endif
