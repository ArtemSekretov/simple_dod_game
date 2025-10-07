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

#include "collision_grid.h"
#include "collision_source_instances.h"
#include "collision_instances_damage.h"
#include "collision_source_radius.h"
#include "collision_source_damage.h"
#include "collision_damage.h"

#include "enemy_instances_update.c"
#include "enemy_instances_draw.c"

#include "hero_instances_update.c"
#include "hero_instances_draw.c"

#include "bullets_update.c"
#include "bullets_draw.c"
#include "level_update.c"
#include "wave_update.c"
#include "collision_grid_update.c"
#include "collision_damage_update.c"

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

    FrameDataViewport *viewport_prt      = FrameDataViewportPrt(frame_data);
    FrameDataScreenSize *screen_size_prt = FrameDataScreenSizePrt(frame_data);

    screen_size_prt->Width  = screen_width;
    screen_size_prt->Height = screen_height;
    *frame_data_count_prt   = 0;

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

#ifndef NDEBUG
static void
collision_grid_print_draw(CollisionGridContext *context_a, CollisionGridContext *context_b)
{
    CollisionGrid *a_collision_grid = context_a->Root;

    CollisionGridGridRowCount *a_collision_grid_row_count = CollisionGridGridRowCountPrt(a_collision_grid);
    CollisionGridGridRows *a_collision_grid_rows = CollisionGridGridRowsPrt(a_collision_grid);

    CollisionGrid *b_collision_grid = context_b->Root;

    CollisionGridGridRowCount *b_collision_grid_row_count = CollisionGridGridRowCountPrt(b_collision_grid);
    CollisionGridGridRows *b_collision_grid_rows = CollisionGridGridRowsPrt(b_collision_grid);
    
    printf("\033[0;0H");

    for (s32 row_index = 0; row_index < kCollisionGridRowCount; row_index++)
    {
        printf("%02d ", row_index);

        {
            u8 count = a_collision_grid_row_count->GridRowCount[row_index];
            if (count > kCollisionGridColCount)
            {
                printf("\033[1;31m");
            }
            else
            {
                printf("\033[34m");
            }
            printf("%02x ", count);

            printf("\033[32m");
            s32 col_index = 0;
            for (; col_index < count; col_index++)
            {
                u8 source_index = a_collision_grid_rows->GridRows[(row_index * kCollisionGridColCount) + col_index];
                printf("%02x ", source_index);
            }
            printf("\033[37m");
            for (; col_index < kCollisionGridColCount; col_index++)
            {
                u8 source_index = a_collision_grid_rows->GridRows[(row_index * kCollisionGridColCount) + col_index];
                printf("%02x ", source_index);
            }
        }

        printf("  ");

        {
            u8 count = b_collision_grid_row_count->GridRowCount[row_index];
            if (count > kCollisionGridColCount)
            {
                printf("\033[1;31m");
            }
            else
            {
                printf("\033[34m");
            }
            printf("%02x ", count);

            printf("\033[32m");
            s32 col_index = 0;
            for (; col_index < count; col_index++)
            {
                u8 source_index = b_collision_grid_rows->GridRows[(row_index * kCollisionGridColCount) + col_index];
                printf("%02x ", source_index);
            }
            printf("\033[37m");
            for (; col_index < kCollisionGridColCount; col_index++)
            {
                u8 source_index = b_collision_grid_rows->GridRows[(row_index * kCollisionGridColCount) + col_index];
                printf("%02x ", source_index);
            }
        }

        printf("\n");
    }
}

