/* sui_d2d.h — D2D/DWrite/WIC COM vtable declarations, value types, GUIDs, factory imports
 * Included from slim_ui.h under #ifdef SUI_IMPLEMENTATION — no include guard.
 */

/* ── sin/cos for animation support ─────────────────────────────────────── */
#ifdef SUI_NO_CRT
static float sui_sinf(float x) {
    float x2, r;
    while (x > 3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    x2 = x*x;
    r = x * (1.0f - x2/6.0f * (1.0f - x2/20.0f * (1.0f - x2/42.0f)));
    return r;
}
static float sui_cosf(float x) { return sui_sinf(x + 1.5707963f); }
#else
#include <math.h>
#define sui_sinf sinf
#define sui_cosf cosf
#endif

#ifdef SUI_FEATURE_PNG
/* ── IStream forward declaration (compatible with objidl.h) ─────────────── */
#ifndef __IStream_FWD_DEFINED__
#define __IStream_FWD_DEFINED__
typedef struct IStream IStream;
#endif
__declspec(dllimport) HRESULT __stdcall CreateStreamOnHGlobal(HGLOBAL, BOOL, IStream**);

/* ── WIC enums ───────────────────────────────────────────────────────────── */
enum { WICDecodeMetadataCacheOnLoad = 0x1 };
enum { WICBitmapDitherTypeNone      = 0   };
enum { WICBitmapPaletteTypeCustom   = 0   };

/* ── WICRect ─────────────────────────────────────────────────────────────── */
typedef struct { INT32 X, Y, Width, Height; } WICRect;

/* ── IWICBitmapSource (IUnknown + 5 methods) ─────────────────────────────── */
typedef struct IWICBitmapSource IWICBitmapSource;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICBitmapSource*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IWICBitmapSource*);
    ULONG   (STDMETHODCALLTYPE *Release)(IWICBitmapSource*);
    HRESULT (STDMETHODCALLTYPE *GetSize)(IWICBitmapSource*, UINT32*, UINT32*);
    void *GetPixelFormat, *GetResolution, *CopyPalette;
    HRESULT (STDMETHODCALLTYPE *CopyPixels)(IWICBitmapSource*, const WICRect*, UINT32, UINT32, BYTE*);
} IWICBitmapSourceVtbl;
struct IWICBitmapSource { const IWICBitmapSourceVtbl* lpVtbl; };

/* ── IWICBitmapDecoder (IUnknown + 11 methods) ───────────────────────────── */
typedef struct IWICBitmapDecoder     IWICBitmapDecoder;
typedef struct IWICBitmapFrameDecode IWICBitmapFrameDecode;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICBitmapDecoder*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IWICBitmapDecoder*);
    ULONG   (STDMETHODCALLTYPE *Release)(IWICBitmapDecoder*);
    void *QueryCapability, *Initialize, *GetContainerFormat, *GetDecoderInfo;
    void *CopyPalette, *GetMetadataQueryReader, *GetPreview, *GetColorContexts;
    void *GetThumbnail, *GetFrameCount;
    HRESULT (STDMETHODCALLTYPE *GetFrame)(IWICBitmapDecoder*, UINT32, IWICBitmapFrameDecode**);
} IWICBitmapDecoderVtbl;
struct IWICBitmapDecoder { const IWICBitmapDecoderVtbl* lpVtbl; };

/* ── IWICBitmapFrameDecode (IWICBitmapSource + 3 own methods) ────────────── */
typedef struct {
    IWICBitmapSourceVtbl Base;
    void *GetMetadataQueryReader, *GetColorContexts, *GetThumbnail;
} IWICBitmapFrameDecodeVtbl;
struct IWICBitmapFrameDecode { const IWICBitmapFrameDecodeVtbl* lpVtbl; };

/* ── IWICFormatConverter (IWICBitmapSource + 2 own methods) ──────────────── */
typedef struct IWICFormatConverter IWICFormatConverter;
typedef struct {
    IWICBitmapSourceVtbl Base;
    HRESULT (STDMETHODCALLTYPE *Initialize)(IWICFormatConverter*, IWICBitmapSource*,
        const GUID*, int, void*, double, int);
    void *CanConvert;
} IWICFormatConverterVtbl;
struct IWICFormatConverter { const IWICFormatConverterVtbl* lpVtbl; };

/* ── IWICImagingFactory (IUnknown + 25 methods; CreateDecoderFromStream +
 *    CreateFormatConverter are the two we use) ──────────────────────────── */
