// The text input's shader on the demo's Shader page: slate blue and orange flow around its border, with a soft glow outside it. It only
// touches the edge, so what's typed inside stays as sharp as ever.
//
// shader_glow_ps.h is this file compiled. After changing it, run this from a Visual Studio developer prompt:
//   fxc /nologo /T ps_4_0_level_9_3 /E main /Vn kGlowBytes /Fh shader_glow_ps.h shader_glow.hlsl

Texture2D Source : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer Constants : register(b0) {
    float seconds;
    float width;
    float height;
    float margin;   // the room left around the control for the glow: 14 DIPs
};

// How far a point is outside a rounded box: negative inside it.
float rounded_box(float2 p, float2 half_size, float radius) {
    float2 q = abs(p) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// How far round the box a point is, from 0 to 1. It measures along the sides, so the color keeps one speed on a wide, short box.
float around(float2 p, float2 half_size) {
    float2 c = clamp(p, -half_size, half_size);
    float2 h = half_size;
    float top_or_bottom = p.y < 0.0 ? c.x + h.x : 3.0 * h.x + 2.0 * h.y - c.x;
    float left_or_right = p.x > 0.0 ? 2.0 * h.x + c.y + h.y : 4.0 * h.x + 3.0 * h.y - c.y;
    return (h.x - abs(p.x) > h.y - abs(p.y) ? top_or_bottom : left_or_right) / (4.0 * (h.x + h.y));
}

float4 main(float4 position : SV_POSITION, float4 scene : SCENE_POSITION, float4 source : TEXCOORD0) : SV_Target {
    float4 pixel = Source.Sample(SourceSampler, source.xy);
    float2 size = float2(width, height);
    float2 p = scene.xy - size * 0.5;
    float2 half_size = size * 0.5 - margin;
    float dip = margin / 14.0;
    float edge = rounded_box(p, half_size, 4.0 * dip) / dip;
    float way = around(p, half_size);

    // Orange through dusty rose to slate blue and back, twice around the box. Cosines meet where they start, so there's no seam.
    float3 flow = float3(0.66, 0.50, 0.50) + float3(0.34, 0.05, -0.35) * cos((2.0 * way - seconds * 0.11) * 6.28318);

    // Two slow waves of brightness going opposite ways, so the light gathers and thins instead of just turning.
    float wave = 0.5 + 0.5 * cos((way - seconds * 0.07) * 6.28318);
    float swell = 0.8 + 0.2 * cos((2.0 * way + seconds * 0.05) * 6.28318);
    float level = 0.35 + 0.65 * wave * wave * swell;

    // A fine line on the edge, soft on the outside and tight on the inside, to stay clear of the text.
    float stroke = exp(-edge * edge / 0.6);
    float bloom = edge > 0.0 ? exp(-edge / 4.0) : exp(edge / 1.7);
    float strength = saturate(stroke * 0.95 + bloom * 0.50) * level;
    return float4(pixel.rgb * (1.0 - strength) + flow * strength, pixel.a + strength * (1.0 - pixel.a));
}
