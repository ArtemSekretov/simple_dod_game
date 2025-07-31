#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>

#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "d3dcompiler")

#ifndef __cplusplus
typedef struct DirectX11State            DirectX11State;
typedef struct VertexBuffer              VertexBuffer;
typedef struct RenderPipelineDescription RenderPipelineDescription;
#endif

struct VertexBuffer
{
    ID3D11Buffer* buffer;
    UINT stride;
    UINT offset;
};

#define kMaxConstantBufferSlotCounts (D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)

struct RenderPipelineDescription
{
    ID3D11InputLayout* layout;
    VertexBuffer vertex_buffer;

    ID3D11Buffer* constant_buffers[kMaxConstantBufferSlotCounts];
    u32 constant_buffers_count;

    ID3D11VertexShader* vshader;
    ID3D11PixelShader* pshader;
};

struct DirectX11State
{
    ID3D11Device* device;
    ID3D11DeviceContext* context;
	IDXGISwapChain1* swap_chain;

    ID3D11Buffer* objects_constant_buffer;

    RenderPipelineDescription main_pipeline;
    RenderPipelineDescription background_pipeline;

	ID3D11RenderTargetView* rt_view;
    ID3D11DepthStencilView* ds_view;

	ID3D11RasterizerState* rasterizer_state;
	ID3D11BlendState* blend_state;
	ID3D11DepthStencilState* depth_state;

	s32 current_width;
    s32 current_height;
};

static void
FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

static RenderPipelineDescription 
CreateBackgroundPipeline(ID3D11Device* device)
{
    HRESULT hr;

    struct Vertex
    {
        f32 position[2];
        f32 color[3];
    };

    ID3D11Buffer* vbuffer;
    {
		struct Vertex data[] =
        {
            { { -1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 0.0f, 0.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f, 1.0f } },
        };

        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(data),
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        };

        D3D11_SUBRESOURCE_DATA initial = { .pSysMem = data };
        ID3D11Device_CreateBuffer(device, &desc, &initial, &vbuffer);
    }

    // vertex & pixel shaders for drawing triangle, plus input layout for vertex input
    ID3D11InputLayout* layout;
    ID3D11VertexShader* vshader;
    ID3D11PixelShader* pshader;
    {
        // these must match vertex shader input layout (VS_INPUT in vertex shader source below)
        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(struct Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(struct Vertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

		#if 0
        // alternative to hlsl compilation at runtime is to precompile shaders offline
        // it improves startup time - no need to parse hlsl files at runtime!
        // and it allows to remove runtime dependency on d3dcompiler dll file

        // a) save shader source code into "shader.hlsl" file
        // b) run hlsl compiler to compile shader, these run compilation with optimizations and without debug info:
        //      fxc.exe /nologo /T vs_5_0 /E vs /O3 /WX /Zpc /Ges /Fh d3d11_vshader.h /Vn d3d11_vshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv shader.hlsl
        //      fxc.exe /nologo /T ps_5_0 /E ps /O3 /WX /Zpc /Ges /Fh d3d11_pshader.h /Vn d3d11_pshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv shader.hlsl
        //    they will save output to d3d11_vshader.h and d3d11_pshader.h files
        // c) change #if 0 above to #if 1

        // you can also use "/Fo d3d11_*shader.bin" argument to save compiled shader as binary file to store with your assets
        // then provide binary data for Create*Shader functions below without need to include shader bytes in C

        #include "d3d11_vshader.h"
        #include "d3d11_pshader.h"

        ID3D11Device_CreateVertexShader(device, d3d11_vshader, sizeof(d3d11_vshader), NULL, &vshader);
        ID3D11Device_CreatePixelShader(device, d3d11_pshader, sizeof(d3d11_pshader), NULL, &pshader);
        ID3D11Device_CreateInputLayout(device, desc, ARRAYSIZE(desc), d3d11_vshader, sizeof(d3d11_vshader), &layout);
		#else
        const char hlsl[] =
            "#line " STR(__LINE__) "                                  \n\n" // actual line number in this file for nicer error messages
            "                                                           \n"
            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float3 color      : COLOR;                               \n"
            "  uint   instanceID : SV_InstanceID;                       \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float4 color : COLOR;                                    \n"
            "};                                                         \n"
			"                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "    PS_INPUT output;                                       \n"
            "                                                           \n"
            "    output.pos = float4(input.pos, 0, 1);                  \n"
            "    output.color = float4(input.color, 1);                 \n"
            "    return output;                                         \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  return input.color;                                      \n"
            "}                                                          \n";
        ;
	
        UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
		#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		#endif

        ID3DBlob* error;

        ID3DBlob* vblob;
        hr = D3DCompile(hlsl, sizeof(hlsl), NULL, NULL, NULL, "vs", "vs_5_0", flags, 0, &vblob, &error);
        if (FAILED(hr))
        {
            const char* message = ID3D10Blob_GetBufferPointer(error);
            OutputDebugStringA(message);
            Assert(!"Failed to compile vertex shader!");
        }

        ID3DBlob* pblob;
        hr = D3DCompile(hlsl, sizeof(hlsl), NULL, NULL, NULL, "ps", "ps_5_0", flags, 0, &pblob, &error);
        if (FAILED(hr))
        {
            const char* message = ID3D10Blob_GetBufferPointer(error);
            OutputDebugStringA(message);
            Assert(!"Failed to compile pixel shader!");
        }

        ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vblob), ID3D10Blob_GetBufferSize(vblob), NULL, &vshader);
        ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(pblob), ID3D10Blob_GetBufferSize(pblob), NULL, &pshader);
        ID3D11Device_CreateInputLayout(device, desc, ARRAYSIZE(desc), ID3D10Blob_GetBufferPointer(vblob), ID3D10Blob_GetBufferSize(vblob), &layout);

        ID3D10Blob_Release(pblob);
        ID3D10Blob_Release(vblob);
		#endif
    }

    RenderPipelineDescription result = { 0 };
    result.layout  = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.constant_buffers_count = 0;
    return result;
}