typedef struct IWICImagingFactory IWICImagingFactory;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWICImagingFactory*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IWICImagingFactory*);
    ULONG   (STDMETHODCALLTYPE *Release)(IWICImagingFactory*);
    void *CreateDecoderFromFilename;
    HRESULT (STDMETHODCALLTYPE *CreateDecoderFromStream)(IWICImagingFactory*,
        IStream*, const GUID*, DWORD, IWICBitmapDecoder**);
    void *CreateDecoderFromFileHandle, *CreateComponentInfo, *CreateDecoder;
    void *CreateEncoder, *CreatePalette;
    HRESULT (STDMETHODCALLTYPE *CreateFormatConverter)(IWICImagingFactory*, IWICFormatConverter**);
    /* remaining 15 methods unused */
} IWICImagingFactoryVtbl;
struct IWICImagingFactory { const IWICImagingFactoryVtbl* lpVtbl; };

/* ── WIC GUIDs ───────────────────────────────────────────────────────────── */
static const CLSID CLSID_WICImagingFactory =
    {0xcacaf262,0x9370,0x4615,{0xa1,0x3b,0x9f,0x55,0x39,0xda,0x4c,0x0a}};
static const IID  IID_IWICImagingFactory =
    {0xec5ec8a9,0xc395,0x4314,{0x9c,0x77,0x54,0xd7,0xa9,0x35,0xff,0x70}};
static const GUID GUID_WICPixelFormat32bppPBGRA =
    {0x6fddc324,0x4e03,0x4bfe,{0xb1,0x85,0x3d,0x77,0x76,0x8d,0xc9,0x10}};

#endif /* SUI_FEATURE_PNG */

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

/* ── Minimal D2D/DWrite C-compatible declarations ───────────────────────── *
 * System headers d2d1.h/dwrite.h use C++ class syntax and cannot be        *
 * included in C. We declare just enough COM vtable structures, enums, and  *
 * value types to drive the rendering code in pure C.                       */

/* D2D1/DWrite enums — only values actually used */
enum { sui_DXGI_FORMAT_B8G8R8A8_UNORM = 87 };
#define DXGI_FORMAT_B8G8R8A8_UNORM sui_DXGI_FORMAT_B8G8R8A8_UNORM
enum { D2D1_ALPHA_MODE_PREMULTIPLIED = 1 };
enum { D2D1_FACTORY_TYPE_SINGLE_THREADED = 0 };
enum { D2D1_RENDER_TARGET_TYPE_DEFAULT = 0 };
enum { D2D1_RENDER_TARGET_USAGE_NONE = 0 };
enum { D2D1_FEATURE_LEVEL_DEFAULT = 0 };
enum { D2D1_PRESENT_OPTIONS_IMMEDIATELY = 2 };
enum { D2D1_DRAW_TEXT_OPTIONS_NONE = 0 };
enum {
    D2D1_TEXT_ANTIALIAS_MODE_DEFAULT = 0,
    D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE = 1,
    D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE = 2,
    D2D1_TEXT_ANTIALIAS_MODE_ALIASED = 3
};
enum { D2D1_BITMAP_INTERPOLATION_MODE_LINEAR = 1 };
enum { D2D1_FILL_MODE_ALTERNATE = 0 };
enum { DWRITE_FACTORY_TYPE_SHARED = 0 };
enum { DWRITE_FONT_WEIGHT_NORMAL = 400, DWRITE_FONT_WEIGHT_SEMI_BOLD = 600, DWRITE_FONT_WEIGHT_BOLD = 700 };
enum { DWRITE_FONT_STYLE_NORMAL = 0, DWRITE_FONT_STYLE_OBLIQUE = 1, DWRITE_FONT_STYLE_ITALIC = 2 };
enum { DWRITE_FONT_STRETCH_NORMAL = 5 };
enum { DWRITE_MEASURING_MODE_NATURAL = 0, DWRITE_MEASURING_MODE_GDI_NATURAL = 2 };
enum { DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC = 5 };
enum { DWRITE_GRID_FIT_MODE_ENABLED = 2 };
enum { DWRITE_WORD_WRAPPING_WRAP = 0, DWRITE_WORD_WRAPPING_NO_WRAP = 1 };

/* D2D1 value types */
typedef struct { float r, g, b, a; } D2D1_COLOR_F;
typedef struct { float left, top, right, bottom; } D2D1_RECT_F;
typedef struct { float x, y; } D2D1_POINT_2F;
typedef struct { UINT32 width, height; } D2D1_SIZE_U;
typedef struct { int format; int alphaMode; } D2D1_PIXEL_FORMAT;
typedef struct { int type; D2D1_PIXEL_FORMAT pixelFormat; float dpiX, dpiY; int usage; int minLevel; } D2D1_RENDER_TARGET_PROPERTIES;
typedef struct { HWND hwnd; D2D1_SIZE_U pixelSize; int presentOptions; } D2D1_HWND_RENDER_TARGET_PROPERTIES;
typedef struct { D2D1_PIXEL_FORMAT pixelFormat; float dpiX, dpiY; } D2D1_BITMAP_PROPERTIES;

