// The image control's shader on the demo's Shader page: the logo's flat panel becomes a slow night sky, and a warm light runs along its
// swoosh. The logo itself stays still, so its edges are as sharp as the picture.
//
// shader_dusk_ps.h is this file compiled. After changing it, run this from a Visual Studio developer prompt:
//   fxc /nologo /T ps_4_0_level_9_3 /E main /Vn kDuskBytes /Fh shader_dusk_ps.h shader_dusk.hlsl

Texture2D Source : register(t0);
SamplerState SourceSampler : register(s0);

cbuffer Constants : register(b0) {
    float seconds;
    float width;
    float height;
    float margin;
};

// The panel's color in logo.svg, which tells it apart from the letters and the swoosh.
static const float3 kPanel = float3(0.298, 0.333, 0.420);

float cloud(float2 p, float2 middle, float2 size) {
    float2 d = (p - middle) / size;
    return exp(-dot(d, d));
}

float3 night_sky(float2 p, float t) {
    // Bending space a little, and differently over time, makes the clouds change shape instead of sliding.
    float2 q = p + 0.10 * sin(p.yx * float2(3.1, 2.3) + t * float2(0.31, -0.27))
                 + 0.05 * sin(p.yx * float2(7.3, 5.1) + t * float2(-0.23, 0.19));
    float3 sky = float3(0.078, 0.098, 0.153);
    sky += float3(0.153, 0.251, 0.431) * cloud(q, float2(0.55 + 0.35 * sin(t * 0.21), 0.15 + 0.10 * cos(t * 0.17)), float2(0.95, 0.55));
    sky += float3(0.247, 0.208, 0.376) * cloud(q, float2(1.70 + 0.30 * cos(t * 0.19), 0.25 + 0.12 * sin(t * 0.23)), float2(0.70, 0.55));
    sky += float3(0.380, 0.160, 0.270) * cloud(q, float2(1.10 + 0.70 * sin(t * 0.13), 1.05), float2(1.05, 0.45));
    return sky;
}

float4 main(float4 position : SV_POSITION, float4 scene : SCENE_POSITION, float4 source : TEXCOORD0) : SV_Target {
    float4 pixel = Source.Sample(SourceSampler, source.xy);
    float3 color = pixel.rgb / max(pixel.a, 0.001);   // colors arrive premultiplied

    // The logo's parts, by color: how much of this pixel is letter or swoosh, and how far their shadow darkens the panel.
    float letters = saturate((min(color.r, min(color.g, color.b)) - kPanel.r) / (1.0 - kPanel.r));
    float swoosh = saturate((color.r - color.b) / 0.84);
    float front = max(letters, swoosh);
    float shade = saturate(max(color.r, max(color.g, color.b)) / kPanel.b);

    // The light runs left to right, then waits off the end before coming round again.
    float across = scene.x / width;
    float run = frac(seconds * 0.17) * 2.8 - 0.4;
    float gap = (across - run) / 0.13;
    float pulse = exp(-gap * gap);

    // Swap the panel's color for the sky under everything else, so antialiased edges and the shadow come out right.
    float2 p = scene.xy / height;
    float3 sky = night_sky(p, seconds) + float3(0.30, 0.14, 0.05) * cloud(float2(across, p.y), float2(run, 0.74), float2(0.22, 0.26));
    color += (sky * (0.55 + 0.45 * shade) - kPanel * shade) * (1.0 - front);
    color += float3(0.65, 0.57, 0.45) * swoosh * pulse;
    return float4(saturate(color) * pixel.a, pixel.a);
}