static RenderPipelineDescription 
CreateMainPipeline(ID3D11Device* device, 
                   ID3D11Buffer* transform_constant_buffer, 
                   ID3D11Buffer* objects_constant_buffer)
{
    HRESULT hr;

    struct Vertex
    {
        f32 position[2];
        f32 uv[2];
    };

    ID3D11Buffer* vbuffer;
    {
		struct Vertex data[] =
        {
            { { -0.5f, -0.5f }, { 0.0f, 0.0f } },
            { { -0.5f, +0.5f }, { 0.0f, 1.0f } },
            { { +0.5f, +0.5f }, { 1.0f, 1.0f } },

            { { +0.5f, +0.5f }, { 1.0f, 1.0f } },
            { { +0.5f, -0.5f }, { 1.0f, 0.0f } },
            { { -0.5f, -0.5f }, { 0.0f, 0.0f } },
        };

        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(data),
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        };

        D3D11_SUBRESOURCE_DATA initial = { .pSysMem = data };
        ID3D11Device_CreateBuffer(device, &desc, &initial, &vbuffer);
    }

    // vertex & pixel shaders for drawing triangle, plus input layout for vertex input
    ID3D11InputLayout* layout;
    ID3D11VertexShader* vshader;
    ID3D11PixelShader* pshader;
    {
        // these must match vertex shader input layout (VS_INPUT in vertex shader source below)
        D3D11_INPUT_ELEMENT_DESC desc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(struct Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(struct Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

		#if 0
        // alternative to hlsl compilation at runtime is to precompile shaders offline
        // it improves startup time - no need to parse hlsl files at runtime!
        // and it allows to remove runtime dependency on d3dcompiler dll file

        // a) save shader source code into "shader.hlsl" file
        // b) run hlsl compiler to compile shader, these run compilation with optimizations and without debug info:
        //      fxc.exe /nologo /T vs_5_0 /E vs /O3 /WX /Zpc /Ges /Fh d3d11_vshader.h /Vn d3d11_vshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv shader.hlsl
        //      fxc.exe /nologo /T ps_5_0 /E ps /O3 /WX /Zpc /Ges /Fh d3d11_pshader.h /Vn d3d11_pshader /Qstrip_reflect /Qstrip_debug /Qstrip_priv shader.hlsl
        //    they will save output to d3d11_vshader.h and d3d11_pshader.h files
        // c) change #if 0 above to #if 1

        // you can also use "/Fo d3d11_*shader.bin" argument to save compiled shader as binary file to store with your assets
        // then provide binary data for Create*Shader functions below without need to include shader bytes in C

        #include "d3d11_vshader.h"
        #include "d3d11_pshader.h"

        ID3D11Device_CreateVertexShader(device, d3d11_vshader, sizeof(d3d11_vshader), NULL, &vshader);
        ID3D11Device_CreatePixelShader(device, d3d11_pshader, sizeof(d3d11_pshader), NULL, &pshader);
        ID3D11Device_CreateInputLayout(device, desc, ARRAYSIZE(desc), d3d11_vshader, sizeof(d3d11_vshader), &layout);
		#else
        const char hlsl[] =
            "#line " STR(__LINE__) "                                  \n\n" // actual line number in this file for nicer error messages
            "                                                           \n"
            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float2 uv         : TEXCOORD;                            \n"
            "  uint   instanceID : SV_InstanceID;                       \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "  float4 color : COLOR;                                    \n"
            "};                                                         \n"
			"cbuffer cbuffer0 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
            "{                                                          \n"
            "  float4x4 uTransform;                                     \n"
            "}                                                          \n"
			"                                                           \n"
			"struct ObjectData                                          \n"
            "{                                                          \n"
            "  float4 pos_and_scale;                                    \n" 
            "  float4 color;                                            \n"
            "};                                                         \n"
            "                                                           \n"
            "cbuffer cbuffer1 : register(b1)                            \n" // b1 = constant buffer bound to slot 1
            "{                                                          \n"
            "  ObjectData objects[" STR(kFrameDataMaxObjectDataCapacity) "];      \n"
            "}                                                          \n"
			"                                                           \n"
            "float sd_circle( float2 p, float r )                       \n"
            "{                                                          \n"
            "  return length(p) - r;                                    \n"
            "}                                                          \n"
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "    PS_INPUT output;                                       \n"
            "    ObjectData object_data = objects[input.instanceID];    \n"
            "    float2 position = input.pos * object_data.pos_and_scale.z; \n"
            "    position += object_data.pos_and_scale.xy;              \n"
            "                                                           \n"
            "    output.pos = mul(uTransform, float4(position, 0, 1));  \n"
            "    output.uv = input.uv;                                  \n"
            "    output.color = float4(object_data.color.xyz, 1);       \n"
            "    return output;                                         \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  float2 uv = input.uv * 2 - 1;                            \n"
			"  float dist = sd_circle(uv, 0.99);                        \n"
			"  float2 ddist = float2(ddx(dist), ddy(dist));             \n"
			"  float pixelDist = dist / length(ddist);                  \n"
			"  float alpha = saturate(0.5 - pixelDist);                 \n"
            "  return float4(input.color.xyz, alpha);                   \n"
            "}                                                          \n";
        ;
	
        UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
		#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
		#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		#endif

        ID3DBlob* error;

        ID3DBlob* vblob;
        hr = D3DCompile(hlsl, sizeof(hlsl), NULL, NULL, NULL, "vs", "vs_5_0", flags, 0, &vblob, &error);
        if (FAILED(hr))
        {
            const char* message = ID3D10Blob_GetBufferPointer(error);
            OutputDebugStringA(message);
            Assert(!"Failed to compile vertex shader!");
        }

        ID3DBlob* pblob;
        hr = D3DCompile(hlsl, sizeof(hlsl), NULL, NULL, NULL, "ps", "ps_5_0", flags, 0, &pblob, &error);
        if (FAILED(hr))
        {
            const char* message = ID3D10Blob_GetBufferPointer(error);
            OutputDebugStringA(message);
            Assert(!"Failed to compile pixel shader!");
        }

        ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vblob), ID3D10Blob_GetBufferSize(vblob), NULL, &vshader);
        ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(pblob), ID3D10Blob_GetBufferSize(pblob), NULL, &pshader);
        ID3D11Device_CreateInputLayout(device, desc, ARRAYSIZE(desc), ID3D10Blob_GetBufferPointer(vblob), ID3D10Blob_GetBufferSize(vblob), &layout);

        ID3D10Blob_Release(pblob);
        ID3D10Blob_Release(vblob);
		#endif
    }

    RenderPipelineDescription result = { 0 };
    result.layout  = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.constant_buffers_count = 2;
    result.constant_buffers[0] = transform_constant_buffer;
    result.constant_buffers[1] = objects_constant_buffer;
    return result;
}