static void
collision_damage_print_draw(CollisionDamageContext *context)
{
    CollisionDamage *collision_damage_bin = context->Root;

    CollisionDamageDamageEvents *collision_damage_damage_events_sheet = CollisionDamageDamageEventsPrt(collision_damage_bin);
    f32 *damage_events_time_prt = CollisionDamageDamageEventsTimePrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u8 *damage_events_a_source_instance_index_prt = CollisionDamageDamageEventsASourceInstanceIndexPrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u8 *damage_events_b_source_instance_index_prt = CollisionDamageDamageEventsBSourceInstanceIndexPrt(collision_damage_bin, collision_damage_damage_events_sheet);

    CollisionDamageAccumulatedDamage *collision_damage_accumulated_damage_sheet = CollisionDamageAccumulatedDamagePrt(collision_damage_bin);
    u16 *accumulated_damage_a_value_prt = CollisionDamageAccumulatedDamageAValuePrt(collision_damage_bin, collision_damage_accumulated_damage_sheet);
    u16 *accumulated_damage_b_value_prt = CollisionDamageAccumulatedDamageBValuePrt(collision_damage_bin, collision_damage_accumulated_damage_sheet);

    u16 *a_damage_value_prt = CollisionDamageDamageEventsAValuePrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u16 *b_damage_value_prt = CollisionDamageDamageEventsBValuePrt(collision_damage_bin, collision_damage_damage_events_sheet);

    printf("\033[0;0H");
    
    for (s32 event_index = 0; event_index < kCollisionDamageMaxDamageEventCount / 2; event_index++)
    {
        s32 a_col_index = event_index;
        s32 b_col_index = event_index + kCollisionDamageMaxDamageEventCount / 2;

        printf("\033[1;35m");

        printf("%5.2f ", damage_events_time_prt[a_col_index]);

        printf("\033[37m");

        u16 a_col_a_source_index = damage_events_a_source_instance_index_prt[a_col_index];
        u16 a_col_b_source_index = damage_events_b_source_instance_index_prt[a_col_index];

        printf("%02x %02x", a_col_a_source_index, a_col_b_source_index);

        printf(" ");

        printf("%04x %04x", a_damage_value_prt[a_col_index], b_damage_value_prt[a_col_index]);

        //printf(" ");
        
        //printf("%04x %04x", accumulated_damage_a_value_prt[a_col_a_source_index], accumulated_damage_b_value_prt[a_col_b_source_index]);

        printf("  ");

        printf("\033[1;35m");

        printf("%5.2f ", damage_events_time_prt[b_col_index]);

        printf("\033[37m");

        u16 b_col_a_source_index = damage_events_a_source_instance_index_prt[b_col_index];
        u16 b_col_b_source_index = damage_events_b_source_instance_index_prt[b_col_index];

        printf("%02x %02x", b_col_a_source_index, b_col_b_source_index);

        printf(" ");

        //printf("%04x %04x", a_damage_value_prt[b_col_index], b_damage_value_prt[b_col_index]);

        //printf(" ");

        printf("%04x %04x", accumulated_damage_a_value_prt[b_col_a_source_index], accumulated_damage_b_value_prt[b_col_b_source_index]);

        printf("\n");
    }

}
#endif