typedef struct D2D1_ROUNDED_RECT {
    D2D1_RECT_F rect;
    float radiusX;
    float radiusY;
} D2D1_ROUNDED_RECT;

typedef struct D2D1_ELLIPSE {
    D2D1_POINT_2F point;
    FLOAT radiusX;
    FLOAT radiusY;
} D2D1_ELLIPSE;

typedef struct D2D1_MATRIX_3X2_F {
    float _11, _12, _21, _22, _31, _32;
} D2D1_MATRIX_3X2_F;

/* Minimal geometry interface types (for focus ring drawing) */
typedef struct ID2D1Geometry              ID2D1Geometry;
typedef struct ID2D1RectangleGeometry     ID2D1RectangleGeometry;
typedef struct ID2D1RoundedRectangleGeometry ID2D1RoundedRectangleGeometry;
typedef struct ID2D1GeometryGroup         ID2D1GeometryGroup;
typedef struct { void *QueryInterface, *AddRef; ULONG (STDMETHODCALLTYPE *Release)(void*); } sui__MinGeomVtbl;
struct ID2D1Geometry                 { const sui__MinGeomVtbl* lpVtbl; };
struct ID2D1RectangleGeometry        { const sui__MinGeomVtbl* lpVtbl; };
struct ID2D1RoundedRectangleGeometry { const sui__MinGeomVtbl* lpVtbl; };
struct ID2D1GeometryGroup            { const sui__MinGeomVtbl* lpVtbl; };
/* A path geometry and the sink that fills it (continuous rounded shapes): the vtables in COM order, typed where called. The sink
 * Open returns is an ID2D1GeometrySink, whose vtable begins with ID2D1SimplifiedGeometrySink's methods, the only ones used. */
typedef struct ID2D1PathGeometry  ID2D1PathGeometry;
typedef struct ID2D1GeometrySink  ID2D1GeometrySink;
typedef struct { D2D1_POINT_2F point1, point2, point3; } D2D1_BEZIER_SEGMENT;
typedef struct {
    void *QueryInterface, *AddRef; ULONG (STDMETHODCALLTYPE *Release)(void*);
    void *GetFactory;                                            /* ID2D1Resource */
    void *GetBounds, *GetWidenedBounds, *StrokeContainsPoint, *FillContainsPoint, *CompareWithGeometry, *Simplify, *Tessellate,
         *CombineWithGeometry, *Outline, *ComputeArea, *ComputeLength, *ComputePointAtLength, *Widen;   /* ID2D1Geometry */
    HRESULT (STDMETHODCALLTYPE *Open)(ID2D1PathGeometry*, ID2D1GeometrySink**);
} sui__PathGeomVtbl;
struct ID2D1PathGeometry { const sui__PathGeomVtbl* lpVtbl; };
typedef struct {
    void *QueryInterface, *AddRef; ULONG (STDMETHODCALLTYPE *Release)(void*);
    void (STDMETHODCALLTYPE *SetFillMode)(ID2D1GeometrySink*, int);
    void *SetSegmentFlags;
    void (STDMETHODCALLTYPE *BeginFigure)(ID2D1GeometrySink*, D2D1_POINT_2F, int);
    void (STDMETHODCALLTYPE *AddLines)(ID2D1GeometrySink*, const D2D1_POINT_2F*, UINT32);
    void (STDMETHODCALLTYPE *AddBeziers)(ID2D1GeometrySink*, const D2D1_BEZIER_SEGMENT*, UINT32);
    void (STDMETHODCALLTYPE *EndFigure)(ID2D1GeometrySink*, int);
    HRESULT (STDMETHODCALLTYPE *Close)(ID2D1GeometrySink*);
} sui__GeomSinkVtbl;
struct ID2D1GeometrySink { const sui__GeomSinkVtbl* lpVtbl; };

/* DWrite text alignment constants */
#define DWRITE_TEXT_ALIGNMENT_CENTER      2
#define DWRITE_PARAGRAPH_ALIGNMENT_CENTER 2

