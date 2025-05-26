// example how to set up D3D11 rendering on Windows in C

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stddef.h>

#pragma comment (lib, "gdi32")
#pragma comment (lib, "user32")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "d3dcompiler")

#define STR2(x) #x
#define STR(x) STR2(x)

#ifndef NDEBUG
#include "enable_console_output.h"
#endif

#include "types.h"
#include "math.h"

#include "enemy_instances.h"
#include "frame_data.h"
#include "enemy_bullets.h"

#include "enemy_instances_update.c"
#include "enemy_instances_draw.c"

#define AssertHR(hr) Assert(SUCCEEDED(hr))

#ifndef __cplusplus
typedef struct MapFileData      MapFileData;
typedef enum MapFilePermissions MapFilePermissions;
typedef struct DirectX11State   DirectX11State;
typedef struct Vertex           Vertex;
#endif

struct MapFileData
{
	HANDLE fileHandle;
	void* data;
};

enum MapFilePermissions
{
	MapFilePermitions_Read = 0,
    MapFilePermitions_ReadWriteCopy = 1,
	MapFilePermitions_ReadWrite = 2
};

struct DirectX11State
{
    ID3D11Device* device;
    ID3D11DeviceContext* context;
	IDXGISwapChain1* swapChain;

	ID3D11Buffer* vbuffer;
	ID3D11InputLayout* layout;
	ID3D11VertexShader* vshader;
	ID3D11Buffer* ubuffer;
	ID3D11Buffer* objectBuffer;
    ID3D11PixelShader* pshader;

	ID3D11RenderTargetView* rtView;
    ID3D11DepthStencilView* dsView;

	ID3D11RasterizerState* rasterizerState;
	ID3D11BlendState* blendState;
	ID3D11DepthStencilState* depthState;

	s32 currentWidth;
    s32 currentHeight;
};

struct Vertex
{
	f32 position[2];
	f32 uv[2];
};

static void
FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

static LRESULT CALLBACK 
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(wnd, msg, wparam, lparam);
}

static DirectX11State 
InitDirectX11(HWND window, f32 projection_martix[16])
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
	
    ID3D11Buffer* vbuffer;
    {
		Vertex data[] =
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
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, uv),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
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

    ID3D11Buffer* ubuffer;
    {
        D3D11_BUFFER_DESC desc =
        {
            // space for 4x4 float matrix (cbuffer0 from pixel shader)
            .ByteWidth = 4 * 4 * sizeof(f32),
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        };
        D3D11_SUBRESOURCE_DATA initial = { .pSysMem = projection_martix };
        ID3D11Device_CreateBuffer(device, &desc, &initial, &ubuffer);
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

	DirectX11State state = {};
	state.device = device;
	state.context = context;
	state.swapChain = swapChain;

	state.vbuffer = vbuffer;
	state.layout = layout;
	state.vshader = vshader;
	state.ubuffer = ubuffer;
	state.objectBuffer = objectBuffer;
	state.pshader = pshader;

	state.rasterizerState = rasterizerState;
	state.blendState = blendState;
	state.depthState = depthState;

	return state;
}

