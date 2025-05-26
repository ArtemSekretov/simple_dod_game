
v2  g_enemy_instances_positions[64]       = { 0 };
u8  g_enemy_instances_way_point_index[64] = { 0 };
u64 g_enemy_instances_live                = 0;

u8  g_level_index = 0;

u8  g_wave_index                   = 0;
u8  g_wave_instances_spawned_count = 0;
f32 g_wave_spawn_last_time         = 0.0f;
f32 g_wave_time                    = 0.0f;

u8  g_player_grid_x = 0;

static v2 
spawn_position(u8 index, EnemyInstancesSpawnPointsXYQ4 *spawn_points_xy_q4)
{
    if (index > 63)
    {
        return g_enemy_instances_positions[index - 64];
    }

    EnemyInstancesSpawnPointsXYQ4 spawn_point_xy_q4 = spawn_points_xy_q4[index];

    f32 spawn_x = ((f32)spawn_point_xy_q4.XQ4) * kQ4ToFloat;
    f32 spawn_y = ((f32)spawn_point_xy_q4.YQ4) * kQ4ToFloat;

    return V2(spawn_x, spawn_y);
}

static void
enemy_instances_spawn(EnemyInstances* enemy_instances)
{
    u8 flat_wave_index = (g_level_index << 2) + g_wave_index;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet  = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesSpawnPoints    *spawn_points_sheet     = EnemyInstancesSpawnPointsPrt(enemy_instances);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    u16 *enemy_instances_start_time_q4          = EnemyInstancesEnemyInstancesStartTimeQ4Prt(enemy_instances, enemy_instances_sheet);
    u8  *enemy_instances_flat_spawn_point_index = EnemyInstancesEnemyInstancesFlatSpawnPointIndexPrt(enemy_instances, enemy_instances_sheet);

    EnemyInstancesSpawnPointsXYQ4 *spawn_points_xy_q4 = EnemyInstancesSpawnPointsXYQ4Prt(enemy_instances, spawn_points_sheet);

    while (g_wave_instances_spawned_count < wave_instance.EnemyInstancesCount)
    {
        u8 wave_instance_index = g_wave_instances_spawned_count;
        
        u16 enemy_instances_index = wave_instance.EnemyInstancesStartIndex + wave_instance_index;

        u16 start_time_q4 = enemy_instances_start_time_q4[enemy_instances_index];
        
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;

        if (g_wave_time < start_time)
        {
            break;
        }

        u8 flat_spawn_point_index = enemy_instances_flat_spawn_point_index[enemy_instances_index];

        v2 spawn_point = spawn_position(flat_spawn_point_index, spawn_points_xy_q4);

        g_enemy_instances_positions[wave_instance_index] = spawn_point;
        g_enemy_instances_way_point_index[wave_instance_index] = 0;
        g_enemy_instances_live |= 1ULL << wave_instance_index;

        g_wave_spawn_last_time = start_time;
        g_wave_instances_spawned_count++;
    }
}