static void
collision_damage_draw(CollisionDamageContext *context, FrameData *frame_data)
{
    CollisionDamage *collision_damage_bin = context->Root;
    LevelUpdate *level_update_bin = context->LevelUpdateBin;

    CollisionDamageDamageEvents *collision_damage_damage_events_sheet = CollisionDamageDamageEventsPrt(collision_damage_bin);

    u16 damage_events_count = *CollisionDamageDamageEventsCountPrt(collision_damage_bin);
    u16 damage_events_capacity = *CollisionDamageDamageEventsCapacityPrt(collision_damage_bin);

    damage_events_count = min(damage_events_count, damage_events_capacity);

    v2 *a_damage_position_prt = (v2*)CollisionDamageDamageEventsAPositionPrt(collision_damage_bin, collision_damage_damage_events_sheet);
    v2 *b_damage_position_prt = (v2*)CollisionDamageDamageEventsBPositionPrt(collision_damage_bin, collision_damage_damage_events_sheet);

    f32 *damage_time_prt = CollisionDamageDamageEventsTimePrt(collision_damage_bin, collision_damage_damage_events_sheet);

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr = FrameDataFrameDataCountPrt(frame_data);
    u16 frame_data_capacity   = *FrameDataFrameDataCapacityPrt(frame_data);

    f32 level_time = *LevelUpdateTimePrt(level_update_bin);

    for (u16 damage_index = 0; damage_index < damage_events_count; damage_index++)
    {
        f32 damage_time = damage_time_prt[damage_index];

        f32 delta = fabsf(level_time - damage_time);

        if (delta < 0.25f)
        {
            {
                v2 damage_position = a_damage_position_prt[damage_index];
                u16 frame_data_count = (*frame_data_count_ptr) % frame_data_capacity;
                FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

                object_data->PositionAndScale[0] = damage_position.x;
                object_data->PositionAndScale[1] = damage_position.y;
                object_data->PositionAndScale[2] = 0.1f;

                object_data->Color[0] = 1.0f;
                object_data->Color[1] = 0.0f;
                object_data->Color[2] = 1.0f;

                (*frame_data_count_ptr)++;
            }

            {
                v2 damage_position = b_damage_position_prt[damage_index];
                u16 frame_data_count = (*frame_data_count_ptr) % frame_data_capacity;
                FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

                object_data->PositionAndScale[0] = damage_position.x;
                object_data->PositionAndScale[1] = damage_position.y;
                object_data->PositionAndScale[2] = 0.1f;

                object_data->Color[0] = 1.0f;
                object_data->Color[1] = 1.0f;
                object_data->Color[2] = 1.0f;

                (*frame_data_count_ptr)++;
            }
        }
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

    MapFileData hero_bullets_collision_grid_map_data = CreateMapFile("collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *hero_bullets_collision_grid       = (CollisionGrid *)hero_bullets_collision_grid_map_data.data;

    MapFileData enemy_bullets_collision_grid_map_data = CreateMapFile("collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *enemy_bullets_collision_grid       = (CollisionGrid *)enemy_bullets_collision_grid_map_data.data;

    MapFileData hero_instances_collision_grid_map_data = CreateMapFile("collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *hero_instances_collision_grid       = (CollisionGrid *)hero_instances_collision_grid_map_data.data;

    MapFileData enemy_instances_collision_grid_map_data = CreateMapFile("collision_grid.bin", MapFilePermitions_ReadWriteCopy);
    CollisionGrid *enemy_instances_collision_grid       = (CollisionGrid *)enemy_instances_collision_grid_map_data.data;

    MapFileData enemy_instances_vs_hero_bullets_collision_damage_map_data = CreateMapFile("collision_damage.bin", MapFilePermitions_ReadWriteCopy);
    CollisionDamage *enemy_instances_vs_hero_bullets_collision_damage     = (CollisionDamage *)enemy_instances_vs_hero_bullets_collision_damage_map_data.data;

    MapFileData hero_instances_vs_enemy_bullets_collision_damage_map_data = CreateMapFile("collision_damage.bin", MapFilePermitions_ReadWriteCopy);
    CollisionDamage *hero_instances_vs_enemy_bullets_collision_damage     = (CollisionDamage *)hero_instances_vs_enemy_bullets_collision_damage_map_data.data;

    BulletsUpdateContext enemy_bullets_update_context;
    enemy_bullets_update_context.Root                        = enemy_bullets_update_data;
    enemy_bullets_update_context.BulletsBin                  = enemy_bullets;
    enemy_bullets_update_context.BulletSourceInstancesBin    = EnemyInstancesBulletSourceInstancesMapPrt(enemy_instances);
    enemy_bullets_update_context.GameStateBin                = game_state;
    enemy_bullets_update_context.PlayClockBin                = WaveUpdatePlayClockMapPrt(wave_update_data);
    enemy_bullets_update_context.CollisionInstancesDamageBin = CollisionDamageBCollisionInstancesDamageMapPrt(hero_instances_vs_enemy_bullets_collision_damage);

    BulletsDrawContext enemy_bullets_draw_context;
    enemy_bullets_draw_context.BulletsBin       = enemy_bullets;
    enemy_bullets_draw_context.BulletsUpdateBin = enemy_bullets_update_data;
    enemy_bullets_draw_context.FrameDataBin     = frame_data;

    BulletsUpdateContext hero_bullets_update_context;
    hero_bullets_update_context.Root                        = hero_bullets_update_data;
    hero_bullets_update_context.BulletsBin                  = hero_bullets;
    hero_bullets_update_context.BulletSourceInstancesBin    = HeroInstancesBulletSourceInstancesMapPrt(hero_instances);
    hero_bullets_update_context.GameStateBin                = game_state;
    hero_bullets_update_context.PlayClockBin                = LevelUpdatePlayClockMapPrt(level_update_data);
    hero_bullets_update_context.CollisionInstancesDamageBin = CollisionDamageBCollisionInstancesDamageMapPrt(enemy_instances_vs_hero_bullets_collision_damage);

    BulletsDrawContext hero_bullets_draw_context;
    hero_bullets_draw_context.BulletsBin       = hero_bullets;
    hero_bullets_draw_context.BulletsUpdateBin = hero_bullets_update_data;
    hero_bullets_draw_context.FrameDataBin     = frame_data;

    EnemyInstancesContext enemy_instances_context;
    enemy_instances_context.Root           = enemy_instances;
    enemy_instances_context.GameStateBin   = game_state;
    enemy_instances_context.WaveUpdateBin  = wave_update_data;
    enemy_instances_context.LevelUpdateBin = level_update_data;
    enemy_instances_context.CollisionInstancesDamageBin = CollisionDamageACollisionInstancesDamageMapPrt(enemy_instances_vs_hero_bullets_collision_damage);

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
    hero_instances_context.CollisionInstancesDamageBin = CollisionDamageACollisionInstancesDamageMapPrt(hero_instances_vs_enemy_bullets_collision_damage);

    HeroInstancesDrawContext hero_instances_draw_context;
    hero_instances_draw_context.HeroInstancesBin = hero_instances;
    hero_instances_draw_context.FrameDataBin     = frame_data;

    CollisionGridContext hero_bullets_collision_grid_context;
    hero_bullets_collision_grid_context.Root = hero_bullets_collision_grid;
    hero_bullets_collision_grid_context.CollisionSourceInstancesBin = BulletsUpdateCollisionSourceInstancesMapPrt(hero_bullets_update_data);
    hero_bullets_collision_grid_context.CollisionSourceRadiusBin = BulletsCollisionSourceRadiusMapPrt(hero_bullets);

    CollisionGridContext enemy_bullets_collision_grid_context;
    enemy_bullets_collision_grid_context.Root = enemy_bullets_collision_grid;
    enemy_bullets_collision_grid_context.CollisionSourceInstancesBin = BulletsUpdateCollisionSourceInstancesMapPrt(enemy_bullets_update_data);
    enemy_bullets_collision_grid_context.CollisionSourceRadiusBin = BulletsCollisionSourceRadiusMapPrt(enemy_bullets);

    CollisionGridContext hero_instances_collision_grid_context;
    hero_instances_collision_grid_context.Root = hero_instances_collision_grid;
    hero_instances_collision_grid_context.CollisionSourceInstancesBin = HeroInstancesCollisionSourceInstancesMapPrt(hero_instances);
    hero_instances_collision_grid_context.CollisionSourceRadiusBin = HeroInstancesCollisionSourceRadiusMapPrt(hero_instances);

    CollisionGridContext enemy_instances_collision_grid_context;
    enemy_instances_collision_grid_context.Root = enemy_instances_collision_grid;
    enemy_instances_collision_grid_context.CollisionSourceInstancesBin = EnemyInstancesCollisionSourceInstancesMapPrt(enemy_instances);
    enemy_instances_collision_grid_context.CollisionSourceRadiusBin = EnemyInstancesCollisionSourceRadiusMapPrt(enemy_instances);

    CollisionDamageContext enemy_instances_vs_hero_bullets_collision_damage_context;
    enemy_instances_vs_hero_bullets_collision_damage_context.Root = enemy_instances_vs_hero_bullets_collision_damage;
    
    enemy_instances_vs_hero_bullets_collision_damage_context.ACollisionGridBin = enemy_instances_collision_grid;
    enemy_instances_vs_hero_bullets_collision_damage_context.ACollisionSourceInstancesBin = enemy_instances_collision_grid_context.CollisionSourceInstancesBin;
    enemy_instances_vs_hero_bullets_collision_damage_context.ACollisionSourceRadiusBin = enemy_instances_collision_grid_context.CollisionSourceRadiusBin;
    enemy_instances_vs_hero_bullets_collision_damage_context.ACollisionSourceDamageBin = EnemyInstancesCollisionSourceDamageMapPrt(enemy_instances);

    enemy_instances_vs_hero_bullets_collision_damage_context.BCollisionGridBin = hero_bullets_collision_grid;
    enemy_instances_vs_hero_bullets_collision_damage_context.BCollisionSourceInstancesBin = hero_bullets_collision_grid_context.CollisionSourceInstancesBin;
    enemy_instances_vs_hero_bullets_collision_damage_context.BCollisionSourceRadiusBin = hero_bullets_collision_grid_context.CollisionSourceRadiusBin;
    enemy_instances_vs_hero_bullets_collision_damage_context.BCollisionSourceDamageBin = BulletsCollisionSourceDamageMapPrt(hero_bullets);
    enemy_instances_vs_hero_bullets_collision_damage_context.LevelUpdateBin = level_update_data;

    CollisionDamageContext hero_instances_vs_enemy_bullets_collision_damage_context;
    hero_instances_vs_enemy_bullets_collision_damage_context.Root = hero_instances_vs_enemy_bullets_collision_damage;
    
    hero_instances_vs_enemy_bullets_collision_damage_context.ACollisionGridBin = hero_instances_collision_grid;
    hero_instances_vs_enemy_bullets_collision_damage_context.ACollisionSourceInstancesBin = hero_instances_collision_grid_context.CollisionSourceInstancesBin;
    hero_instances_vs_enemy_bullets_collision_damage_context.ACollisionSourceRadiusBin = hero_instances_collision_grid_context.CollisionSourceRadiusBin;
    hero_instances_vs_enemy_bullets_collision_damage_context.ACollisionSourceDamageBin = HeroInstancesCollisionSourceDamageMapPrt(hero_instances);

    hero_instances_vs_enemy_bullets_collision_damage_context.BCollisionGridBin = enemy_bullets_collision_grid;
    hero_instances_vs_enemy_bullets_collision_damage_context.BCollisionSourceInstancesBin = enemy_bullets_collision_grid_context.CollisionSourceInstancesBin;
    hero_instances_vs_enemy_bullets_collision_damage_context.BCollisionSourceRadiusBin = enemy_bullets_collision_grid_context.CollisionSourceRadiusBin;
    hero_instances_vs_enemy_bullets_collision_damage_context.BCollisionSourceDamageBin = BulletsCollisionSourceDamageMapPrt(enemy_bullets);
    hero_instances_vs_enemy_bullets_collision_damage_context.LevelUpdateBin = level_update_data;

    f32 *time_delta_ptr          = GameStateTimeDeltaPrt(game_state);
    f64 *time_ptr                = GameStateTimePrt(game_state);
    f32 *play_time_ptr           = GameStatePlayTimePrt(game_state);
    u64 *frame_count_ptr         = GameStateFrameCounterPrt(game_state);
    u32 *state_ptr               = GameStateStatePrt(game_state);
    v2 *world_mouse_position_ptr = (v2 *)GameStateWorldMousePositionPrt(game_state);

    u64 *hero_instances_live_ptr = HeroInstancesInstancesLivePrt(hero_instances);

    f32 game_aspect = game_area.x / game_area.y;

    LARGE_INTEGER freq, c1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c1);

    *state_ptr |= kGameStateReset|kGameStatePlayEnable;

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
            else if (msg.message == WM_KEYUP)
            {
                u32 VKCode = (u32)msg.wParam;
                
                //b32 AltKeyWasDown = (msg.lParam & (1 << 29));
                //b32 ShiftKeyWasDown = (GetKeyState(VK_SHIFT) & (1 << 15));
                
                // NOTE(casey): Since we are comparing WasDown to IsDown,
                // we MUST use == and != to convert these bit tests to actual
                // 0 or 1 values.
                b32 WasDown = ((msg.lParam & (1 << 30)) != 0);
                b32 IsDown = ((msg.lParam & (1UL << 31)) == 0);
                if (WasDown != IsDown)
                {
                    if(VKCode == ' ')
                    {
                        //if(IsDown)
                        {
                            if ((*hero_instances_live_ptr) == 0)
                            {
                                *state_ptr |= kGameStateReset|kGameStatePlayEnable;
                            }
                            else
                            {
                                *state_ptr ^= kGameStatePlayEnable;
                            }
                        }
                    }
                }

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

            if ((*state_ptr) & kGameStatePlayEnable)
            {
                POINT mouseP;
                GetCursorPos(&mouseP);
                ScreenToClient(window, &mouseP);
                f32 mouseX = (f32)mouseP.x;
                f32 mouseY = (f32)((height - 1) - mouseP.y);

                FrameDataViewport viewport = *FrameDataViewportPrt(frame_data);

                f32 clip_space_mouseX = clamp_binormal_map_to_range(viewport.X, mouseX, viewport.X + viewport.Width);
                f32 clip_space_mouseY = clamp_binormal_map_to_range(viewport.Y, mouseY, viewport.Y + viewport.Height);

                *world_mouse_position_ptr = transform(matrix.inverse, V2(clip_space_mouseX, clip_space_mouseY));

                *play_time_ptr += *time_delta_ptr;

                level_update(&level_update_context);
                wave_update(&wave_update_context);

                enemy_instances_update(&enemy_instances_context);
                hero_instances_update(&hero_instances_context);

                bullets_update(&enemy_bullets_update_context);
                bullets_update(&hero_bullets_update_context);

                collision_grid_update(&hero_bullets_collision_grid_context);
                collision_grid_update(&enemy_bullets_collision_grid_context);
                collision_grid_update(&hero_instances_collision_grid_context);
                collision_grid_update(&enemy_instances_collision_grid_context);

                collision_damage_update(&enemy_instances_vs_hero_bullets_collision_damage_context);
                collision_damage_update(&hero_instances_vs_enemy_bullets_collision_damage_context);

                *state_ptr &= ~kGameStateReset;
            }

            #ifndef NDEBUG
            //collision_damage_print_draw(&enemy_instances_vs_hero_bullets_collision_damage_context);
            //collision_grid_print_draw(&enemy_instances_collision_grid_context, &hero_bullets_collision_grid_context);
            #endif

            enemy_instances_draw(&enemy_instances_draw_context);
            hero_instances_draw(&hero_instances_draw_context);
            bullets_draw(&enemy_bullets_draw_context);
            bullets_draw(&hero_bullets_draw_context);

            collision_damage_draw(&enemy_instances_vs_hero_bullets_collision_damage_context, frame_data);
            collision_damage_draw(&hero_instances_vs_enemy_bullets_collision_damage_context, frame_data);

            *time_ptr += *time_delta_ptr;
            (*frame_count_ptr)++;

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
    CloseMapFile(&hero_bullets_collision_grid_map_data);
    CloseMapFile(&enemy_bullets_collision_grid_map_data);
    CloseMapFile(&hero_instances_collision_grid_map_data);
    CloseMapFile(&enemy_instances_collision_grid_map_data);
    CloseMapFile(&enemy_instances_vs_hero_bullets_collision_damage_map_data);
    CloseMapFile(&hero_instances_vs_enemy_bullets_collision_damage_map_data);
}