/* Forward-declare COM interfaces */
typedef struct ID2D1Factory ID2D1Factory;
typedef struct ID2D1RenderTarget ID2D1RenderTarget;
typedef struct ID2D1HwndRenderTarget ID2D1HwndRenderTarget;
typedef struct ID2D1DCRenderTarget ID2D1DCRenderTarget;
typedef struct ID2D1Bitmap ID2D1Bitmap;
typedef struct ID2D1Brush ID2D1Brush;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;
typedef struct IDWriteFactory IDWriteFactory;
typedef struct IDWriteRenderingParams IDWriteRenderingParams;
typedef struct IDWriteTextFormat IDWriteTextFormat;
typedef struct IDWriteTextLayout IDWriteTextLayout;

typedef struct D2D1_LAYER_PARAMETERS {
    D2D1_RECT_F contentBounds;
    ID2D1Geometry* geometricMask;
    int maskAntialiasMode;
    D2D1_MATRIX_3X2_F maskTransform;
    float opacity;
    ID2D1Brush* opacityBrush;
    int layerOptions;
} D2D1_LAYER_PARAMETERS;

/* ── COM vtable structures (C: obj->lpVtbl->Method(obj, ...)) ──────── */

/* ID2D1Resource vtable (IUnknown[3] + GetFactory) — base for nesting */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(void*);
    ULONG (STDMETHODCALLTYPE *Release)(void*);
    void* GetFactory;
} sui__D2DResourceVtbl;

/* ID2D1Image vtable (= Resource, no new methods) */
typedef struct { sui__D2DResourceVtbl Base; } sui__D2DImageVtbl;

/* ID2D1Bitmap vtable (Image + 7 methods) */
typedef struct {
    sui__D2DImageVtbl Base;
    void *GetSize, *GetPixelSize, *GetPixelFormat, *GetDpi;
    void *CopyFromBitmap, *CopyFromRenderTarget, *CopyFromMemory;
} ID2D1BitmapVtbl;
struct ID2D1Bitmap { const ID2D1BitmapVtbl* lpVtbl; };

/* ID2D1Brush vtable (Resource + 4 methods) */
typedef struct {
    sui__D2DResourceVtbl Base;
    void *SetOpacity, *SetTransform, *GetOpacity, *GetTransform;
} sui__D2DBrushVtbl;

/* ID2D1SolidColorBrush vtable (Brush + 2 methods) */
typedef struct {
    sui__D2DBrushVtbl Base;
    void (STDMETHODCALLTYPE *SetColor)(ID2D1SolidColorBrush*, const D2D1_COLOR_F*);
    void *GetColor;
} ID2D1SolidColorBrushVtbl;
struct ID2D1SolidColorBrush { const ID2D1SolidColorBrushVtbl* lpVtbl; };

