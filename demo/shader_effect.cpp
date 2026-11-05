/* Runs shader_ripple.hlsl and shader_glow.hlsl as a Direct2D custom effect, with a control's own pixels as the input image. */
#include "shader_effect.h"

#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1effectauthor.h>
#include <d2d1effecthelpers.h>
#include <new>

#include "shader_glow_ps.h"
#include "shader_ripple_ps.h"

namespace {

// {8C9D2A5E-3F7B-4C1E-9A6D-5B2E7F1C4D30}
constexpr GUID kEffectId = {0x8c9d2a5e, 0x3f7b, 0x4c1e, {0x9a, 0x6d, 0x5b, 0x2e, 0x7f, 0x1c, 0x4d, 0x30}};
// {2E6F4B1A-7D3C-4A58-B1E9-6C0D8A3F5E72}
constexpr GUID kRippleId = {0x2e6f4b1a, 0x7d3c, 0x4a58, {0xb1, 0xe9, 0x6c, 0x0d, 0x8a, 0x3f, 0x5e, 0x72}};
// {A17C3E90-52D4-4F6B-8E21-0B9D6F4C7A15}
constexpr GUID kGlowId = {0xa17c3e90, 0x52d4, 0x4f6b, {0x8e, 0x21, 0x0b, 0x9d, 0x6f, 0x4c, 0x7a, 0x15}};

// The furthest shader_ripple.hlsl looks away from a pixel.
constexpr LONG kReach = 8;

constexpr wchar_t kEffectXml[] =
    L"<?xml version='1.0'?>"
    L"<Effect>"
    L"  <Property name='DisplayName' type='string' value='Slim UI demo shader'/>"
    L"  <Property name='Author' type='string' value='Slim UI'/>"
    L"  <Property name='Category' type='string' value='Stylize'/>"
    L"  <Property name='Description' type='string' value='Runs a shader over a control'/>"
    L"  <Inputs>"
    L"    <Input name='Source'/>"
    L"  </Inputs>"
    L"  <Property name='Constants' type='vector4'>"
    L"    <Property name='DisplayName' type='string' value='Seconds, width, height, margin'/>"
    L"    <Property name='Default' type='vector4' value='(0.0, 1.0, 1.0, 0.0)'/>"
    L"  </Property>"
    L"  <Property name='Kind' type='int32'>"
    L"    <Property name='DisplayName' type='string' value='Which shader'/>"
    L"    <Property name='Default' type='int32' value='0'/>"
    L"  </Property>"
    L"</Effect>";

class ControlShader final : public ID2D1EffectImpl, public ID2D1DrawTransform {
public:
    static HRESULT CALLBACK Create(IUnknown** effect) {
        *effect = static_cast<ID2D1EffectImpl*>(new (std::nothrow) ControlShader);
        return *effect ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT SetConstants(D2D_VECTOR_4F value) {
        constants_ = value;
        return S_OK;
    }
    D2D_VECTOR_4F GetConstants() const { return constants_; }
    HRESULT SetKind(INT32 value) {
        kind_ = value;
        return S_OK;
    }
    INT32 GetKind() const { return kind_; }

    // ID2D1EffectImpl
    IFACEMETHODIMP Initialize(ID2D1EffectContext* context, ID2D1TransformGraph* graph) override {
        HRESULT hr = context->LoadPixelShader(kRippleId, kRippleBytes, sizeof(kRippleBytes));
        if (SUCCEEDED(hr)) hr = context->LoadPixelShader(kGlowId, kGlowBytes, sizeof(kGlowBytes));
        return SUCCEEDED(hr) ? graph->SetSingleTransformNode(this) : hr;
    }
    IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE) override {
        HRESULT hr = info_->SetPixelShader(kind_ == DEMO_SHADER_GLOW ? kGlowId : kRippleId);
        return SUCCEEDED(hr) ? info_->SetPixelShaderConstantBuffer(reinterpret_cast<const BYTE*>(&constants_), sizeof(constants_)) : hr;
    }
    IFACEMETHODIMP SetGraph(ID2D1TransformGraph*) override { return E_NOTIMPL; }

    // ID2D1DrawTransform
    IFACEMETHODIMP SetDrawInfo(ID2D1DrawInfo* info) override {
        info_ = info;
        return info_->SetPixelShader(kRippleId);
    }

    // ID2D1Transform: a shader's output is the size of its input, and it may look a little way past the pixel it's working on.
    IFACEMETHODIMP MapOutputRectToInputRects(const D2D1_RECT_L* output, D2D1_RECT_L* inputs, UINT32 count) const override {
        if (count != 1) return E_INVALIDARG;
        inputs[0] = {output->left - kReach, output->top - kReach, output->right + kReach, output->bottom + kReach};
        return S_OK;
    }
    IFACEMETHODIMP MapInputRectsToOutputRect(const D2D1_RECT_L* inputs, const D2D1_RECT_L*, UINT32 count, D2D1_RECT_L* output, D2D1_RECT_L* opaque) override {
        if (count != 1) return E_INVALIDARG;
        area_ = inputs[0];
        *output = area_;
        *opaque = {0, 0, 0, 0};
        return S_OK;
    }
    IFACEMETHODIMP MapInvalidRect(UINT32, D2D1_RECT_L, D2D1_RECT_L* output) const override {
        *output = area_;
        return S_OK;
    }
    IFACEMETHODIMP_(UINT32) GetInputCount() const override { return 1; }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID id, void** out) override {
        if (id == __uuidof(ID2D1EffectImpl) || id == __uuidof(IUnknown)) *out = static_cast<ID2D1EffectImpl*>(this);
        else if (id == __uuidof(ID2D1DrawTransform) || id == __uuidof(ID2D1Transform) || id == __uuidof(ID2D1TransformNode))
            *out = static_cast<ID2D1DrawTransform*>(this);
        else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return ++references_; }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG left = --references_;
        if (!left) delete this;
        return left;
    }

