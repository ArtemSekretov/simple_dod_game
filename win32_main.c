#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <stddef.h>

#pragma comment (lib, "gdi32")
#pragma comment (lib, "user32")

#define STR2(x) #x
#define STR(x) STR2(x)

#ifndef NDEBUG
#include "enable_console_output.h"
#endif

#include "types.h"
#include "math.h"

#include "game_state.h"
#include "level_update.h"
#include "wave_update.h"

#include "play_clock.h"

#include "play_area.h"
#include "frame_data.h"

#include "enemy_instances.h"
#include "enemy_instances_wave.h"
#include "hero_instances.h"

#include "bullets.h"
#include "bullets_update.h"
#include "bullets_draw.h"
#include "bullet_source_instances.h"

#include "enemy_instances_draw.h"
#include "hero_instances_draw.h"

#include "enemy_instances_update.c"
#include "enemy_instances_draw.c"

#include "hero_instances_update.c"
#include "hero_instances_draw.c"

#include "bullets_update.c"
#include "bullets_draw.c"
#include "level_update.c"
#include "wave_update.c"

#include "collision_grid.h"
#include "collision_source_instances.h"
#include "collision_source_types.h"

#define AssertHR(hr) Assert(SUCCEEDED(hr))

static void
FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

#include "win32_d3d11.c"

#ifndef __cplusplus
typedef struct MapFileData      MapFileData;
typedef enum MapFilePermissions MapFilePermissions;
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
    u16 *frame_data_count_prt   = FrameDataFrameDataCountPrt(frame_data);
    u32 *frame_object_count_ptr = FrameDataFrameObjectCountPrt(frame_data);

    FrameDataViewport *viewport_prt      = FrameDataViewportPrt(frame_data);
    FrameDataScreenSize *screen_size_prt = FrameDataScreenSizePrt(frame_data);

    screen_size_prt->Width  = screen_width;
    screen_size_prt->Height = screen_height;
    *frame_data_count_prt   = 0;
    *frame_object_count_ptr = 0;

    f32 window_aspect = (f32)screen_size_prt->Width / screen_size_prt->Height;

    if (window_aspect > game_aspect)
    {
        viewport_prt->Width  = screen_size_prt->Height * game_aspect;
        viewport_prt->Height = (f32)screen_size_prt->Height;
        viewport_prt->X      = (screen_size_prt->Width - viewport_prt->Width) * 0.5f;
        viewport_prt->Y      = 0.0f;
    }
    else
    {
        viewport_prt->Width  = (f32)screen_size_prt->Width;
        viewport_prt->Height = screen_size_prt->Width / game_aspect;
        viewport_prt->X      = 0.0f;
        viewport_prt->Y      = (screen_size_prt->Height - viewport_prt->Height) * 0.5f;
    }
}