/* ID2D1RenderTarget vtable (flat: IUnknown[3] + Resource[1] + RT[53] = 57) */
typedef struct {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ID2D1RenderTarget*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(ID2D1RenderTarget*);
    ULONG (STDMETHODCALLTYPE *Release)(ID2D1RenderTarget*);
    /* ID2D1Resource */
    void* GetFactory;
    /* ID2D1RenderTarget — 53 methods in vtable order */
    HRESULT (STDMETHODCALLTYPE *CreateBitmap)(ID2D1RenderTarget*, D2D1_SIZE_U, const void*, UINT32,
        const D2D1_BITMAP_PROPERTIES*, ID2D1Bitmap**);
#ifdef SUI_FEATURE_PNG
    HRESULT (STDMETHODCALLTYPE *CreateBitmapFromWicBitmap)(ID2D1RenderTarget*,
        IWICBitmapSource*, const D2D1_BITMAP_PROPERTIES*, ID2D1Bitmap**);
#else
    void *CreateBitmapFromWicBitmap;
#endif
    void *CreateSharedBitmap, *CreateBitmapBrush;
    HRESULT (STDMETHODCALLTYPE *CreateSolidColorBrush)(ID2D1RenderTarget*, const D2D1_COLOR_F*,
        const void*, ID2D1SolidColorBrush**);
    void *CreateGradientStopCollection, *CreateLinearGradientBrush, *CreateRadialGradientBrush;
    HRESULT (STDMETHODCALLTYPE *CreateCompatibleRenderTarget)(ID2D1RenderTarget*,
        const void*, const void*, const void*, int, void**);
    void *CreateLayer, *CreateMesh;
    void (STDMETHODCALLTYPE *DrawLine)(ID2D1RenderTarget*, D2D1_POINT_2F, D2D1_POINT_2F,
        ID2D1Brush*, float, void*);
    void* DrawRectangle;
    void (STDMETHODCALLTYPE *FillRectangle)(ID2D1RenderTarget*, const D2D1_RECT_F*, ID2D1Brush*);
    void (STDMETHODCALLTYPE *DrawRoundedRectangle)(ID2D1RenderTarget*, const D2D1_ROUNDED_RECT*, ID2D1Brush*, float, void*);
    void (STDMETHODCALLTYPE *FillRoundedRectangle)(ID2D1RenderTarget*, const D2D1_ROUNDED_RECT*, ID2D1Brush*);
    void (STDMETHODCALLTYPE *DrawEllipse)(ID2D1RenderTarget*, const D2D1_ELLIPSE*, ID2D1Brush*, float, void*);
    void (STDMETHODCALLTYPE *FillEllipse)(ID2D1RenderTarget*, const D2D1_ELLIPSE*, ID2D1Brush*);
    void *DrawGeometry;
    void (STDMETHODCALLTYPE *FillGeometry)(ID2D1RenderTarget*, ID2D1Geometry*, ID2D1Brush*, ID2D1Brush*);
    void *FillMesh, *FillOpacityMask;
    void (STDMETHODCALLTYPE *DrawBitmap)(ID2D1RenderTarget*, ID2D1Bitmap*, const D2D1_RECT_F*,
        float, int, const D2D1_RECT_F*);
    void (STDMETHODCALLTYPE *DrawText)(ID2D1RenderTarget*, const wchar_t*, UINT32,
        IDWriteTextFormat*, const D2D1_RECT_F*, ID2D1Brush*, int, int);
    void (STDMETHODCALLTYPE *DrawTextLayout)(ID2D1RenderTarget*, D2D1_POINT_2F,
        IDWriteTextLayout*, ID2D1Brush*, int);
    void *DrawGlyphRun;
    void (STDMETHODCALLTYPE *SetTransform)(ID2D1RenderTarget*, const D2D1_MATRIX_3X2_F*);
    void (STDMETHODCALLTYPE *GetTransform)(ID2D1RenderTarget*, D2D1_MATRIX_3X2_F*);
    void *SetAntialiasMode, *GetAntialiasMode;
    void (STDMETHODCALLTYPE *SetTextAntialiasMode)(ID2D1RenderTarget*, int);
    int (STDMETHODCALLTYPE *GetTextAntialiasMode)(ID2D1RenderTarget*);
    void (STDMETHODCALLTYPE *SetTextRenderingParams)(ID2D1RenderTarget*, IDWriteRenderingParams*);
    void (STDMETHODCALLTYPE *GetTextRenderingParams)(ID2D1RenderTarget*, IDWriteRenderingParams**);
    void *SetTags, *GetTags;
    void (STDMETHODCALLTYPE *PushLayer)(ID2D1RenderTarget*, const D2D1_LAYER_PARAMETERS*, void*);
    void (STDMETHODCALLTYPE *PopLayer)(ID2D1RenderTarget*);
    void *Flush, *SaveDrawingState, *RestoreDrawingState;
    void (STDMETHODCALLTYPE *PushAxisAlignedClip)(ID2D1RenderTarget*, const D2D1_RECT_F*, int);
    void (STDMETHODCALLTYPE *PopAxisAlignedClip)(ID2D1RenderTarget*);
    void (STDMETHODCALLTYPE *Clear)(ID2D1RenderTarget*, const D2D1_COLOR_F*);
    void (STDMETHODCALLTYPE *BeginDraw)(ID2D1RenderTarget*);
    HRESULT (STDMETHODCALLTYPE *EndDraw)(ID2D1RenderTarget*, void*, void*);
    void *GetPixelFormat_RT, *SetDpi, *GetDpi, *GetSize, *GetPixelSize;
    void *GetMaximumBitmapSize, *IsSupported;
} ID2D1RenderTargetVtbl;
struct ID2D1RenderTarget { const ID2D1RenderTargetVtbl* lpVtbl; };

/* ID2D1BitmapRenderTarget — compatible offscreen render target */
typedef struct ID2D1BitmapRenderTarget ID2D1BitmapRenderTarget;
typedef struct {
    ID2D1RenderTargetVtbl Base;
    HRESULT (STDMETHODCALLTYPE *GetBitmap)(ID2D1BitmapRenderTarget*, ID2D1Bitmap**);
} ID2D1BitmapRenderTargetVtbl;
struct ID2D1BitmapRenderTarget { const ID2D1BitmapRenderTargetVtbl* lpVtbl; };

/* ID2D1HwndRenderTarget vtable (nested Base = RenderTarget + 3 methods) */
typedef struct {
    ID2D1RenderTargetVtbl Base;
    void* CheckWindowState;
    HRESULT (STDMETHODCALLTYPE *Resize)(ID2D1HwndRenderTarget*, const D2D1_SIZE_U*);
    void* GetHwnd;
} ID2D1HwndRenderTargetVtbl;
struct ID2D1HwndRenderTarget { const ID2D1HwndRenderTargetVtbl* lpVtbl; };

