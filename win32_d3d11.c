#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>

#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "d3dcompiler")

#ifndef __cplusplus
typedef struct DirectX11State DirectX11State;
typedef struct VertexBuffer VertexBuffer;
typedef struct RenderPipelineDescription RenderPipelineDescription;
typedef struct PixelTextureResource PixelTextureResource;
typedef struct ObjectBufferHeader ObjectBufferHeader;
typedef struct RadianceData RadianceData;
typedef struct JumpFloodData JumpFloodData;
typedef struct RenderSizeData RenderSizeData;
#endif

struct ObjectBufferHeader
{
    u32 frame_object_count;
    u32 padding[3];
};

struct VertexBuffer
{
    ID3D11Buffer *buffer;
    UINT stride;
    UINT offset;
};

struct PixelTextureResource
{
    ID3D11SamplerState *sampler;
    ID3D11ShaderResourceView *texture_view;
};

#define kMaxConstantBufferSlotCounts (D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)
#define kMaxPixelTextureResourceSlotCounts (D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)

#define kMaxRadianceCascades 10

struct JumpFloodData
{
    f32 jump_distance;
    f32 padding[3];
};

struct RenderSizeData
{
    v2 size;
    f32 padding[2];
};

struct RadianceData                                      
{                                                        
    v2 render_size;                                      
    v2 radiance_size;                                    
    u32 cascade_count;                                       
    u32 cascade_index;                                       
    f32 radiance_linear;                                     
    f32 radiance_interval;                                   
};

struct RenderPipelineDescription
{
    ID3D11InputLayout *layout;
    VertexBuffer vertex_buffer;

    ID3D11Buffer *vertex_constant_buffers[kMaxConstantBufferSlotCounts];
    u32 vertex_constant_buffers_count;

    ID3D11Buffer *pixel_constant_buffers[kMaxConstantBufferSlotCounts];
    u32 pixel_constant_buffers_count;

    ID3D11VertexShader *vshader;

    PixelTextureResource pixel_texture_resources[kMaxPixelTextureResourceSlotCounts];
    u32 pixel_texture_resource_count;

    ID3D11PixelShader *pshader;
};

struct DirectX11State
{
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    IDXGISwapChain1 *swap_chain;

    ID3D11Buffer *objects_constant_buffer;

    ID3D11Buffer *radiance_constant_buffer[kMaxRadianceCascades];
    u8 radiance_constant_buffer_count;

    ID3D11Buffer *jump_flood_constant_buffer;
    u8 jump_flood_passes;

    ID3D11Buffer *render_size_constant_buffer;

    RenderPipelineDescription main_pipeline;
    
    RenderPipelineDescription sdf_pipeline;

    RenderPipelineDescription seed_jump_flood_pipeline;
    RenderPipelineDescription jump_flood_pipeline;

    RenderPipelineDescription radiance_pipeline;

    RenderPipelineDescription final_blit_pipeline;

    ID3D11RenderTargetView *backbuffer_rt_view;
    ID3D11DepthStencilView *ds_view;

    ID3D11RenderTargetView *radiance_current_rt_view;
    ID3D11ShaderResourceView *radiance_current_view;

    ID3D11RenderTargetView *radiance_previous_rt_view;
    ID3D11ShaderResourceView *radiance_previous_view;

    ID3D11RenderTargetView *jf_rt_view;
    ID3D11ShaderResourceView *jf_view;

    ID3D11RenderTargetView *sdf_rt_view;
    ID3D11ShaderResourceView *sdf_view;

    ID3D11RenderTargetView *game_rt_view;
    ID3D11ShaderResourceView *game_view;

    ID3D11RasterizerState *rasterizer_state;
    ID3D11BlendState *blend_state;
    ID3D11BlendState *no_blend_state;
    ID3D11DepthStencilState *depth_state;

    ID3D11SamplerState *linear_clamp_sampler;
    ID3D11SamplerState *linear_wrap_sampler;
    ID3D11SamplerState *point_sampler;

    s32 current_screen_width;
    s32 current_screen_height;

    s32 render_width;
    s32 render_height;

    s32 radiance_width;
    s32 radiance_height;
};

static RenderPipelineDescription
CreateSeedJumpFloodPipeline(ID3D11Device* device)
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "#define F16V2(f) float2(floor(f * 255.0) * float(0.0039215689), frac(f * 255.0)) \n"

            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float2 uv         : TEXCOORD;                            \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "};                                                         \n"
            "sampler game_sampler : register(s0);                       \n" // s0 = sampler bound to slot 0
            "                                                           \n"
            "Texture2D<float4> game_texture : register(t0);             \n" // t0 = shader resource bound to slot 0
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "  PS_INPUT output;                                         \n"
            "                                                           \n"
            "  output.pos = float4(input.pos, 0, 1);                    \n"
            "  output.uv = input.uv;                                    \n"
            "  return output;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  float2 uv = float2(input.uv.x, 1.0 - input.uv.y);        \n"
            "                                                           \n"
            "  float4 scene = game_texture.Sample(game_sampler, uv);    \n"
            "                                                           \n"
            "  float2 u = F16V2(uv.x * scene.a);                        \n"
            "  float2 v = F16V2(uv.y * scene.a);                        \n"
            "                                                           \n"
            "  return float4(u.x, u.y, v.x, v.y);                       \n"
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 0;
    result.pixel_texture_resource_count = 0;
    result.pixel_constant_buffers_count = 0;
    return result;
}