private:
    ID2D1DrawInfo* info_ = nullptr;  // owned by the effect, and valid for its whole life
    D2D_VECTOR_4F  constants_ = {0.0f, 1.0f, 1.0f, 0.0f};
    D2D1_RECT_L    area_ = {0, 0, 0, 0};
    INT32          kind_ = DEMO_SHADER_RIPPLE;
    ULONG          references_ = 1;
};

template <typename T>
void Drop(T*& object) {
    if (object) object->Release();
    object = nullptr;
}

}  // namespace

/* One bitmap and effect per shader, kept between frames so a running page allocates nothing new. */
struct Pass {
    ID2D1Effect*             effect = nullptr;
    ID2D1BitmapRenderTarget* bitmap = nullptr;
    UINT32                   bitmap_w = 0, bitmap_h = 0;
    float                    x = 0.0f, y = 0.0f, margin = 0.0f;
};

struct DemoShader {
    void*               target = nullptr;
    ID2D1DeviceContext* context = nullptr;
    Pass                passes[DEMO_SHADER_KINDS];
};

static void DropAll(DemoShader* shader) {
    for (Pass& pass : shader->passes) {
        Drop(pass.bitmap);
        Drop(pass.effect);
    }
    Drop(shader->context);
}

/* Everything here belongs to one render target, and the renderer replaces its target if the graphics device is lost. */
static void Prepare(DemoShader* shader, void* render_target) {
    static const D2D1_PROPERTY_BINDING bindings[] = {
        D2D1_VALUE_TYPE_BINDING(L"Constants", &ControlShader::SetConstants, &ControlShader::GetConstants),
        D2D1_VALUE_TYPE_BINDING(L"Kind", &ControlShader::SetKind, &ControlShader::GetKind),
    };
    ID2D1Factory* factory = nullptr;
    ID2D1Factory1* factory1 = nullptr;

    DropAll(shader);
    shader->target = render_target;
    if (FAILED(static_cast<ID2D1RenderTarget*>(render_target)->QueryInterface(&shader->context))) return;
    shader->context->GetFactory(&factory);
    HRESULT hr = factory->QueryInterface(&factory1);
    if (SUCCEEDED(hr)) hr = factory1->RegisterEffectFromString(kEffectId, kEffectXml, bindings, ARRAYSIZE(bindings), ControlShader::Create);
    for (int kind = 0; SUCCEEDED(hr) && kind < DEMO_SHADER_KINDS; kind++) {
        hr = shader->context->CreateEffect(kEffectId, &shader->passes[kind].effect);
        if (SUCCEEDED(hr)) hr = shader->passes[kind].effect->SetValue(1, static_cast<INT32>(kind));
    }
    Drop(factory1);
    Drop(factory);
}

extern "C" DemoShader* demo_shader_create(void) {
    return new (std::nothrow) DemoShader;
}

extern "C" void* demo_shader_begin(DemoShader* shader, void* render_target, int kind, float x, float y, float w, float h, float margin) {
    if (!shader || !render_target) return nullptr;
    if (shader->target != render_target) Prepare(shader, render_target);
    Pass& pass = shader->passes[kind];
    if (!pass.effect) return nullptr;

    const UINT32 bitmap_w = static_cast<UINT32>(w + margin * 2.0f + 0.5f), bitmap_h = static_cast<UINT32>(h + margin * 2.0f + 0.5f);
    if (!pass.bitmap || pass.bitmap_w != bitmap_w || pass.bitmap_h != bitmap_h) {
        const D2D1_SIZE_F size = D2D1::SizeF(static_cast<float>(bitmap_w), static_cast<float>(bitmap_h));
        const D2D1_PIXEL_FORMAT format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        Drop(pass.bitmap);
        if (FAILED(shader->context->CreateCompatibleRenderTarget(&size, nullptr, &format, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &pass.bitmap)))
            return nullptr;
        pass.bitmap_w = bitmap_w;
        pass.bitmap_h = bitmap_h;
    }
    pass.x = x - margin;
    pass.y = y - margin;
    pass.margin = margin;
    pass.bitmap->BeginDraw();
    pass.bitmap->Clear(D2D1::ColorF(0.0f, 0.0f));
    pass.bitmap->SetTransform(D2D1::Matrix3x2F::Translation(margin - x, margin - y));
    return static_cast<ID2D1RenderTarget*>(pass.bitmap);
}

extern "C" void demo_shader_end(DemoShader* shader, int kind, float seconds) {
    Pass& pass = shader->passes[kind];
    ID2D1Bitmap* bitmap = nullptr;
    if (FAILED(pass.bitmap->EndDraw()) || FAILED(pass.bitmap->GetBitmap(&bitmap))) return;
    pass.effect->SetInput(0, bitmap);
    pass.effect->SetValue(0, D2D1::Vector4F(seconds, static_cast<float>(pass.bitmap_w), static_cast<float>(pass.bitmap_h), pass.margin));
    shader->context->DrawImage(pass.effect, D2D1::Point2F(pass.x, pass.y));
    bitmap->Release();
}

extern "C" void demo_shader_release(DemoShader* shader) {
    if (!shader) return;
    DropAll(shader);
    delete shader;
}