/* ID2D1DCRenderTarget — used by layered popup rendering. */
typedef struct {
    ID2D1RenderTargetVtbl Base;
    HRESULT (STDMETHODCALLTYPE *BindDC)(ID2D1DCRenderTarget*, const HDC, const RECT*);
} ID2D1DCRenderTargetVtbl;
struct ID2D1DCRenderTarget { const ID2D1DCRenderTargetVtbl* lpVtbl; };

/* ID2D1Factory vtable (flat: IUnknown[3] + Factory[14] = 17) */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ID2D1Factory*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(ID2D1Factory*);
    ULONG (STDMETHODCALLTYPE *Release)(ID2D1Factory*);
    void *ReloadSystemMetrics, *GetDesktopDpi;
    HRESULT (STDMETHODCALLTYPE *CreateRectangleGeometry)(ID2D1Factory*, const D2D1_RECT_F*, ID2D1RectangleGeometry**);
    HRESULT (STDMETHODCALLTYPE *CreateRoundedRectangleGeometry)(ID2D1Factory*, const D2D1_ROUNDED_RECT*, ID2D1RoundedRectangleGeometry**);
    void *CreateEllipseGeometry;
    HRESULT (STDMETHODCALLTYPE *CreateGeometryGroup)(ID2D1Factory*, int, ID2D1Geometry**, UINT32, ID2D1GeometryGroup**);
    void *CreateTransformedGeometry;
    HRESULT (STDMETHODCALLTYPE *CreatePathGeometry)(ID2D1Factory*, ID2D1PathGeometry**);
    void *CreateStrokeStyle, *CreateDrawingStateBlock, *CreateWicBitmapRenderTarget;
    HRESULT (STDMETHODCALLTYPE *CreateHwndRenderTarget)(ID2D1Factory*,
        const D2D1_RENDER_TARGET_PROPERTIES*, const D2D1_HWND_RENDER_TARGET_PROPERTIES*,
        ID2D1HwndRenderTarget**);
    void *CreateDxgiSurfaceRenderTarget;
    HRESULT (STDMETHODCALLTYPE *CreateDCRenderTarget)(ID2D1Factory*,
        const D2D1_RENDER_TARGET_PROPERTIES*, ID2D1DCRenderTarget**);
} ID2D1FactoryVtbl;
struct ID2D1Factory { const ID2D1FactoryVtbl* lpVtbl; };

/* DWrite hit-test metrics — used by IDWriteTextLayout::HitTest* methods.
 * Declared here (before the vtables) so the function-pointer signatures below
 * can reference it. */
typedef struct DWRITE_HIT_TEST_METRICS {
    UINT32 textPosition;
    UINT32 length;
    FLOAT  left;
    FLOAT  top;
    FLOAT  width;
    FLOAT  height;
    UINT32 bidiLevel;
    BOOL   isText;
    BOOL   isTrimmed;
} DWRITE_HIT_TEST_METRICS;

typedef enum DWRITE_TRIMMING_GRANULARITY {
    DWRITE_TRIMMING_GRANULARITY_NONE      = 0,
    DWRITE_TRIMMING_GRANULARITY_CHARACTER = 1,
    DWRITE_TRIMMING_GRANULARITY_WORD      = 2
} DWRITE_TRIMMING_GRANULARITY;

typedef struct DWRITE_TRIMMING {
    DWRITE_TRIMMING_GRANULARITY granularity;
    UINT32 delimiter;
    UINT32 delimiterCount;
} DWRITE_TRIMMING;

typedef struct IDWriteInlineObject IDWriteInlineObject;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteInlineObject*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteInlineObject*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteInlineObject*);
    void *Draw, *GetMetrics, *GetOverhangMetrics, *GetBreakConditions;
} IDWriteInlineObjectVtbl;
struct IDWriteInlineObject { const IDWriteInlineObjectVtbl* lpVtbl; };