static RenderPipelineDescription
CreateJumpFloodPipeline(ID3D11Device* device,
                        ID3D11Buffer* render_size_constant_buffer,
                        ID3D11Buffer* jump_flood_constant_buffer)
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "#define F16V2(f) float2(floor(f * 255.0) * 0.0039215689, frac(f * 255.0)) \n"
            "#define V2F16(v) ((v.y * 0.0039215689) + v.x)              \n"
            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float2 uv         : TEXCOORD;                            \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "};                                                         \n"
            "sampler jf_sampler : register(s0);                         \n" // s0 = sampler bound to slot 0
            "                                                           \n"
            "Texture2D<float4> jf_texture : register(t0);               \n" // t0 = shader resource bound to slot 0
            "                                                           \n"
            "struct RenderSizeData                                      \n"
            "{                                                          \n"
            "  float2 size;                                             \n"
            "  float padding0;                                          \n"
            "  float padding1;                                          \n"
            "};                                                         \n"
            "struct JumpFloodData                                       \n"
            "{                                                          \n"
            "  float jump_distance;                                     \n"
            "  float padding0;                                          \n"
            "  float padding1;                                          \n"
            "  float padding2;                                          \n"
            "};                                                         \n"
            "                                                           \n"
            "cbuffer cbuffer1 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
            "{                                                          \n"
            "  RenderSizeData render_size_input;                        \n"
            "}                                                          \n"
            "cbuffer cbuffer2 : register(b1)                            \n" // b1 = constant buffer bound to slot 1
            "{                                                          \n"
            "  JumpFloodData jf_input;                                  \n"
            "}                                                          \n"
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "  PS_INPUT output;                                         \n"
            "                                                           \n"
            "  output.pos = float4(input.pos, 0, 1);                    \n"
            "  output.uv = input.uv;                                    \n"
            "  return output;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  float2 uv = float2(input.uv.x, 1.0 - input.uv.y);        \n"
            "                                                           \n"
            "  float2 offsets[9];                                       \n"
            "  offsets[0] = float2(-1.0, -1.0);                         \n"
            "  offsets[1] = float2(-1.0, 0.0);                          \n"
            "  offsets[2] = float2(-1.0, 1.0);                          \n"
            "  offsets[3] = float2(0.0, -1.0);                          \n"
            "  offsets[4] = float2(0.0, 0.0);                           \n"
            "  offsets[5] = float2(0.0, 1.0);                           \n"
            "  offsets[6] = float2(1.0, -1.0);                          \n"
            "  offsets[7] = float2(1.0, 0.0);                           \n"
            "  offsets[8] = float2(1.0, 1.0);                           \n"
            "                                                           \n"
            "  float closest_dist = 1e10;                               \n"
            "  float4 closest_data = float4(0.0, 0.0, 0.0, 0.0);        \n"
            "                                                           \n"
            "  float2 distance_offset = float2(jf_input.jump_distance / render_size_input.size.x, jf_input.jump_distance / render_size_input.size.y); \n"
            "                                                           \n"
            "  [unroll]                                                 \n"
            "  for(uint i = 0; i < 9; i++)                              \n"
            "  {                                                        \n"
            "     float2 jump = uv + (offsets[i] * distance_offset);    \n"
            "     float4 seed = jf_texture.Sample(jf_sampler, jump);    \n"
            "                                                           \n"
            "     float2 seedpos = float2(V2F16(seed.xy), V2F16(seed.zw)); \n"
            "                                                           \n"
            "     float dist = distance(seedpos * render_size_input.size, uv * render_size_input.size); \n"
            "                                                           \n"
            "     if (seedpos.x != 0.0 && seedpos.y != 0.0 && dist <= closest_dist) \n"
            "     {                                                     \n"
            "        closest_dist = dist;                               \n"
            "        closest_data = seed;                               \n"
            "     }                                                     \n"
            "  }                                                        \n"
            "  return closest_data;                                     \n"
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 0;
    result.pixel_texture_resource_count = 0;
    result.pixel_constant_buffers_count = 2;
    result.pixel_constant_buffers[0] = render_size_constant_buffer;
    result.pixel_constant_buffers[1] = jump_flood_constant_buffer;
    return result;
}

