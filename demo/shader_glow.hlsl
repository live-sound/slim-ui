// The text input's shader on the demo's Shader page: an orange and a blue comet chase each other around its border. It only touches the
// edges, so what's typed inside stays as sharp as ever.
//
// shader_glow_ps.h is this file compiled. After changing it, run this from a Visual Studio developer prompt:
//   fxc /nologo /T ps_4_0_level_9_3 /E main /Vn kGlowBytes /Fh shader_glow_ps.h shader_glow.hlsl

Texture2D Source : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer Constants : register(b0) {
    float seconds;
    float width;
    float height;
    float margin;   // the room left around the control for the glow
};

// How far a point is outside a rounded box: negative inside it.
float rounded_box(float2 p, float2 half_size, float radius) {
    float2 q = abs(p) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// A long tail that brightens towards its head, and a head that fades out instead of stopping dead.
float comet(float position) {
    float t = frac(position);
    return pow(t, 5.0) * smoothstep(1.0, 0.94, t);
}

float4 main(float4 position : SV_POSITION, float4 scene : SCENE_POSITION, float4 source : TEXCOORD0) : SV_Target {
    float4 color = Source.Sample(SourceSampler, source.xy);
    float2 size = float2(width, height);
    float2 p = scene.xy - size * 0.5;
    float edge = rounded_box(p, size * 0.5 - margin, 6.0);

    // Where this pixel is on the way around the box, from 0 to 1.
    float around = atan2(p.y / size.y, p.x / size.x) / 6.28318 + 0.5;
    float orange = comet(around - seconds * 0.22);
    float blue = comet(around - seconds * 0.22 + 0.5);

    // Soft on the outside and tight on the inside, to stay clear of the text.
    float ring = exp(-abs(edge) / (edge > 0.0 ? 5.0 : 1.5));
    float3 glow = (float3(1.00, 0.55, 0.16) * orange + float3(0.30, 0.76, 1.00) * blue) * ring * 1.7;

    float strength = saturate(max(glow.r, max(glow.g, glow.b)));
    return float4(color.rgb + glow * (1.0 - color.a * 0.4), saturate(color.a + strength * (1.0 - color.a)));
}