static DirectX11State 
InitDirectX11(HWND window, m4x4 projection_martix)
{
	HRESULT hr;

    ID3D11Device* device;
    ID3D11DeviceContext* context;

    // create D3D11 device & context
    {
        UINT flags = 0;
		#ifndef NDEBUG
        // this enables VERY USEFUL debug messages in debugger output
        flags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        hr = D3D11CreateDevice(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &device, NULL, &context);
        // make sure device creation succeeeds before continuing
        // for simple applciation you could retry device creation with
        // D3D_DRIVER_TYPE_WARP driver type which enables software rendering
        // (could be useful on broken drivers or remote desktop situations)
        AssertHR(hr);
    }

	#ifndef NDEBUG
    // for debug builds enable VERY USEFUL debug break on API errors
    {
        ID3D11InfoQueue* info;
        ID3D11Device_QueryInterface(device, &IID_ID3D11InfoQueue, (void**)&info);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        ID3D11InfoQueue_Release(info);
    }

    // enable debug break for DXGI too
    {
        IDXGIInfoQueue* dxgiInfo;
        hr = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, (void**)&dxgiInfo);
        AssertHR(hr);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgiInfo, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgiInfo, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
        IDXGIInfoQueue_Release(dxgiInfo);
    }

    // after this there's no need to check for any errors on device functions manually
    // so all HRESULT return  values in this code will be ignored
    // debugger will break on errors anyway
	#endif

    // create DXGI swap chain
    IDXGISwapChain1* swapChain;
    {
        // get DXGI device from D3D11 device
        IDXGIDevice* dxgiDevice;
        hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void**)&dxgiDevice);
        AssertHR(hr);

        // get DXGI adapter from DXGI device
        IDXGIAdapter* dxgiAdapter;
        hr = IDXGIDevice_GetAdapter(dxgiDevice, &dxgiAdapter);
        AssertHR(hr);

        // get DXGI factory from DXGI adapter
        IDXGIFactory2* factory;
        hr = IDXGIAdapter_GetParent(dxgiAdapter, &IID_IDXGIFactory2, (void**)&factory);
        AssertHR(hr);

        DXGI_SWAP_CHAIN_DESC1 desc =
        {
            // default 0 value for width & height means to get it from HWND automatically
            //.Width = 0,
            //.Height = 0,

            // or use DXGI_FORMAT_R8G8B8A8_UNORM_SRGB for storing sRGB
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,

            // FLIP presentation model does not allow MSAA framebuffer
            // if you want MSAA then you'll need to render offscreen and manually
            // resolve to non-MSAA framebuffer
            .SampleDesc = { 1, 0 },

            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = 2,

            // we don't want any automatic scaling of window content
            // this is supported only on FLIP presentation model
            .Scaling = DXGI_SCALING_NONE,

            // use more efficient FLIP presentation model
            // Windows 10 allows to use DXGI_SWAP_EFFECT_FLIP_DISCARD
            // for Windows 8 compatibility use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
            // for Windows 7 compatibility use DXGI_SWAP_EFFECT_DISCARD
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        };

        hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown*)device, window, &desc, NULL, NULL, &swapChain);
        AssertHR(hr);

        // disable silly Alt+Enter changing monitor resolution to match window size
        IDXGIFactory_MakeWindowAssociation(factory, window, DXGI_MWA_NO_ALT_ENTER);

        IDXGIFactory2_Release(factory);
        IDXGIAdapter_Release(dxgiAdapter);
        IDXGIDevice_Release(dxgiDevice);
    }
	
    ID3D11Buffer* transform_constant_buffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            // space for 4x4 float matrix (cbuffer0 from pixel shader)
            .ByteWidth = sizeof(m4x4),
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        D3D11_SUBRESOURCE_DATA initial = { .pSysMem = &projection_martix };
        ID3D11Device_CreateBuffer(device, &desc, &initial, &transform_constant_buffer);
    }

    ID3D11Buffer* objectBuffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(FrameDataFrameDataObjectData) * kFrameDataMaxObjectDataCapacity,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(device, &desc, NULL, &objectBuffer);
    }

    ID3D11BlendState* blendState;
    {
        // enable alpha blending
        D3D11_BLEND_DESC desc =
        {
            .RenderTarget[0] =
            {
                .BlendEnable = TRUE,
                .SrcBlend = D3D11_BLEND_SRC_ALPHA,
                .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
                .BlendOp = D3D11_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA,
                .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
                .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            },
        };
        ID3D11Device_CreateBlendState(device, &desc, &blendState);
    }

    ID3D11RasterizerState* rasterizerState;
    {
        // disable culling
        D3D11_RASTERIZER_DESC desc =
        {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
            .DepthClipEnable = TRUE,
        };
        ID3D11Device_CreateRasterizerState(device, &desc, &rasterizerState);
    }

    ID3D11DepthStencilState* depthState;
    {
        // disable depth & stencil test
        D3D11_DEPTH_STENCIL_DESC desc =
        {
            .DepthEnable = FALSE,
            .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D11_COMPARISON_LESS,
            .StencilEnable = FALSE,
            .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
            // .FrontFace = ... 
            // .BackFace = ...
        };
        ID3D11Device_CreateDepthStencilState(device, &desc, &depthState);
    }

    RenderPipelineDescription main_render_pipeline = CreateMainPipeline(device, transform_constant_buffer, objectBuffer);
    RenderPipelineDescription backgound_pipeline   = CreateBackgroundPipeline(device);

	DirectX11State state = { 0 };
	state.device = device;
	state.context = context;
	state.swap_chain = swapChain;

    state.main_pipeline       = main_render_pipeline;
    state.background_pipeline = backgound_pipeline;

	state.objects_constant_buffer = objectBuffer;

	state.rasterizer_state = rasterizerState;
	state.blend_state = blendState;
	state.depth_state = depthState;

	return state;
}