int WINAPI 
WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
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

    v2 game_area = V2(kPlayAreaWidth, kPlayAreaHeight);

    v2 half_game_area = v2_scale(game_area, 0.5f);

    f32 left = -half_game_area.x;
    f32 right = half_game_area.x;
    f32 bottom = -half_game_area.y;
    f32 top = half_game_area.y;

    f32 near_clip_plane = 0.0f;
    f32 far_clip_plane = 1.0f;

    m4x4_inv matrix = orthographic_projection(left, right, bottom, top, near_clip_plane, far_clip_plane);

    DirectX11State directx_state = { 0 };
	InitDirectX11(&directx_state, window, matrix.forward);

    // show the window
    ShowWindow(window, SW_SHOWDEFAULT);

    MapFileData game_state_map_data = CreateMapFile("game_state.bin", MapFilePermitions_ReadWriteCopy);
    GameState *game_state           = (GameState *)game_state_map_data.data;

    MapFileData level_update_map_data = CreateMapFile("level_update.bin", MapFilePermitions_ReadWriteCopy);
    LevelUpdate *level_update_data     = (LevelUpdate *)level_update_map_data.data;

    MapFileData wave_update_map_data = CreateMapFile("wave_update.bin", MapFilePermitions_ReadWriteCopy);
    WaveUpdate *wave_update_data     = (WaveUpdate *)wave_update_map_data.data;

	MapFileData enemy_instances_map_data = CreateMapFile("enemy_instances.bin", MapFilePermitions_ReadWriteCopy);
	EnemyInstances *enemy_instances      = (EnemyInstances *)enemy_instances_map_data.data;
	
    MapFileData enemy_bullets_map_data = CreateMapFile("enemy_bullets.bin", MapFilePermitions_Read);
	Bullets *enemy_bullets             = (Bullets *)enemy_bullets_map_data.data;

    MapFileData hero_bullets_map_data = CreateMapFile("hero_bullets.bin", MapFilePermitions_Read);
	Bullets *hero_bullets             = (Bullets *)hero_bullets_map_data.data;
    
    MapFileData hero_instances_map_data = CreateMapFile("hero_instances.bin", MapFilePermitions_ReadWriteCopy);
    HeroInstances *hero_instances       = (HeroInstances *)hero_instances_map_data.data;

    MapFileData frame_data_map_data = CreateMapFile("frame_data.bin", MapFilePermitions_ReadWriteCopy);
    FrameData *frame_data           = (FrameData *)frame_data_map_data.data;

    MapFileData enemy_bullets_update_map_data = CreateMapFile("enemy_bullets_update.bin", MapFilePermitions_ReadWriteCopy);
    BulletsUpdate *enemy_bullets_update_data  = (BulletsUpdate *)enemy_bullets_update_map_data.data;

    MapFileData hero_bullets_update_map_data = CreateMapFile("hero_bullets_update.bin", MapFilePermitions_ReadWriteCopy);
    BulletsUpdate *hero_bullets_update_data  = (BulletsUpdate *)hero_bullets_update_map_data.data;

    MapFileData hero_bullets_collition_grip_map_data = CreateMapFile("hero_bullets_collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *hero_bullets_collition_grip       = (CollisionGrid *)hero_bullets_collition_grip_map_data.data;

    MapFileData enemy_bullets_collition_grip_map_data = CreateMapFile("enemy_bullets_collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *enemy_bullets_collition_grip       = (CollisionGrid *)enemy_bullets_collition_grip_map_data.data;

    MapFileData hero_instances_collition_grip_map_data = CreateMapFile("hero_instances_collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *hero_instances_collition_grip       = (CollisionGrid *)hero_instances_collition_grip_map_data.data;

    MapFileData enemy_instances_collition_grip_map_data = CreateMapFile("enemy_instances_collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *enemy_instances_collition_grip       = (CollisionGrid *)enemy_instances_collition_grip_map_data.data;

    BulletSourceInstances *enemy_bullet_source_instances = EnemyInstancesBulletSourceInstancesMapPrt(enemy_instances);
    BulletSourceInstances *hero_bullet_source_instances  = HeroInstancesBulletSourceInstancesMapPrt(hero_instances);

    PlayClock *enemy_play_clock = WaveUpdatePlayClockMapPrt(wave_update_data);
    PlayClock *hero_play_clock  = LevelUpdatePlayClockMapPrt(level_update_data);

    BulletsUpdateContext enemy_bullets_update_context;
    enemy_bullets_update_context.Root                     = enemy_bullets_update_data;
    enemy_bullets_update_context.BulletsBin               = enemy_bullets;
    enemy_bullets_update_context.BulletSourceInstancesBin = enemy_bullet_source_instances;
    enemy_bullets_update_context.GameStateBin             = game_state;
    enemy_bullets_update_context.PlayClockBin             = enemy_play_clock;

    BulletsDrawContext enemy_bullets_draw_context;
    enemy_bullets_draw_context.BulletsBin       = enemy_bullets;
    enemy_bullets_draw_context.BulletsUpdateBin = enemy_bullets_update_data;
    enemy_bullets_draw_context.FrameDataBin     = frame_data;

    BulletsUpdateContext hero_bullets_update_context;
    hero_bullets_update_context.Root                     = hero_bullets_update_data;
    hero_bullets_update_context.BulletsBin               = hero_bullets;
    hero_bullets_update_context.BulletSourceInstancesBin = hero_bullet_source_instances;
    hero_bullets_update_context.GameStateBin             = game_state;
    hero_bullets_update_context.PlayClockBin             = hero_play_clock;

    BulletsDrawContext hero_bullets_draw_context;
    hero_bullets_draw_context.BulletsBin       = hero_bullets;
    hero_bullets_draw_context.BulletsUpdateBin = hero_bullets_update_data;
    hero_bullets_draw_context.FrameDataBin     = frame_data;

    EnemyInstancesContext enemy_instances_context;
    enemy_instances_context.Root           = enemy_instances;
    enemy_instances_context.GameStateBin   = game_state;
    enemy_instances_context.WaveUpdateBin  = wave_update_data;
    enemy_instances_context.LevelUpdateBin = level_update_data;

    EnemyInstancesDrawContext enemy_instances_draw_context;
    enemy_instances_draw_context.EnemyInstancesBin = enemy_instances;
    enemy_instances_draw_context.FrameDataBin      = frame_data;

    LevelUpdateContext level_update_context;
    level_update_context.Root                  = level_update_data;
    level_update_context.GameStateBin          = game_state;
    level_update_context.EnemyInstancesBin     = enemy_instances;
    level_update_context.EnemyBulletsUpdateBin = enemy_bullets_update_data;

    WaveUpdateContext wave_update_context;
    wave_update_context.Root                  = wave_update_data;
    wave_update_context.GameStateBin          = game_state;
    wave_update_context.EnemyInstancesBin     = enemy_instances;
    wave_update_context.EnemyBulletsUpdateBin = enemy_bullets_update_data;
    wave_update_context.LevelUpdateBin        = level_update_data;

    HeroInstancesContext hero_instances_context;
    hero_instances_context.Root         = hero_instances;
    hero_instances_context.GameStateBin = game_state;

    HeroInstancesDrawContext hero_instances_draw_context;
    hero_instances_draw_context.HeroInstancesBin = hero_instances;
    hero_instances_draw_context.FrameDataBin     = frame_data;

    CollisionGridContext hero_bullets_collition_grip_context;
    hero_bullets_collition_grip_context.Root = hero_bullets_collition_grip;
    hero_bullets_collition_grip_context.CollisionSourceInstancesBin = BulletsUpdateCollisionSourceInstancesMapPrt(hero_bullets_update_data);
    hero_bullets_collition_grip_context.CollisionSourceTypesBin = BulletsCollisionSourceTypesMapPrt(hero_bullets);

    CollisionGridContext enemy_bullets_collition_grip_context;
    enemy_bullets_collition_grip_context.Root = enemy_bullets_collition_grip;
    enemy_bullets_collition_grip_context.CollisionSourceInstancesBin = BulletsUpdateCollisionSourceInstancesMapPrt(enemy_bullets_update_data);
    enemy_bullets_collition_grip_context.CollisionSourceTypesBin = BulletsCollisionSourceTypesMapPrt(enemy_bullets);

    f32 *time_delta_ptr          = GameStateTimeDeltaPrt(game_state);
    f64 *time_ptr                = GameStateTimePrt(game_state);
    f32 *play_time_ptr           = GameStatePlayTimePrt(game_state);
    u64 *frame_count_ptr         = GameStateFrameCounterPrt(game_state);
    u32 *state_ptr               = GameStateStatePrt(game_state);
    v2 *world_mouse_position_ptr = (v2 *)GameStateWorldMousePositionPrt(game_state);

    f32 game_aspect = game_area.x / game_area.y;

    LARGE_INTEGER freq, c1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c1);

    *state_ptr |= kGameStateStateReset;

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
            *time_delta_ptr = (f32)((f64)(c2.QuadPart - c1.QuadPart) / freq.QuadPart);
			c1 = c2;

            POINT mouseP;
            GetCursorPos(&mouseP);
            ScreenToClient(window, &mouseP);
            f32 mouseX = (f32)mouseP.x;
            f32 mouseY = (f32)((height - 1) - mouseP.y);

            FrameDataViewport viewport = *FrameDataViewportPrt(frame_data);

            f32 clip_space_mouseX = clamp_binormal_map_to_range(viewport.X, mouseX, viewport.X + viewport.Width);
            f32 clip_space_mouseY = clamp_binormal_map_to_range(viewport.Y, mouseY, viewport.Y + viewport.Height);

            *world_mouse_position_ptr = transform(matrix.inverse, V2(clip_space_mouseX, clip_space_mouseY));

            level_update(&level_update_context);
            wave_update(&wave_update_context);
            enemy_instances_update(&enemy_instances_context);
            hero_instances_update(&hero_instances_context);
            bullets_update(&enemy_bullets_update_context);
            bullets_update(&hero_bullets_update_context);

            enemy_instances_draw(&enemy_instances_draw_context);
            hero_instances_draw(&hero_instances_draw_context);
            bullets_draw(&enemy_bullets_draw_context);
            bullets_draw(&hero_bullets_draw_context);

            *time_ptr += *time_delta_ptr;
            *play_time_ptr += *time_delta_ptr;
            (*frame_count_ptr)++;
            *state_ptr &= ~kGameStateStateReset;

            //printf("%f\n", *time_delta_ptr);
		}

		EndFrameDirectX11(&directx_state, frame_data);
    }

	CloseMapFile(&hero_instances_map_data);
	CloseMapFile(&enemy_instances_map_data);
	CloseMapFile(&enemy_bullets_map_data);
	CloseMapFile(&hero_bullets_map_data);
	CloseMapFile(&frame_data_map_data);
	CloseMapFile(&enemy_bullets_update_map_data);
	CloseMapFile(&hero_bullets_update_map_data);
	CloseMapFile(&game_state_map_data);
	CloseMapFile(&wave_update_map_data);
	CloseMapFile(&level_update_map_data);
    CloseMapFile(&hero_bullets_collition_grip_map_data);
    CloseMapFile(&enemy_bullets_collition_grip_map_data);
    CloseMapFile(&hero_instances_collition_grip_map_data);
    CloseMapFile(&enemy_instances_collition_grip_map_data);
}