static RenderPipelineDescription
CreateRadiancePipeline(ID3D11Device* device)
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "#define EPS 0.00010                                        \n"
            "#define TAU 6.283185                                       \n"
            "#define V2F16(v) ((v.y * 0.0039215689) + v.x)              \n"
            "                                                           \n"
            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float2 uv         : TEXCOORD;                            \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "};                                                         \n"
            "                                                           \n"
            "sampler sdf_sampler : register(s0);                        \n" // s0 = sampler bound to slot 0
            "                                                           \n"
            "Texture2D<float4> sdf_texture : register(t0);              \n" // t0 = shader resource bound to slot 0
            "                                                           \n"
            "sampler radiance_sampler : register(s1);                   \n" // s1 = sampler bound to slot 1
            "                                                           \n"
            "Texture2D<float4> radiance_texture : register(t1);         \n" // t1 = shader resource bound to slot 1
            "                                                           \n"
            "sampler game_sampler : register(s2);                       \n" // s2 = sampler bound to slot 2
            "                                                           \n"
            "Texture2D<float4> game_texture : register(t2);             \n" // t2 = shader resource bound to slot 2
            "                                                           \n"
            "struct RadianceData                                        \n"
            "{                                                          \n"
            "  float2 render_size;                                      \n"
            "  float2 radiance_size;                                    \n"
            "  uint cascade_count;                                      \n"
            "  uint cascade_index;                                      \n"
            "  float radiance_linear;                                   \n"
            "  float radiance_interval;                                 \n"
            "};                                                         \n"
            "                                                           \n"
            "struct ProbeInfo                                           \n" // Information struct about scene probes within the cascade.
            "{                                                          \n"
            "  float angular;                                           \n"
            "  float2 linear_spacing;                                   \n"
            "  float2 size;                                             \n"
            "  float2 probe;                                            \n"
            "  float index;                                             \n"
            "  float offset;                                            \n"
            "  float range;                                             \n"
            "  float scale;                                             \n"
            "};                                                         \n"
            "                                                           \n"
            "cbuffer cbuffer1 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
            "{                                                          \n"
            "  RadianceData radiance_input;                             \n"
            "}                                                          \n"
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "    PS_INPUT output;                                       \n"
            "                                                           \n"
            "    output.pos = float4(input.pos, 0, 1);                  \n"
            "    output.uv = input.uv;                                  \n"
            "    return output;                                         \n"
            "}                                                          \n"
            "                                                           \n"
            "ProbeInfo cascade_texel_info(float2 uv)                    \n"
            "{                                                          \n"
            "  float2 coord = floor(uv * radiance_input.radiance_size); \n"
            "                                                           \n"
            "  float angular = pow(2.0, radiance_input.cascade_index);  \n" // Ray Count.
            "  float2 radiance_linear = float2(radiance_input.radiance_linear * angular, radiance_input.radiance_linear * angular); \n" // Cascade Probe Spacing.
            "  float2 size = radiance_input.radiance_size / angular;    \n" // Size of Probe-Group.
            "  float2 probe = fmod(floor(coord), size);                 \n" // Probe-Group Index.
            "  float2 raypos = floor(uv * angular);                     \n" //	* spatial-xy ray-index position.
            "  float index = raypos.x + (angular * raypos.y);           \n" // PreAvg Index (actual = index * 4).
            "  float offset = (radiance_input.radiance_interval * (1.0 - pow(4.0, radiance_input.cascade_index))) / (1.0 - 4.0);\n" // Offset of Ray Interval (geometric sum).
            "  float range = radiance_input.radiance_interval * pow(4.0, radiance_input.cascade_index); \n" // Length of Ray Interval (geometric sum).
            "  float range_compensation = radiance_input.radiance_linear * pow(2.0, radiance_input.cascade_index + 1.0); \n" //	* light Leak Fix.
            "  range += length(float2(range_compensation, range_compensation)); \n" //	* light Leak Fix.
            "  float scale = length(radiance_input.render_size);        \n" // Diagonal of Render Extent (for SDF scaling).
            "                                                           \n"
            "  ProbeInfo result;                                        \n"
            "  result.angular = angular * angular;                      \n"
            "  result.linear_spacing = radiance_linear;                 \n"
            "  result.size = size;                                      \n"
            "  result.probe = probe;                                    \n"
            "  result.index = index;                                    \n"
            "  result.offset = offset;                                  \n"
            "  result.range = range;                                    \n"
            "  result.scale = scale;                                    \n"
            "                                                           \n"
            "  return result;                                           \n" // Output probe information struct.
            "}                                                          \n"
            "                                                           \n"
            "float4 raymarch(float2 origin, float theta, ProbeInfo info)\n"
            "{                                                          \n"
            "  float2 texel = 1.0 / radiance_input.render_size;	        \n" // Scalar for converting pixel-coordinates back to screen-space UV.
            "  float2 delta = float2(cos(theta), -sin(theta));	        \n" // Ray component to move in the direction of theta.
            "                                                           \n"
            "  float2 ray = (origin + (delta * info.offset)) * texel;   \n" // Ray origin at interval offset starting point.
            "                                                           \n"
            "  float4 result = float4(0.0, 0.0, 0.0, 1.0);              \n"
            "  [loop]                                                   \n"
            "  for(float i = 0.0, rd = 0.0; i < info.range; i++)        \n"
            "  {                                                        \n"
            "    float sdf_data = V2F16(sdf_texture.SampleLevel(sdf_sampler, ray, 0).xy); \n"
            "                                                           \n"
            "    rd += sdf_data * info.scale;                           \n"
            "    ray += (delta * sdf_data * info.scale * texel);        \n"
            "                                                           \n"
            "    float2 floor_ray = floor(ray);                         \n"
            "    if (rd >= info.range || (floor_ray.x != 0.0 && floor_ray.y != 0.0)) \n" // If ray has reached range or out-of-bounds, return no-hit.
            "    {                                                      \n"
            "      break;                                               \n"
            "    }                                                      \n"
            "                                                           \n"
            "    if (sdf_data <= EPS && rd <= EPS && radiance_input.cascade_index != 0) \n" // 2D light only cast light at their surfaces, not their volume.
            "    {                                                      \n"
            "      result = float4(0.0, 0.0, 0.0, 0.0);                 \n"
            "      break;                                               \n"
            "    }                                                      \n"
            "                                                           \n"
            "    if (sdf_data <= EPS)                                   \n" // On-hit return radiance from scene (with visibility term of 0--e.g. no visibility to merge with higher cascades).
            "    {                                                      \n"
            "      float4 scene = game_texture.SampleLevel(game_sampler, ray, 0); \n"
            "      result = float4(scene.x, scene.y, scene.z, 0.0);     \n"
            "      break;                                               \n"
            "    }                                                      \n"
            "  }                                                        \n"
            "  return result;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 merge(float4 rinfo, float index, ProbeInfo info)    \n"
            "{                                                          \n"
            "  float4 result;                                           \n"
            "                                                           \n"
            "  if (rinfo.a == 0.0 || radiance_input.cascade_index >= radiance_input.cascade_count - 1) \n" // For any radiance with zero-alpha do not merge (highest cascade also cannot merge).
            "  {                                                        \n"
            "    result = float4(rinfo.rgb, 1.0 - rinfo.a);             \n"  // Return non-merged radiance (invert alpha to correct alpha from raymarch ray-visibility term).
            "  }                                                        \n"
            "  else                                                     \n"
            "  {                                                        \n"
            "    float angularN1 = pow(2.0, radiance_input.cascade_index + 1.0); \n" // Angular resolution of cascade N+1 for probe lookups.
            "    float2 sizeN1 = info.size * 0.5;                       \n" // Size of probe group of cascade N+1 (N+1 has 1/4 total probe count or 1/2 each x,y axis).
            "    float2 probeN1 = float2(fmod(index, angularN1), floor(index / angularN1)) * sizeN1; \n" // Get the probe group correlated to the ray index passed of the current cascade ray we're merging with.
            "    float2 interpUVN1 = (info.probe * 0.5) + 0.25;         \n" // Interpolated probe position in cascade N+1 (layouts match but with 1/2 count, probe falls into its interpolated position by default).
            "    float2 clampedUVN1 = max(float2(1.0, 1.0), min(interpUVN1, sizeN1 - float2(1.0, 1.0))); \n" // Clamp interpolated probe position away from edge to avoid hardware inteprolation affecting merge lookups from adjacet probe groups.
            "    float2 probeUVN1 = probeN1 + clampedUVN1;              \n" // Final lookup cascade position of the interpolated merge lookup.
            "    float2 probeUV = probeUVN1 * (1.0 / radiance_input.radiance_size); \n"
            "                                                           \n"
            "    float4 interpolated = radiance_texture.Sample(radiance_sampler, probeUV); \n" // Texture lookup of the merge sample.
            "                                                           \n"
            "    result = rinfo + interpolated;                         \n"
            "    result.a = 1;                                          \n"
            "  }                                                        \n"
            "  return result;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "                                                           \n"
            "  float2 uv = float2(input.uv.x, 1.0 - input.uv.y);        \n"
            "  ProbeInfo pinfo = cascade_texel_info(uv);                \n"
            "                                                           \n"
            "  float2 origin = (pinfo.probe + 0.5f) * pinfo.linear_spacing;\n" // Get this probes position in screen space.
            "  float preavg_index = pinfo.index * 4.0;                  \n" // Convert this probe's pre-averaged index to its actual angular index (casting 4x rays, but storing 1x averaged).
            "  float theta_scalar = TAU / (pinfo.angular * 4.0);        \n" // Get the scalar for converting our angular index to radians (0 to 2pi).
            "                                                           \n"
            "  float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);          \n"
            "                                                           \n"
            "  [unroll]                                                 \n"
            "  for(uint i = 0; i < 4; i++)                              \n" // Cast 4 rays, one for each angular index for this pre-averaged ray.
            "  {                                                        \n"
            "    float index = preavg_index + float(i);                 \n" // Get the actual index for this pre-averaged ray.
            "    float theta = (index + 0.5) * theta_scalar;            \n" // Get the actual angle (theta) for this pre-averaged ray.
            "                                                           \n"
            "    float4 rinfo = raymarch(origin, theta, pinfo);         \n"
            "                                                           \n"
            "    float4 merge_result = merge(rinfo, index, pinfo);      \n"
            "                                                           \n"
            "    result += merge_result * 0.25f;                        \n"
            "  }                                                        \n"
            "                                                           \n"
            "  return result;                                           \n"
            "}                                                          \n";
	
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 0;
    result.pixel_texture_resource_count = 0;
    result.pixel_constant_buffers_count = 0;
    return result;
}