static void 
EndFrameDirectX11(DirectX11State *directx_state, FrameData *frame_data)
{
	HRESULT hr;

	// resize swap chain if needed
	if (directx_state->rtView == NULL || frame_data->Width != directx_state->currentWidth || frame_data->Height != directx_state->currentHeight)
	{
		if (directx_state->rtView)
		{
			// release old swap chain buffers
			ID3D11DeviceContext_ClearState(directx_state->context);
			ID3D11RenderTargetView_Release(directx_state->rtView);
			ID3D11DepthStencilView_Release(directx_state->dsView);
			directx_state->rtView = NULL;
		}

		// resize to new size for non-zero size
		if (frame_data->Width != 0 && frame_data->Height != 0)
		{
			hr = IDXGISwapChain1_ResizeBuffers(directx_state->swapChain, 0, frame_data->Width, frame_data->Height, DXGI_FORMAT_UNKNOWN, 0);
			if (FAILED(hr))
			{
				FatalError("Failed to resize swap chain!");
			}

			// create RenderTarget view for new backbuffer texture
			ID3D11Texture2D* backbuffer;
			IDXGISwapChain1_GetBuffer(directx_state->swapChain, 0, &IID_ID3D11Texture2D, (void**)&backbuffer);
			ID3D11Device_CreateRenderTargetView(directx_state->device, (ID3D11Resource*)backbuffer, NULL, &directx_state->rtView);
			ID3D11Texture2D_Release(backbuffer);

			D3D11_TEXTURE2D_DESC depthDesc =
			{
				.Width = frame_data->Width,
				.Height = frame_data->Height,
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
			ID3D11Device_CreateDepthStencilView(directx_state->device, (ID3D11Resource*)depth, NULL, &directx_state->dsView);
			ID3D11Texture2D_Release(depth);
		}

		directx_state->currentWidth = frame_data->Width;
		directx_state->currentHeight = frame_data->Height;
	}

	// can render only if window size is non-zero - we must have backbuffer & RenderTarget view created
	if (directx_state->rtView)
	{
		// output viewport covering all client area of window
		D3D11_VIEWPORT viewport =
		{
			.TopLeftX = frame_data->ViewportX,
			.TopLeftY = frame_data->ViewportY,
			.Width = frame_data->ViewportWidth,
			.Height = frame_data->ViewportHeight,
			.MinDepth = 0,
			.MaxDepth = 1,
		};

		// clear screen
		FLOAT color[] = { 0.392f, 0.584f, 0.929f, 1.f };
		ID3D11DeviceContext_ClearRenderTargetView(directx_state->context, directx_state->rtView, color);
		ID3D11DeviceContext_ClearDepthStencilView(directx_state->context, directx_state->dsView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

		// setup object data in uniform		
        D3D11_MAPPED_SUBRESOURCE mapped;
		ID3D11DeviceContext_Map(directx_state->context, (ID3D11Resource*)directx_state->objectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		
        FrameDataFrameData* frame_data_sheet = FrameDataFrameDataPrt(frame_data);
        FrameDataFrameDataObjectData* object_data = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

        memcpy(mapped.pData, object_data, sizeof(FrameDataFrameDataObjectData) * kFrameDataMaxObjectDataCapacity);
		ID3D11DeviceContext_Unmap(directx_state->context, (ID3D11Resource*)directx_state->objectBuffer, 0);
            
		// Input Assembler
		ID3D11DeviceContext_IASetInputLayout(directx_state->context, directx_state->layout);
		ID3D11DeviceContext_IASetPrimitiveTopology(directx_state->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		ID3D11DeviceContext_IASetVertexBuffers(directx_state->context, 0, 1, &directx_state->vbuffer, &stride, &offset);

		// Vertex Shader
		ID3D11DeviceContext_VSSetConstantBuffers(directx_state->context, 0, 1, &directx_state->ubuffer);
		ID3D11DeviceContext_VSSetConstantBuffers(directx_state->context, 1, 1, &directx_state->objectBuffer);
		ID3D11DeviceContext_VSSetShader(directx_state->context, directx_state->vshader, NULL, 0);

		// Rasterizer Stage
		ID3D11DeviceContext_RSSetViewports(directx_state->context, 1, &viewport);
		ID3D11DeviceContext_RSSetState(directx_state->context, directx_state->rasterizerState);

		// Pixel Shader
		ID3D11DeviceContext_PSSetShader(directx_state->context, directx_state->pshader, NULL, 0);

		// Output Merger
		ID3D11DeviceContext_OMSetBlendState(directx_state->context, directx_state->blendState, NULL, ~0U);
		ID3D11DeviceContext_OMSetDepthStencilState(directx_state->context, directx_state->depthState, 0);
		ID3D11DeviceContext_OMSetRenderTargets(directx_state->context, 1, &directx_state->rtView, directx_state->dsView);

		// Draw objects with 6 vertices
		ID3D11DeviceContext_DrawInstanced(directx_state->context, 6, frame_data->FrameDataCount, 0, 0);
			
	}

	// change to FALSE to disable vsync
	BOOL vsync = TRUE;
	hr = IDXGISwapChain1_Present(directx_state->swapChain, vsync ? 1 : 0, 0);
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

static MapFileData 
CreateMapFile(LPSTR fileName, MapFilePermissions permissions)
{
	MapFileData result = {};

	u32 fileAccess = GENERIC_READ;
	u32 fileProtection = PAGE_READONLY;
	u32 memoryAccess = FILE_MAP_READ;

    if (permissions == MapFilePermitions_ReadWriteCopy)
    {
        fileProtection = PAGE_WRITECOPY;
        memoryAccess = FILE_MAP_COPY;
    }
	else if (permissions == MapFilePermitions_ReadWrite)
	{
		fileAccess |= GENERIC_WRITE;
		fileProtection = PAGE_READWRITE;
		memoryAccess = FILE_MAP_WRITE;
	}

	HANDLE file_handle = CreateFile(
		fileName,
		fileAccess,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	
    Assert(file_handle != INVALID_HANDLE_VALUE);

	HANDLE file_mapping_handle = CreateFileMapping(
		file_handle,
		NULL,
		fileProtection,
		// Passing zeroes for the high and low max-size params here will allow the
		// entire file to be mappable.
		0,
		0,
		NULL);

    Assert(file_mapping_handle != NULL);

	// We can close this now because the file mapping retains an open handle to
	// the underlying file.
	CloseHandle(file_handle);

	void* data = MapViewOfFile(
		file_mapping_handle,
		memoryAccess,
		0, // Offset high
		0, // Offset low
		// A zero here indicates we want to map the entire range.
		0);

    Assert(data != NULL);

	result.fileHandle = file_mapping_handle;
	result.data = data;

	return result;
}

void
CloseMapFile(MapFileData *mapData)
{
    UnmapViewOfFile(mapData->data);
	CloseHandle(mapData->fileHandle);
}

void
begin_frame(FrameData *frame_data, f32 game_aspect, s32 screen_width, s32 screen_height)
{
    frame_data->Width          = screen_width;
    frame_data->Height         = screen_height;
    frame_data->FrameDataCount = 0;

    f32 window_aspect = (f32)frame_data->Width / frame_data->Height;

    if (window_aspect > game_aspect)
    {
        frame_data->ViewportWidth = frame_data->Height * game_aspect;
        frame_data->ViewportHeight = (f32)frame_data->Height;
        frame_data->ViewportX = (frame_data->Width - frame_data->ViewportWidth) * 0.5f;
        frame_data->ViewportY = 0.0f;
    }
    else
    {
        frame_data->ViewportWidth = (f32)frame_data->Width;
        frame_data->ViewportHeight = frame_data->Width / game_aspect;
        frame_data->ViewportX = 0.0f;
        frame_data->ViewportY = (frame_data->Height - frame_data->ViewportHeight) * 0.5f;
    }
}

void
setup_projection_matrix(f32 projection_martix[16], v2 game_area)
{
    v2 half_game_area = v2_scale(game_area, 0.5f);

    f32 left = -half_game_area.x;
    f32 right = half_game_area.x;
    f32 bottom = -half_game_area.y;
    f32 top = half_game_area.y;
    f32 near_clip_plane = 0.0f;
    f32 far_clip_plane = 1.0f;

    f32 a = 2.0f / (right - left);
    f32 b = 2.0f / (top - bottom);

    f32 a1 = (left + right) / (right - left);
    f32 b1 = (top + bottom) / (top - bottom);

    f32 n = near_clip_plane;
    f32 f = far_clip_plane;
    f32 d = 2.0f / (n - f);
    f32 e = (n + f) / (n - f);

    // a, 0, 0, -a1,
    // 0, b, 0, -b1,
    // 0, 0, d,  e,
    // 0, 0, 0,  1,
    projection_martix[0]  =  a;
    projection_martix[3]  = -a1;
    projection_martix[5]  =  b;
    projection_martix[7]  = -b1;
    projection_martix[10] =  d;
    projection_martix[11] =  e;
    projection_martix[15] =  1;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
{

#ifndef NDEBUG
    enable_console();
#endif

    // register window class to have custom WindowProc callback
    WNDCLASSEXW wc =
    {
        .cbSize = sizeof(wc),
        .lpfnWndProc = WindowProc,
        .hInstance = instance,
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = L"d3d11_window_class",
    };
    ATOM atom = RegisterClassExW(&wc);
    Assert(atom && "Failed to register window class");

    // window properties - width, height and style
    s32 width = CW_USEDEFAULT;
    s32 height = CW_USEDEFAULT;
    // WS_EX_NOREDIRECTIONBITMAP flag here is needed to fix ugly bug with Windows 10
    // when window is resized and DXGI swap chain uses FLIP presentation model
    // DO NOT use it if you choose to use non-FLIP presentation model
    // read about the bug here: https://stackoverflow.com/q/63096226 and here: https://stackoverflow.com/q/53000291
    DWORD exstyle = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;
    DWORD style = WS_OVERLAPPEDWINDOW;

    // uncomment in case you want fixed size window
    //style &= ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    //RECT rect = { 0, 0, 1280, 720 };
    //AdjustWindowRectEx(&rect, style, FALSE, exstyle);
    //width = rect.right - rect.left;
    //height = rect.bottom - rect.top;

    // create window
    HWND window = CreateWindowExW(
        exstyle, wc.lpszClassName, L"D3D11 Window", style,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, wc.hInstance, NULL);
    Assert(window && "Failed to create window");

    v2 game_area = V2(kEnemyInstancesWidth, kEnemyInstancesHeight);

    f32 projection_matrix[16] = { 0 };
    setup_projection_matrix(projection_matrix, game_area);

	DirectX11State directxState = InitDirectX11(window, projection_matrix);

    // show the window
    ShowWindow(window, SW_SHOWDEFAULT);

    LARGE_INTEGER freq, c1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c1);

	MapFileData enemy_instances_map_data = CreateMapFile("enemy_instances.bin", MapFilePermitions_Read);
	EnemyInstances* enemy_instances      = (EnemyInstances *)enemy_instances_map_data.data;
	
    MapFileData enemy_bullets_map_data = CreateMapFile("enemy_bullets.bin", MapFilePermitions_Read);
	EnemyBullets* enemy_bullets        = (EnemyBullets *)enemy_bullets_map_data.data;

    MapFileData frame_data_map_data = CreateMapFile("frame_data.bin", MapFilePermitions_ReadWriteCopy);
    FrameData* frame_data           = (FrameData *)frame_data_map_data.data;

    f64 time = 0.0;

    f32 game_aspect = game_area.x / game_area.y;

    for (;;)
    {
        // process all incoming Windows messages
        MSG msg;
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        // get current size for window client area
        RECT rect;
        GetClientRect(window, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        begin_frame(frame_data, game_aspect, width, height);

		if (width != 0 && height != 0)
		{
			LARGE_INTEGER c2;
			QueryPerformanceCounter(&c2);
			f32 delta = (f32)((f64)(c2.QuadPart - c1.QuadPart) / freq.QuadPart);
			c1 = c2;

            enemy_instances_update(enemy_instances, delta);

            enemy_instances_draw(enemy_instances, frame_data);

            time += delta;
		}

		EndFrameDirectX11(&directxState, frame_data);
    }

	CloseMapFile(&enemy_instances_map_data);
	CloseMapFile(&enemy_bullets_map_data);
	CloseMapFile(&frame_data_map_data);
}