/* IDWriteTextFormat vtable (flat: IUnknown[3] + TextFormat[25] = 28) */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteTextFormat*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteTextFormat*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteTextFormat*);
    HRESULT (STDMETHODCALLTYPE *SetTextAlignment)(IDWriteTextFormat*, int);
    HRESULT (STDMETHODCALLTYPE *SetParagraphAlignment)(IDWriteTextFormat*, int);
    HRESULT (STDMETHODCALLTYPE *SetWordWrapping)(IDWriteTextFormat*, int);
    void *SetReadingDirection, *SetFlowDirection, *SetIncrementalTabStop;
    HRESULT (STDMETHODCALLTYPE *SetTrimming)(IDWriteTextFormat*, const DWRITE_TRIMMING*, IDWriteInlineObject*);
    void *SetLineSpacing;
    void *GetTextAlignment, *GetParagraphAlignment, *GetWordWrapping;
    void *GetReadingDirection, *GetFlowDirection, *GetIncrementalTabStop;
    void *GetTrimming, *GetLineSpacing, *GetFontCollection;
    void *GetFontFamilyNameLength, *GetFontFamilyName;
    void *GetFontWeight, *GetFontStyle, *GetFontStretch, *GetFontSize;
    void *GetLocaleNameLength, *GetLocaleName;
} IDWriteTextFormatVtbl;
struct IDWriteTextFormat { const IDWriteTextFormatVtbl* lpVtbl; };

/* IDWriteTextLayout vtable (IUnknown[3] + TextFormat[25] + TextLayout[30] = 58) */
typedef struct IDWriteTextLayout IDWriteTextLayout;
typedef struct {
    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteTextLayout*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteTextLayout*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteTextLayout*);
    /* IDWriteTextFormat — 25 methods (inherited) */
    HRESULT (STDMETHODCALLTYPE *SetTextAlignment)(IDWriteTextLayout*, int);
    HRESULT (STDMETHODCALLTYPE *SetParagraphAlignment)(IDWriteTextLayout*, int);
    HRESULT (STDMETHODCALLTYPE *SetWordWrapping)(IDWriteTextLayout*, int);
    void *SetReadingDirection, *SetFlowDirection, *SetIncrementalTabStop;
    HRESULT (STDMETHODCALLTYPE *SetTrimming)(IDWriteTextLayout*, const DWRITE_TRIMMING*, IDWriteInlineObject*);
    void *SetLineSpacing;
    void *GetTextAlignment, *GetParagraphAlignment, *GetWordWrapping;
    void *GetReadingDirection, *GetFlowDirection, *GetIncrementalTabStop;
    void *GetTrimming, *GetLineSpacing, *GetFontCollection;
    void *GetFontFamilyNameLength, *GetFontFamilyName;
    void *GetFontWeight, *GetFontStyle, *GetFontStretch, *GetFontSize;
    void *GetLocaleNameLength, *GetLocaleName;
    /* IDWriteTextLayout — 30 methods */
    void *SetMaxWidth, *SetMaxHeight;
    void *SetFontCollection_TL, *SetFontFamilyName_TL, *SetFontWeight_TL;
    void *SetFontStyle_TL, *SetFontStretch_TL, *SetFontSize_TL;
    void *SetUnderline, *SetStrikethrough, *SetDrawingEffect;
    void *SetInlineObject, *SetTypography, *SetLocaleName_TL;
    void *GetMaxWidth, *GetMaxHeight;
    void *GetFontCollection_TL, *GetFontFamilyNameLength_TL, *GetFontFamilyName_TL;
    void *GetFontWeight_TL, *GetFontStyle_TL, *GetFontStretch_TL;
    void *GetFontSize_TL, *GetUnderline, *GetStrikethrough;
    void *GetDrawingEffect, *GetInlineObject, *GetTypography;
    void *GetLocaleNameLength_TL, *GetLocaleName_TL;
    /* Layout-specific queries */
    void *Draw;
    HRESULT (STDMETHODCALLTYPE *GetLineMetrics)(IDWriteTextLayout*, void*, UINT32, UINT32*);
    HRESULT (STDMETHODCALLTYPE *GetMetrics)(IDWriteTextLayout*, void*);
    void *GetOverhangMetrics, *GetClusterMetrics;
    void *DetermineMinWidth;
    HRESULT (STDMETHODCALLTYPE *HitTestPoint)(IDWriteTextLayout*,
        FLOAT pointX, FLOAT pointY,
        BOOL* isTrailingHit, BOOL* isInside,
        DWRITE_HIT_TEST_METRICS* hitTestMetrics);
    HRESULT (STDMETHODCALLTYPE *HitTestTextPosition)(IDWriteTextLayout*,
        UINT32 textPosition, BOOL isTrailingHit,
        FLOAT* pointX, FLOAT* pointY,
        DWRITE_HIT_TEST_METRICS* hitTestMetrics);
    HRESULT (STDMETHODCALLTYPE *HitTestTextRange)(IDWriteTextLayout*,
        UINT32 textPosition, UINT32 textLength,
        FLOAT originX, FLOAT originY,
        DWRITE_HIT_TEST_METRICS* hitTestMetrics,
        UINT32 maxHitTestMetricsCount,
        UINT32* actualHitTestMetricsCount);
} IDWriteTextLayoutVtbl;
struct IDWriteTextLayout { const IDWriteTextLayoutVtbl* lpVtbl; };