static RenderPipelineDescription
CreateSDFPipeline(ID3D11Device* device,
                  ID3D11Buffer* render_size_constant_buffer)
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "#define F16V2(f) float2(floor(f * 255.0) * float(0.0039215689), frac(f * 255.0)) \n"
            "#define V2F16(v) ((v.y * float(0.0039215689)) + v.x)              \n"
            "struct VS_INPUT                                            \n"
            "{                                                          \n"
            "  float2 pos        : POSITION;                            \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
            "  float2 uv         : TEXCOORD;                            \n"
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "};                                                         \n"
            "                                                           \n"
            "sampler jf_sampler : register(s0);                         \n" // s0 = sampler bound to slot 0
            "                                                           \n"
            "Texture2D<float4> jf_texture : register(t0);               \n" // t0 = shader resource bound to slot 0
            "                                                           \n"
            "struct RenderSizeData                                      \n"
            "{                                                          \n"
            "  float2 size;                                             \n"
            "  float padding0;                                          \n"
            "  float padding1;                                          \n"
            "};                                                         \n"
            "                                                           \n"
            "cbuffer cbuffer1 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
            "{                                                          \n"
            "  RenderSizeData render_size_input;                        \n"
            "}                                                          \n"
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "  PS_INPUT output;                                         \n"
            "                                                           \n"
            "  output.pos = float4(input.pos, 0, 1);                    \n"
            "  output.uv = input.uv;                                    \n"
            "  return output;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  float2 uv = float2(input.uv.x, 1.0 - input.uv.y);        \n"
            "                                                           \n"
            "  float4 jfuv = jf_texture.Sample(jf_sampler, uv);         \n"
            "                                                           \n"
            "  float2 jumpflood = float2(V2F16(jfuv.xy),V2F16(jfuv.zw));\n"
            "  float dist = distance(uv * render_size_input.size, jumpflood * render_size_input.size); \n"
            "                                                           \n"
            "  float2 d = F16V2(dist / length(render_size_input.size)); \n"
            "                                                           \n"
            "  return float4(d.x, d.y, 0.0, 1.0);                       \n"
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 0;
    result.pixel_texture_resource_count = 0;
    result.pixel_constant_buffers_count = 1;
    result.pixel_constant_buffers[0] = render_size_constant_buffer;
    return result;
}

static RenderPipelineDescription
CreateFinalBlitPipeline(ID3D11Device* device)
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "};                                                         \n"
            "                                                           \n"
            "struct PS_INPUT                                            \n"
            "{                                                          \n"
            "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
            "  float2 uv    : TEXCOORD;                                 \n"
            "};                                                         \n"
            "                                                           \n"
            "sampler texture_sampler : register(s0);                    \n" // s0 = sampler bound to slot 0
            "                                                           \n"
            "Texture2D<float4> main_texture : register(t0);             \n" // t0 = shader resource bound to slot 0
            "                                                           \n"
            "PS_INPUT vs(VS_INPUT input)                                \n"
            "{                                                          \n"
            "  PS_INPUT output;                                         \n"
            "                                                           \n"
            "  output.pos = float4(input.pos, 0, 1);                    \n"
            "  output.uv = input.uv;                                    \n"
            "  return output;                                           \n"
            "}                                                          \n"
            "                                                           \n"
            "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
            "{                                                          \n"
            "  float2 uv = float2(input.uv.x, 1.0 - input.uv.y);        \n"
            "                                                           \n"
            "  float4 color = main_texture.Sample(texture_sampler, uv); \n"
            "  return color;                                            \n"
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 0;
    result.pixel_texture_resource_count = 0;
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
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
            { { -1.0f, +1.0f }, { 0.0f, 1.0f } },
            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },

            { { +1.0f, +1.0f }, { 1.0f, 1.0f } },
            { { +1.0f, -1.0f }, { 1.0f, 0.0f } },
            { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
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
            "cbuffer cbuffer1 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
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
            "cbuffer cbuffer2 : register(b1)                            \n" // b1 = constant buffer bound to slot 1
            "{                                                          \n"
            "  uint count;                                              \n"
            "  uint3 padding;                                           \n"
            "  ObjectData objects[" STR(kFrameDataMaxObjectDataCapacity) "];      \n"
            "}                                                          \n"
            "                                                           \n"
            "float sd_circle(float2 p, float r)                         \n"
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
    result.layout = layout;
    result.vshader = vshader;
    result.pshader = pshader;
    result.vertex_buffer.buffer = vbuffer;
    result.vertex_buffer.stride = sizeof(struct Vertex);
    result.vertex_buffer.offset = 0;
    result.vertex_constant_buffers_count = 2;
    result.vertex_constant_buffers[0] = transform_constant_buffer;
    result.vertex_constant_buffers[1] = objects_constant_buffer;
    result.pixel_texture_resource_count = 0;
    return result;
}