static void
enemy_instances_move(EnemyInstances* enemy_instances, f32 delta)
{
    u8 flat_wave_index = (g_level_index << 2) + g_wave_index;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet          = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet           = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesEnemyTypes *enemy_sheet                         = EnemyInstancesEnemyTypesPrt(enemy_instances);
    EnemyInstancesWayPointPathsIndex *way_point_paths_index_sheet = EnemyInstancesWayPointPathsIndexPrt(enemy_instances);
    EnemyInstancesWayPoints *way_points_sheet                     = EnemyInstancesWayPointsPrt(enemy_instances);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    u8 *enemy_instances_enemy_index = EnemyInstancesEnemyInstancesEnemyIndexPrt(enemy_instances, enemy_instances_sheet);

    u8 *enemy_movement_speed_q4 = EnemyInstancesEnemyTypesMovementSpeedQ4Prt(enemy_instances, enemy_sheet);

    s8 *enemy_instance_way_point_path_index = EnemyInstancesEnemyInstancesWayPointPathIndexPrt(enemy_instances, enemy_instances_sheet);

    EnemyInstancesWayPointPathsIndexWayPointPaths *way_point_paths_index = EnemyInstancesWayPointPathsIndexWayPointPathsPrt(enemy_instances, way_point_paths_index_sheet);

    EnemyInstancesWayPointsRedusedXYQ4 *way_points = EnemyInstancesWayPointsRedusedXYQ4Prt(enemy_instances, way_points_sheet);

    for (u8 wave_instance_index = 0; wave_instance_index < g_wave_instances_spawned_count; wave_instance_index++)
    {
        if ((g_enemy_instances_live & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u16 enemy_instances_index = wave_instance.EnemyInstancesStartIndex + wave_instance_index;
        u8 enemy_index = enemy_instances_enemy_index[enemy_instances_index];

        u8  movement_speed_q4 = enemy_movement_speed_q4[enemy_index];
        f32 movement_speed = ((f32)movement_speed_q4) * kQ4ToFloat;
        f32 frame_move_dist = movement_speed * delta;

        s8 way_point_path_id = enemy_instance_way_point_path_index[enemy_instances_index];
        u8 way_point_path_index = ((u8)abs(way_point_path_id)) + (g_player_grid_x & (way_point_path_id < 0));

        EnemyInstancesWayPointPathsIndexWayPointPaths way_point_path_index_way_point = way_point_paths_index[way_point_path_index];

        while (1)
        {
            u8 way_point_index = g_enemy_instances_way_point_index[wave_instance_index];

            EnemyInstancesWayPointsRedusedXYQ4 way_point_xy_q4 = way_points[way_point_path_index_way_point.WayPointStartIndex + way_point_index];

            f32 way_point_x = ((f32)way_point_xy_q4.RedusedXQ4) * kQ4ToFloat;
            f32 way_point_y = ((f32)way_point_xy_q4.RedusedYQ4) * kQ4ToFloat;

            v2 way_point        = V2(way_point_x, way_point_y);
            v2 current_position = g_enemy_instances_positions[wave_instance_index];

            v2  way_point_v         = v2_sub(way_point, current_position);
            f32 way_point_dist      = v2_length(way_point_v);
            f32 way_point_move_dist = fminf(way_point_dist, frame_move_dist);
            v2  way_point_move_v    = v2_scale(way_point_v, way_point_move_dist / way_point_dist);
            v2  next_position       = v2_add(current_position, way_point_move_v);

            g_enemy_instances_positions[wave_instance_index] = next_position;

            if (way_point_dist > frame_move_dist)
            {
                break;
            }

            frame_move_dist -= way_point_dist;

            if (way_point_index == (way_point_path_index_way_point.WayPointCount - 1))
            {
                g_enemy_instances_live &= ~(1ULL << wave_instance_index);
                break;
            }

            g_enemy_instances_way_point_index[wave_instance_index]++;
        }
    }
}

static void
enemy_instances_next_wave(EnemyInstances* enemy_instances)
{
    u8 flat_wave_index = (g_level_index << 2) + g_wave_index;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];
    
    if (g_wave_instances_spawned_count != wave_instance.EnemyInstancesCount)
    {
        return;
    }

    if (g_enemy_instances_live)
    {
        return;
    }

    g_wave_index++;
    g_wave_spawn_last_time         = 0.0f;
    g_wave_time                    = 0.0f;
    g_wave_instances_spawned_count = 0;
}

static void
enemy_instances_update(EnemyInstances* enemy_instances, f32 delta)
{  
    u8 flat_wave_index = (g_level_index << 2) + g_wave_index;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    b32 is_empty_wave = wave_instance.EnemyInstancesCount == 0;
    b32 is_all_waves_complete = g_wave_index == kEnemyInstancesMaxWavesPerLevel;

    if (is_empty_wave || is_all_waves_complete)
    {
        return;
    }

    enemy_instances_spawn(enemy_instances);
    enemy_instances_move(enemy_instances, delta);

    g_wave_time += delta;
    enemy_instances_next_wave(enemy_instances);
}