/* DWrite metrics struct for GetMetrics */
typedef struct DWRITE_TEXT_METRICS {
    float left, top, width, widthIncludingTrailingWhitespace;
    float height, layoutWidth, layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
} DWRITE_TEXT_METRICS;

/* IDWriteFactory vtable (flat: IUnknown[3] + Factory[21] = 24) */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteFactory*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteFactory*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteFactory*);
    void *GetSystemFontCollection, *CreateCustomFontCollection;
    void *RegisterFontCollectionLoader, *UnregisterFontCollectionLoader;
    void *CreateFontFileReference, *CreateCustomFontFileReference, *CreateFontFace;
    void *CreateRenderingParams, *CreateMonitorRenderingParams, *CreateCustomRenderingParams;
    void *RegisterFontFileLoader, *UnregisterFontFileLoader;
    HRESULT (STDMETHODCALLTYPE *CreateTextFormat)(IDWriteFactory*, const wchar_t*, void*,
        int, int, int, float, const wchar_t*, IDWriteTextFormat**);
    void *CreateTypography, *GetGdiInterop;
    HRESULT (STDMETHODCALLTYPE *CreateTextLayout)(IDWriteFactory*, const wchar_t*, UINT32,
        IDWriteTextFormat*, float, float, IDWriteTextLayout**);
    HRESULT (STDMETHODCALLTYPE *CreateGdiCompatibleTextLayout)(IDWriteFactory*, const wchar_t*, UINT32,
        IDWriteTextFormat*, float, float, float, const void*, BOOL, IDWriteTextLayout**);
    HRESULT (STDMETHODCALLTYPE *CreateEllipsisTrimmingSign)(IDWriteFactory*, IDWriteTextFormat*, IDWriteInlineObject**);
    void *CreateTextAnalyzer;
    void *CreateNumberSubstitution, *CreateGlyphRunAnalysis;
} IDWriteFactoryVtbl;
struct IDWriteFactory { const IDWriteFactoryVtbl* lpVtbl; };

/* GUIDs */
static const IID IID_ID2D1Factory =
    {0x06152247,0x6f50,0x465a,{0x92,0x45,0x11,0x8b,0xfd,0x3b,0x60,0x07}};
/* IDWriteRenderingParams2, as far as the toolkit reads it. */
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteRenderingParams*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteRenderingParams*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteRenderingParams*);
    void *GetGamma, *GetEnhancedContrast, *GetClearTypeLevel, *GetPixelGeometry;
    int (STDMETHODCALLTYPE *GetRenderingMode)(IDWriteRenderingParams*);
    void *GetGrayscaleEnhancedContrast;
    int (STDMETHODCALLTYPE *GetGridFitMode)(IDWriteRenderingParams*);
} IDWriteRenderingParamsVtbl;
struct IDWriteRenderingParams { const IDWriteRenderingParamsVtbl* lpVtbl; };

/* IDWriteFactory2, as far as the toolkit calls it (Windows 8.1): 26 methods of its bases and its own come before this one. */
typedef struct IDWriteFactory2 IDWriteFactory2;
typedef struct {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDWriteFactory2*, REFIID, void**);
    ULONG (STDMETHODCALLTYPE *AddRef)(IDWriteFactory2*);
    ULONG (STDMETHODCALLTYPE *Release)(IDWriteFactory2*);
    void *before[26];
    HRESULT (STDMETHODCALLTYPE *CreateCustomRenderingParams)(IDWriteFactory2*, float, float, float, float, int, int, int, IDWriteRenderingParams**);
} IDWriteFactory2Vtbl;
struct IDWriteFactory2 { const IDWriteFactory2Vtbl* lpVtbl; };
static const IID IID_IDWriteFactory2 =
    { 0x0439fc60, 0xca44, 0x4994, { 0x8d, 0xee, 0x3a, 0x9a, 0xf7, 0xb7, 0x32, 0xec } };

static const IID IID_IDWriteFactory =
    {0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};

/* Factory entry points (imported from d2d1.dll / dwrite.dll)
 * Wrapped in extern "C" so the linker finds the C-mangled exports when
 * sui_d2d.h is included from a C++ translation unit. */
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) HRESULT __stdcall D2D1CreateFactory(
    int factoryType, REFIID riid, const void* pOpts, void** ppFactory);
__declspec(dllimport) HRESULT __stdcall DWriteCreateFactory(
    int factoryType, REFIID iid, IUnknown** factory);
#ifdef __cplusplus
}
#endif