static void
InitDirectX11(DirectX11State *state, HWND window, m4x4 projection_martix)
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
        ID3D11Device_QueryInterface(device, &IID_ID3D11InfoQueue, (void* *)&info);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        ID3D11InfoQueue_Release(info);
    }

    // enable debug break for DXGI too
    {
        IDXGIInfoQueue* dxgi_info;
        hr = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, (void* *)&dxgi_info);
        AssertHR(hr);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info, DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
        IDXGIInfoQueue_Release(dxgi_info);
    }

    // after this there's no need to check for any errors on device functions manually
    // so all HRESULT return  values in this code will be ignored
    // debugger will break on errors anyway
    #endif

    // create DXGI swap chain
    IDXGISwapChain1* swap_chain;
    {
        // get DXGI device from D3D11 device
        IDXGIDevice* dxgi_device;
        hr = ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void* *)&dxgi_device);
        AssertHR(hr);

        // get DXGI adapter from DXGI device
        IDXGIAdapter* dxgi_adapter;
        hr = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);
        AssertHR(hr);

        // get DXGI factory from DXGI adapter
        IDXGIFactory2* factory;
        hr = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, (void* *)&factory);
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

        hr = IDXGIFactory2_CreateSwapChainForHwnd(factory, (IUnknown *)device, window, &desc, NULL, NULL, &swap_chain);
        AssertHR(hr);

        // disable silly Alt+Enter changing monitor resolution to match window size
        IDXGIFactory_MakeWindowAssociation(factory, window, DXGI_MWA_NO_ALT_ENTER);

        IDXGIFactory2_Release(factory);
        IDXGIAdapter_Release(dxgi_adapter);
        IDXGIDevice_Release(dxgi_device);
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

    ID3D11Buffer* object_buffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(ObjectBufferHeader) + (sizeof(FrameDataFrameDataObjectData) * kFrameDataMaxObjectDataCapacity),
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(device, &desc, NULL, &object_buffer);
    }

    for (u8 i = 0; i < ArrayCount(state->radiance_constant_buffer); i++)
    {
        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(RadianceData),
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(device, &desc, NULL, &state->radiance_constant_buffer[i]);
    }

    ID3D11Buffer* jump_flood_constant_buffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(JumpFloodData),
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(device, &desc, NULL, &jump_flood_constant_buffer);
    }

    ID3D11Buffer* render_size_constant_buffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            .ByteWidth = sizeof(RenderSizeData),
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        };

        ID3D11Device_CreateBuffer(device, &desc, NULL, &render_size_constant_buffer);
    }

    ID3D11BlendState* blend_state;
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
        ID3D11Device_CreateBlendState(device, &desc, &blend_state);
    }

    ID3D11BlendState* no_blend_state;
    {
        D3D11_BLEND_DESC desc =
        {
            .RenderTarget[0] =
            {
                .BlendEnable = TRUE,
                .SrcBlend = D3D11_BLEND_ONE,
                .DestBlend = D3D11_BLEND_ZERO,
                .BlendOp = D3D11_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D11_BLEND_ONE,
                .DestBlendAlpha = D3D11_BLEND_ZERO,
                .BlendOpAlpha = D3D11_BLEND_OP_ADD,
                .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
            },
        };
        ID3D11Device_CreateBlendState(device, &desc, &no_blend_state);
    }

    ID3D11RasterizerState* rasterizer_state;
    {
        // disable culling
        D3D11_RASTERIZER_DESC desc =
        {
            .FillMode = D3D11_FILL_SOLID,
            .CullMode = D3D11_CULL_NONE,
            .DepthClipEnable = TRUE,
        };
        ID3D11Device_CreateRasterizerState(device, &desc, &rasterizer_state);
    }

    ID3D11DepthStencilState* depth_state;
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
        ID3D11Device_CreateDepthStencilState(device, &desc, &depth_state);
    }

    ID3D11SamplerState* linear_clamp_sampler;
    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
            .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
            .MipLODBias = 0,
            .MaxAnisotropy = 1,
            .MinLOD = 0,
            .MaxLOD = D3D11_FLOAT32_MAX,
        };

        ID3D11Device_CreateSamplerState(device, &desc, &linear_clamp_sampler);
    }

    ID3D11SamplerState* linear_wrap_sampler;
    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
            .MipLODBias = 0,
            .MaxAnisotropy = 1,
            .MinLOD = 0,
            .MaxLOD = D3D11_FLOAT32_MAX,
        };

        ID3D11Device_CreateSamplerState(device, &desc, &linear_wrap_sampler);
    }

    ID3D11SamplerState* point_sampler;
    {
        D3D11_SAMPLER_DESC desc =
        {
            .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
            .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
            .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
            .MipLODBias = 0,
            .MaxAnisotropy = 1,
            .MinLOD = 0,
            .MaxLOD = D3D11_FLOAT32_MAX,
        };

        ID3D11Device_CreateSamplerState(device, &desc, &point_sampler);
    }

    RenderPipelineDescription main_render_pipeline = CreateMainPipeline(device, transform_constant_buffer, object_buffer);

    RenderPipelineDescription seed_jump_flood_pipeline = CreateSeedJumpFloodPipeline(device);
    RenderPipelineDescription jump_flood_pipeline = CreateJumpFloodPipeline(device, render_size_constant_buffer, jump_flood_constant_buffer);

    RenderPipelineDescription sdf_pipeline = CreateSDFPipeline(device, render_size_constant_buffer);

    RenderPipelineDescription radiance_pipeline = CreateRadiancePipeline(device);

    RenderPipelineDescription final_blit_pipeline = CreateFinalBlitPipeline(device);

    state->device = device;
    state->context = context;
    state->swap_chain = swap_chain;

    state->main_pipeline = main_render_pipeline;
    state->sdf_pipeline = sdf_pipeline;
    state->radiance_pipeline = radiance_pipeline;
    state->final_blit_pipeline = final_blit_pipeline;
    state->seed_jump_flood_pipeline = seed_jump_flood_pipeline;
    state->jump_flood_pipeline = jump_flood_pipeline;

    state->jump_flood_constant_buffer = jump_flood_constant_buffer;
    state->render_size_constant_buffer = render_size_constant_buffer;
    state->objects_constant_buffer  = object_buffer;
    state->radiance_constant_buffer_count = 0;

    state->linear_clamp_sampler = linear_clamp_sampler;
    state->linear_wrap_sampler = linear_wrap_sampler;
    state->point_sampler = point_sampler;

    state->rasterizer_state = rasterizer_state;
    state->blend_state = blend_state;
    state->no_blend_state = no_blend_state;
    state->depth_state = depth_state;
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
    for (u8 i = 0; i < pipeline->vertex_constant_buffers_count; i++)
    {
        ID3D11DeviceContext_VSSetConstantBuffers(context, i, 1, &pipeline->vertex_constant_buffers[i]);
    }

    ID3D11DeviceContext_VSSetShader(context, pipeline->vshader, NULL, 0);

    // Pixel Shader
    for (u8 i = 0; i < pipeline->pixel_texture_resource_count; i++)
    {
        PixelTextureResource *resource = &pipeline->pixel_texture_resources[i];
        if (resource->sampler)
        {
            ID3D11DeviceContext_PSSetSamplers(context, i, 1, &resource->sampler);
        }
        ID3D11DeviceContext_PSSetShaderResources(context, i, 1, &resource->texture_view);
    }
    for (u8 i = 0; i < pipeline->pixel_constant_buffers_count; i++)
    {
        ID3D11DeviceContext_PSSetConstantBuffers(context, i, 1, &pipeline->pixel_constant_buffers[i]);
    }
    ID3D11DeviceContext_PSSetShader(context, pipeline->pshader, NULL, 0);
}