static void
BindPineline(ID3D11DeviceContext* context, RenderPipelineDescription *pipeline)
{
    // Input Assembler
    ID3D11DeviceContext_IASetInputLayout(context, pipeline->layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = pipeline->vertex_buffer.stride;
    UINT offset = pipeline->vertex_buffer.offset;
    ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &pipeline->vertex_buffer.buffer, &stride, &offset);

    // Vertex Shader
    for (u8 i = 0; i < pipeline->constant_buffers_count; i++)
    {
        ID3D11DeviceContext_VSSetConstantBuffers(context, i, 1, &pipeline->constant_buffers[i]);
    }

    ID3D11DeviceContext_VSSetShader(context, pipeline->vshader, NULL, 0);

    // Pixel Shader
    ID3D11DeviceContext_PSSetShader(context, pipeline->pshader, NULL, 0);
}

static void 
EndFrameDirectX11(DirectX11State *directx_state, FrameData *frame_data)
{
	HRESULT hr;

    s32 width  = *FrameDataWidthPrt(frame_data);
    s32 height = *FrameDataHeightPrt(frame_data);

	// resize swap chain if needed
	if (directx_state->rt_view == NULL || width != directx_state->current_width || height != directx_state->current_height)
	{
		if (directx_state->rt_view)
		{
			// release old swap chain buffers
			ID3D11DeviceContext_ClearState(directx_state->context);
			ID3D11RenderTargetView_Release(directx_state->rt_view);
			ID3D11DepthStencilView_Release(directx_state->ds_view);
			directx_state->rt_view = NULL;
		}

		// resize to new size for non-zero size
		if (width != 0 && height != 0)
		{
			hr = IDXGISwapChain1_ResizeBuffers(directx_state->swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
			if (FAILED(hr))
			{
				FatalError("Failed to resize swap chain!");
			}

			// create RenderTarget view for new backbuffer texture
			ID3D11Texture2D* back_buffer;
			IDXGISwapChain1_GetBuffer(directx_state->swap_chain, 0, &IID_ID3D11Texture2D, (void**)&back_buffer);
			ID3D11Device_CreateRenderTargetView(directx_state->device, (ID3D11Resource*)back_buffer, NULL, &directx_state->rt_view);
			ID3D11Texture2D_Release(back_buffer);

			D3D11_TEXTURE2D_DESC depthDesc =
			{
				.Width = width,
				.Height = height,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = DXGI_FORMAT_D32_FLOAT, // or use DXGI_FORMAT_D32_FLOAT_S8X24_UINT if you need stencil
				.SampleDesc = { 1, 0 },
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_DEPTH_STENCIL,
			};

			// create new depth stencil texture & DepthStencil view
			ID3D11Texture2D* depth;
			ID3D11Device_CreateTexture2D(directx_state->device, &depthDesc, NULL, &depth);
			ID3D11Device_CreateDepthStencilView(directx_state->device, (ID3D11Resource*)depth, NULL, &directx_state->ds_view);
			ID3D11Texture2D_Release(depth);
		}

		directx_state->current_width  = width;
		directx_state->current_height = height;
	}

	// can render only if window size is non-zero - we must have backbuffer & RenderTarget view created
	if (directx_state->rt_view)
	{
        f32 viewport_x  = *FrameDataViewportXPrt(frame_data);
        f32 viewport_y = *FrameDataViewportYPrt(frame_data);
        f32 viewport_width = *FrameDataViewportWidthPrt(frame_data);
        f32 viewport_height = *FrameDataViewportHeightPrt(frame_data);
        u16 frame_data_count = *FrameDataFrameDataCountPrt(frame_data);

		// output viewport covering all client area of window
		D3D11_VIEWPORT viewport =
		{
			.TopLeftX = viewport_x,
			.TopLeftY = viewport_y,
			.Width = viewport_width,
			.Height = viewport_height,
			.MinDepth = 0,
			.MaxDepth = 1,
		};

		// clear screen
		FLOAT color[] = { 0.392f, 0.584f, 0.929f, 1.f };
		ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->rt_view, color);
		ID3D11DeviceContext_ClearDepthStencilView(directx_state->context, directx_state->ds_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

		// setup object data in uniform		
        D3D11_MAPPED_SUBRESOURCE mapped;
		ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->objects_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		
        FrameDataFrameData* frame_data_sheet = FrameDataFrameDataPrt(frame_data);
        FrameDataFrameDataObjectData* object_data = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

        memcpy(mapped.pData, object_data, sizeof(FrameDataFrameDataObjectData) * kFrameDataMaxObjectDataCapacity);
		ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->objects_constant_buffer, 0);
            
        // Rasterizer Stage
		ID3D11DeviceContext_RSSetViewports(directx_state->context, 1, &viewport);
		ID3D11DeviceContext_RSSetState(directx_state->context, directx_state->rasterizer_state);

        // Output Merger
		ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->blend_state, NULL, ~0U);
		ID3D11DeviceContext_OMSetDepthStencilState(directx_state->context, directx_state->depth_state, 0);
		ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->rt_view, directx_state->ds_view);
        {
            BindPineline(directx_state->context, &directx_state->background_pipeline);

            // Draw objects with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);
        }

        {
            BindPineline(directx_state->context, &directx_state->main_pipeline);

            // Draw objects with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, frame_data_count, 0, 0);
        }
			
	}

	// change to FALSE to disable vsync
	BOOL vsync = TRUE;
	hr = IDXGISwapChain1_Present(directx_state->swap_chain, vsync ? 1 : 0, 0);
	if (hr == DXGI_STATUS_OCCLUDED)
	{
		// window is minimized, cannot vsync - instead sleep a bit
		if (vsync)
		{
			Sleep(10);
		}
	}
	else if (FAILED(hr))
	{
		FatalError("Failed to present swap chain! Device lost?");
	}
}