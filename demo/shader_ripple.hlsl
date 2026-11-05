// The image control's shader on the demo's Shader page: slow waves bend the picture, and a bar of light sweeps across it.
//
// shader_ripple_ps.h is this file compiled. After changing it, run this from a Visual Studio developer prompt:
//   fxc /nologo /T ps_4_0_level_9_3 /E main /Vn kRippleBytes /Fh shader_ripple_ps.h shader_ripple.hlsl

Texture2D Source : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer Constants : register(b0) {
    float seconds;
    float width;
    float height;
    float margin;
};

float4 main(float4 position : SV_POSITION, float4 scene : SCENE_POSITION, float4 source : TEXCOORD0) : SV_Target {
    float2 pixel = scene.xy;
    float2 uv = pixel / float2(width, height);

    // How far to look away from this pixel, in pixels. shader_effect.cpp asks for that much room around the image.
    float2 bend = float2(sin(pixel.y * 0.045 + seconds * 2.0) + 0.5 * sin(pixel.y * 0.110 - seconds * 1.3),
                         cos(pixel.x * 0.035 + seconds * 1.6) + 0.5 * cos(pixel.x * 0.090 + seconds * 1.1)) * 2.5;
    float4 color = Source.Sample(SourceSampler, source.xy + bend * source.zw);

    // Colors arrive premultiplied, so light is scaled by alpha to stay inside the picture.
    float sweep = frac(seconds * 0.22) * 2.6 - 0.8;
    float distance = uv.x + uv.y * 0.35 - sweep;
    color.rgb += color.a * 0.55 * exp(-distance * distance * 60.0);
    return color;
}