static void
CreateRenderTexture(ID3D11Device *device, u32 width, u32 height, DXGI_FORMAT format, ID3D11RenderTargetView **rt_view, ID3D11ShaderResourceView **view)
{ 
    D3D11_TEXTURE2D_DESC desc =
    {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = format,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
    };

    ID3D11Texture2D* texture;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &texture);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)texture, NULL, rt_view);
    ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource*)texture, NULL, view);
    ID3D11Texture2D_Release(texture);
}

static void
EndFrameDirectX11(DirectX11State *directx_state, FrameData *frame_data)
{
    HRESULT hr;

    FrameDataScreenSize screen_size = *FrameDataScreenSizePrt(frame_data);
    FrameDataViewport frame_data_viewport = *FrameDataViewportPrt(frame_data);

    // resize swap chain if needed
    if (directx_state->backbuffer_rt_view == NULL || screen_size.Width != directx_state->current_screen_width || screen_size.Height != directx_state->current_screen_height)
    {
        if (directx_state->backbuffer_rt_view)
        {
            // release old swap chain buffers
            ID3D11DeviceContext_ClearState(directx_state->context);
            ID3D11RenderTargetView_Release(directx_state->backbuffer_rt_view);

            ID3D11RenderTargetView_Release(directx_state->radiance_current_rt_view);
            ID3D11ShaderResourceView_Release(directx_state->radiance_current_view);

            ID3D11RenderTargetView_Release(directx_state->radiance_previous_rt_view);
            ID3D11ShaderResourceView_Release(directx_state->radiance_previous_view);

            ID3D11RenderTargetView_Release(directx_state->sdf_rt_view);
            ID3D11ShaderResourceView_Release(directx_state->sdf_view);
			
            ID3D11RenderTargetView_Release(directx_state->game_rt_view);
            ID3D11ShaderResourceView_Release(directx_state->game_view);

            ID3D11RenderTargetView_Release(directx_state->jf_rt_view);
            ID3D11ShaderResourceView_Release(directx_state->jf_view);

            ID3D11DepthStencilView_Release(directx_state->ds_view);
            directx_state->backbuffer_rt_view = NULL;
            directx_state->sdf_rt_view = NULL;
            directx_state->radiance_current_rt_view = NULL;
            directx_state->radiance_previous_view = NULL;
            directx_state->game_rt_view = NULL;
            directx_state->jf_rt_view = NULL;
        }

        // resize to new size for non-zero size
        if (screen_size.Width != 0 && screen_size.Height != 0)
        {
            // Should be pow2 sizes only (either whole or fractional: 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, etc.).
            // Increasing linear spacing will reduce quality, decreasing linear spacing will increase quality.
            f32 render_linear = 1.0f;

            // Should be equal to diagonal of the square of linear resolution.
            // Set to a large distance for debugging (should see individually cascaded rays in scene).
            f32 render_interval = v2_length(V2(render_linear, render_linear)) * 0.5f;

            f32 targer_width = (f32)frame_data_viewport.Width;
            f32 targer_height = (f32)frame_data_viewport.Height;
            // Calculate the max cascade count based on the highest interval distance that would reach outside the screen.
            // Valdiate intput and correct input settings.
            // Divide the final cascade size by 2, since we're "pre-avergaing."
            // Angular resolution is now set to a minimuim default 4-rays per probe (keeps cascade size consistent).
            // Fidelity can be increased by decreasing spacing between probes instead of rays per probe.
            u8 radiance_cascades = (u8)ceilf(log_base(4.0f, v2_length(V2(targer_width, targer_height))));
            f32 radiance_linear = power_of_n(render_linear, 2.0f);
            f32 radiance_interval = multiple_of_n(render_interval, 2.0f);

            radiance_cascades = min(radiance_cascades, kMaxRadianceCascades);

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // FIXES CASCADE RAY/PROBE TRADE-OFF ERROR RATE FOR NON-POW2 RESOLUTIONS: (very important).
            f32 error_rate = powf((f32)(radiance_cascades - 1), 2.0f);
            f32 errorx = ceilf(targer_width / error_rate);
            f32 errory = ceilf(targer_height / error_rate);
            s32 render_width = (s32)(errorx * error_rate);
            s32 render_height = (s32)(errory * error_rate);
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            s32 radiance_width = (s32)(floorf(render_width / radiance_linear));
            s32 radiance_height = (s32)(floorf(render_height / radiance_linear));

            directx_state->radiance_constant_buffer_count = radiance_cascades;

            directx_state->render_width = render_width;
            directx_state->render_height = render_height;

            directx_state->radiance_width = radiance_width;
            directx_state->radiance_height = radiance_height;

            for (u32 i = 0; i < radiance_cascades; i++)
            {
                RadianceData radiance_data = { 0 };
                radiance_data.cascade_count = radiance_cascades;
                radiance_data.cascade_index = i;
                radiance_data.radiance_linear = radiance_linear;
                radiance_data.radiance_interval = radiance_interval;
                radiance_data.render_size = V2((f32)render_width, (f32)render_height);
                radiance_data.radiance_size = V2((f32)radiance_width, (f32)radiance_height);

                // upload data to gpu
                {
                    // setup object data in uniform		
                    D3D11_MAPPED_SUBRESOURCE mapped;
                    ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->radiance_constant_buffer[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                    memcpy(mapped.pData, &radiance_data, sizeof(RadianceData));
                    ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->radiance_constant_buffer[i], 0);
                }
            }

            directx_state->jump_flood_passes = (u8)ceilf(log_base(2.0f, (f32)max(render_width, render_height)));

            RenderSizeData render_size_data = { 0 };
            render_size_data.size = V2((f32)render_width, (f32)render_height);

            // upload data to gpu
            {
                D3D11_MAPPED_SUBRESOURCE mapped;
                ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->render_size_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                memcpy(mapped.pData, &render_size_data, sizeof(RenderSizeData));
                ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->render_size_constant_buffer, 0);
            }

            {
                hr = IDXGISwapChain1_ResizeBuffers(directx_state->swap_chain, 0, screen_size.Width, screen_size.Height, DXGI_FORMAT_UNKNOWN, 0);
                if (FAILED(hr))
                {
                    FatalError("Failed to resize swap chain!");
                }

                // create RenderTarget view for new backbuffer texture
                ID3D11Texture2D* back_buffer;
                IDXGISwapChain1_GetBuffer(directx_state->swap_chain, 0, &IID_ID3D11Texture2D, (void* *)&back_buffer);
                ID3D11Device_CreateRenderTargetView(directx_state->device, (ID3D11Resource*)back_buffer, NULL, &directx_state->backbuffer_rt_view);
                ID3D11Texture2D_Release(back_buffer);
            }

            {
                D3D11_TEXTURE2D_DESC depth_desc =
                {
                    .Width = screen_size.Width,
                    .Height = screen_size.Height,
                    .MipLevels = 1,
                    .ArraySize = 1,
                    .Format = DXGI_FORMAT_D32_FLOAT, // or use DXGI_FORMAT_D32_FLOAT_S8X24_UINT if you need stencil
                    .SampleDesc = { 1, 0 },
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags = D3D11_BIND_DEPTH_STENCIL,
                };

                // create new depth stencil texture & DepthStencil view
                ID3D11Texture2D* depth;
                ID3D11Device_CreateTexture2D(directx_state->device, &depth_desc, NULL, &depth);
                ID3D11Device_CreateDepthStencilView(directx_state->device, (ID3D11Resource*)depth, NULL, &directx_state->ds_view);
                ID3D11Texture2D_Release(depth);
            }
         
            // create new sdf texture & view
            CreateRenderTexture(directx_state->device, render_width, render_height, DXGI_FORMAT_R8G8B8A8_UNORM, &directx_state->sdf_rt_view, &directx_state->sdf_view);

            CreateRenderTexture(directx_state->device, render_width, render_height, DXGI_FORMAT_R8G8B8A8_UNORM, &directx_state->jf_rt_view, &directx_state->jf_view);

            CreateRenderTexture(directx_state->device, render_width, render_height, DXGI_FORMAT_R8G8B8A8_UNORM, &directx_state->game_rt_view, &directx_state->game_view);

            // create new radiance texture & view
            CreateRenderTexture(directx_state->device, radiance_width, radiance_height, DXGI_FORMAT_R8G8B8A8_UNORM, &directx_state->radiance_current_rt_view, &directx_state->radiance_current_view);
            CreateRenderTexture(directx_state->device, radiance_width, radiance_height, DXGI_FORMAT_R8G8B8A8_UNORM, &directx_state->radiance_previous_rt_view, &directx_state->radiance_previous_view);
        }

        directx_state->current_screen_width = screen_size.Width;
        directx_state->current_screen_height = screen_size.Height;
    }

    // can render only if window size is non-zero - we must have backbuffer & RenderTarget view created
    if (directx_state->backbuffer_rt_view)
    {
        FrameDataFrameData* frame_data_sheet = FrameDataFrameDataPrt(frame_data);
        FrameDataFrameDataObjectData* object_data = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

        u32 frame_object_count = *FrameDataFrameObjectCountPrt(frame_data);

        u32 object_count_this_frame = min(frame_object_count, kFrameDataMaxObjectDataCapacity);

        // upload data to gpu
        {
            // setup object data in uniform		
            D3D11_MAPPED_SUBRESOURCE mapped;
            ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->objects_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

            ObjectBufferHeader header = { 0 };
            header.frame_object_count = object_count_this_frame;

            memcpy(mapped.pData, &header, sizeof(ObjectBufferHeader));
            memcpy((u8 *)mapped.pData + sizeof(ObjectBufferHeader), object_data, sizeof(FrameDataFrameDataObjectData) * kFrameDataMaxObjectDataCapacity);
            ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->objects_constant_buffer, 0);
        }

        D3D11_VIEWPORT game_viewport =
        {
            .TopLeftX = frame_data_viewport.X,
            .TopLeftY = frame_data_viewport.Y,
            .Width = frame_data_viewport.Width,
            .Height = frame_data_viewport.Height,
            .MinDepth = 0,
            .MaxDepth = 1,
        };

        D3D11_VIEWPORT render_viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width = (FLOAT)directx_state->render_width,
            .Height = (FLOAT)directx_state->render_height,
            .MinDepth = 0,
            .MaxDepth = 1,
        };

        D3D11_VIEWPORT radiance_viewport =
        {
            .TopLeftX = 0,
            .TopLeftY = 0,
            .Width = (FLOAT)directx_state->radiance_width,
            .Height = (FLOAT)directx_state->radiance_height,
            .MinDepth = 0,
            .MaxDepth = 1,
        };

        // clear screen
        FLOAT color[] = { 0.392f, 0.584f, 0.929f, 1.f };
        FLOAT black_color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->backbuffer_rt_view, color);
        ID3D11DeviceContext_ClearDepthStencilView(directx_state->context, directx_state->ds_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

        // Rasterizer Stage
        ID3D11DeviceContext_RSSetViewports(directx_state->context, 1, &render_viewport);
        ID3D11DeviceContext_RSSetState(directx_state->context, directx_state->rasterizer_state);

        #if 1
        // Render game objects
        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->game_rt_view, black_color);

        ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->blend_state, NULL, ~0U);
        ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->game_rt_view, NULL);
        {
            BindPineline(directx_state->context, &directx_state->main_pipeline);

            // Draw objects with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, object_count_this_frame, 0, 0);
        }
        #endif

        // Initial pass to initialize jump flood
        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->jf_rt_view, black_color);

        ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->no_blend_state, NULL, ~0U);
        ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->jf_rt_view, NULL);
        {
            directx_state->seed_jump_flood_pipeline.pixel_texture_resource_count = 1;
            directx_state->seed_jump_flood_pipeline.pixel_texture_resources[0].sampler = directx_state->linear_clamp_sampler;
            directx_state->seed_jump_flood_pipeline.pixel_texture_resources[0].texture_view = directx_state->game_view;

            BindPineline(directx_state->context, &directx_state->seed_jump_flood_pipeline);

            // Draw object with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);

            // Unbind textures
            ID3D11ShaderResourceView* nullSRV[1] = { NULL };
            for (u8 t = 0; t < directx_state->seed_jump_flood_pipeline.pixel_texture_resource_count; t++)
            {
                ID3D11DeviceContext_PSSetShaderResources(directx_state->context, t, 1, nullSRV);
            }
        }

        #if 1        
        ID3D11RenderTargetView *jump_flood_render_target_swap[2];
        ID3D11ShaderResourceView *jump_flood_resource_view_swap[2];

        jump_flood_render_target_swap[0] = directx_state->sdf_rt_view;
        jump_flood_render_target_swap[1] = directx_state->jf_rt_view;

        jump_flood_resource_view_swap[0] = directx_state->jf_view;
        jump_flood_resource_view_swap[1] = directx_state->sdf_view;

        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->sdf_rt_view, black_color);

        u8 jump_flood_swap_index = 0;

        {
            for (u8 i = 0; i < directx_state->jump_flood_passes; i++)
            {
                f32 offset = powf(2.0f, (f32)(directx_state->jump_flood_passes - i - 1));

                ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->no_blend_state, NULL, ~0U);
                ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &jump_flood_render_target_swap[jump_flood_swap_index], NULL);

                JumpFloodData jump_flood_data = { 0 };
                jump_flood_data.jump_distance = offset;

                // upload data to gpu
                {
                    D3D11_MAPPED_SUBRESOURCE mapped;
                    ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->jump_flood_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                    memcpy(mapped.pData, &jump_flood_data, sizeof(JumpFloodData));
                    ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->jump_flood_constant_buffer, 0);
                }

                directx_state->jump_flood_pipeline.pixel_texture_resource_count = 1;
                directx_state->jump_flood_pipeline.pixel_texture_resources[0].sampler = directx_state->linear_wrap_sampler;
                directx_state->jump_flood_pipeline.pixel_texture_resources[0].texture_view = jump_flood_resource_view_swap[jump_flood_swap_index];

                BindPineline(directx_state->context, &directx_state->jump_flood_pipeline);

                // Draw object with 6 vertices
                ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);

                // Unbind textures
                ID3D11ShaderResourceView* nullSRV[1] = { NULL };
                for (u8 t = 0; t < directx_state->jump_flood_pipeline.pixel_texture_resource_count; t++)
                {
                    ID3D11DeviceContext_PSSetShaderResources(directx_state->context, t, 1, nullSRV);
                }

                jump_flood_swap_index = (jump_flood_swap_index + 1) % ArrayCount(jump_flood_render_target_swap);
            }
        }
        #endif

        #if 1

        ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->no_blend_state, NULL, ~0U);
        ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->sdf_rt_view, NULL);
        {
            directx_state->sdf_pipeline.pixel_texture_resource_count = 1;
            directx_state->sdf_pipeline.pixel_texture_resources[0].sampler = directx_state->linear_wrap_sampler;
            directx_state->sdf_pipeline.pixel_texture_resources[0].texture_view = directx_state->jf_view;

            BindPineline(directx_state->context, &directx_state->sdf_pipeline);

            // Draw object with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);

            // Unbind textures
            ID3D11ShaderResourceView* nullSRV[1] = { NULL };
            for (u8 t = 0; t < directx_state->sdf_pipeline.pixel_texture_resource_count; t++)
            {
                ID3D11DeviceContext_PSSetShaderResources(directx_state->context, t, 1, nullSRV);
            }
        }
        #endif

        #if 1

        ID3D11RenderTargetView *radiance_render_target_swap[2];
        ID3D11ShaderResourceView *radiance_resource_view_swap[2];

        radiance_render_target_swap[0] = directx_state->radiance_current_rt_view;
        radiance_render_target_swap[1] = directx_state->radiance_previous_rt_view;

        radiance_resource_view_swap[0] = directx_state->radiance_previous_view;
        radiance_resource_view_swap[1] = directx_state->radiance_current_view;

        u8 radiance_swap_index = 0;

        // Rasterizer Stage
        ID3D11DeviceContext_RSSetViewports(directx_state->context, 1, &radiance_viewport);
        ID3D11DeviceContext_RSSetState(directx_state->context, directx_state->rasterizer_state);

        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->radiance_current_rt_view, black_color);
        ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->radiance_previous_rt_view, black_color);

        for (s8 i = directx_state->radiance_constant_buffer_count - 1; i >= 0; i--)
        {
            ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->no_blend_state, NULL, ~0U);
            ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &radiance_render_target_swap[radiance_swap_index], NULL);
            {
                directx_state->radiance_pipeline.pixel_texture_resource_count = 3;
                directx_state->radiance_pipeline.pixel_texture_resources[0].sampler = directx_state->linear_wrap_sampler;
                directx_state->radiance_pipeline.pixel_texture_resources[0].texture_view = directx_state->sdf_view;
                directx_state->radiance_pipeline.pixel_texture_resources[1].sampler = directx_state->linear_wrap_sampler;
                directx_state->radiance_pipeline.pixel_texture_resources[1].texture_view = radiance_resource_view_swap[radiance_swap_index];
                directx_state->radiance_pipeline.pixel_texture_resources[2].sampler = directx_state->linear_clamp_sampler;
                directx_state->radiance_pipeline.pixel_texture_resources[2].texture_view = directx_state->game_view;

                directx_state->radiance_pipeline.pixel_constant_buffers_count = 1;
                directx_state->radiance_pipeline.pixel_constant_buffers[0] = directx_state->radiance_constant_buffer[i];

                BindPineline(directx_state->context, &directx_state->radiance_pipeline);

                // Draw object with 6 vertices
                ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);

                // Unbind textures
                ID3D11ShaderResourceView* nullSRV[1] = { NULL };
                for (u8 t = 0; t < directx_state->radiance_pipeline.pixel_texture_resource_count; t++)
                {
                    ID3D11DeviceContext_PSSetShaderResources(directx_state->context, t, 1, nullSRV);
                }
            }

            radiance_swap_index = (radiance_swap_index + 1) % ArrayCount(radiance_render_target_swap);
        }
        #endif

        // Rasterizer Stage
        ID3D11DeviceContext_RSSetViewports(directx_state->context, 1, &game_viewport);
        
        // Output Merger
        ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->blend_state, NULL, ~0U);
        ID3D11DeviceContext_OMSetDepthStencilState(directx_state->context, directx_state->depth_state, 0);
        ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->backbuffer_rt_view, directx_state->ds_view);

        {
            directx_state->final_blit_pipeline.pixel_texture_resource_count = 1;
            directx_state->final_blit_pipeline.pixel_texture_resources[0].sampler = directx_state->linear_wrap_sampler;
            #if 1
            directx_state->final_blit_pipeline.pixel_texture_resources[0].texture_view = radiance_resource_view_swap[radiance_swap_index];
            #else
            directx_state->final_blit_pipeline.pixel_texture_resources[0].texture_view = directx_state->sdf_view;
            #endif
            BindPineline(directx_state->context, &directx_state->final_blit_pipeline);

            // Draw object with 6 vertices
            ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, 1, 0, 0);

            // Unbind textures
            ID3D11ShaderResourceView* nullSRV[1] = { NULL };
            for (u8 t = 0; t < directx_state->final_blit_pipeline.pixel_texture_resource_count; t++)
            {
                ID3D11DeviceContext_PSSetShaderResources(directx_state->context, t, 1, nullSRV);
            }
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