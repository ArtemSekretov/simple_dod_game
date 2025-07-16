
static v2 
spawn_position(u8 index, EnemyInstancesSpawnPointsXYQ4 *spawn_points_xy_q4, v2 *enemy_instances_positions)
{
    if (index > 63)
    {
        return enemy_instances_positions[index - 64];
    }

    EnemyInstancesSpawnPointsXYQ4 spawn_point_xy_q4 = spawn_points_xy_q4[index];

    f32 spawn_x = ((f32)spawn_point_xy_q4.XQ4) * kQ4ToFloat;
    f32 spawn_y = ((f32)spawn_point_xy_q4.YQ4) * kQ4ToFloat;

    return V2(spawn_x, spawn_y);
}

static void
enemy_instances_spawn(EnemyInstancesUpdateContext *context)
{
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet  = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesSpawnPoints    *spawn_points_sheet     = EnemyInstancesSpawnPointsPrt(enemy_instances);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    u16 *enemy_instances_start_time_q4          = EnemyInstancesEnemyInstancesStartTimeQ4Prt(enemy_instances, enemy_instances_sheet);
    u8  *enemy_instances_flat_spawn_point_index = EnemyInstancesEnemyInstancesFlatSpawnPointIndexPrt(enemy_instances, enemy_instances_sheet);

    EnemyInstancesSpawnPointsXYQ4 *spawn_points_xy_q4 = EnemyInstancesSpawnPointsXYQ4Prt(enemy_instances, spawn_points_sheet);

    EnemyInstancesUpdateEnemyPositions *enemy_instances_update_positions_sheet = EnemyInstancesUpdateEnemyPositionsPrt(enemy_instances_update);
    uint8_t *enemy_instances_way_point_index                                   = EnemyInstancesUpdateEnemyPositionsWayPointIndexPrt(enemy_instances_update, enemy_instances_update_positions_sheet);
    v2 *enemy_instances_positions                                              = (v2 *)EnemyInstancesUpdateEnemyPositionsPositionsPrt(enemy_instances_update, enemy_instances_update_positions_sheet);

    u16 *enemy_positions_count_prt = EnemyInstancesUpdateEnemyPositionsCountPrt(enemy_instances_update);

    while ((*enemy_positions_count_prt) < wave_instance.EnemyInstancesCount)
    {
        u16 wave_instance_index = (*enemy_positions_count_prt);
        
        u16 enemy_instances_index = wave_instance.EnemyInstancesStartIndex + wave_instance_index;

        u16 start_time_q4 = enemy_instances_start_time_q4[enemy_instances_index];
        
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;

        if (enemy_instances_update->WaveTime < start_time)
        {
            break;
        }

        u8 flat_spawn_point_index = enemy_instances_flat_spawn_point_index[enemy_instances_index];

        v2 spawn_point = spawn_position(flat_spawn_point_index, spawn_points_xy_q4, enemy_instances_positions);

        enemy_instances_positions[wave_instance_index] = spawn_point;
        enemy_instances_way_point_index[wave_instance_index] = 0;
        enemy_instances_update->InstancesLive |= 1ULL << wave_instance_index;

        enemy_instances_update->WaveSpawnLastTime = start_time;
        (*enemy_positions_count_prt)++;
    }
}

static void
enemy_instances_move(EnemyInstancesUpdateContext *context)
{
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet          = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet           = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesEnemyTypes *enemy_sheet                         = EnemyInstancesEnemyTypesPrt(enemy_instances);
    EnemyInstancesWayPointPathsIndex *way_point_paths_index_sheet = EnemyInstancesWayPointPathsIndexPrt(enemy_instances);
    EnemyInstancesWayPoints *way_points_sheet                     = EnemyInstancesWayPointsPrt(enemy_instances);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    u16 *enemy_instances_start_time_q4                                   = EnemyInstancesEnemyInstancesStartTimeQ4Prt(enemy_instances, enemy_instances_sheet);
    u8 *enemy_instances_enemy_index                                      = EnemyInstancesEnemyInstancesEnemyIndexPrt(enemy_instances, enemy_instances_sheet);
    s8 *enemy_instance_way_point_path_index                              = EnemyInstancesEnemyInstancesWayPointPathIndexPrt(enemy_instances, enemy_instances_sheet);
    u8 *enemy_movement_speed_q4                                          = EnemyInstancesEnemyTypesMovementSpeedQ4Prt(enemy_instances, enemy_sheet);
    EnemyInstancesWayPointsRedusedXYQ4 *way_points                       = EnemyInstancesWayPointsRedusedXYQ4Prt(enemy_instances, way_points_sheet);
    EnemyInstancesWayPointPathsIndexWayPointPaths *way_point_paths_index = EnemyInstancesWayPointPathsIndexWayPointPathsPrt(enemy_instances, way_point_paths_index_sheet);
    u8 *enemy_instance_way_point_time_out_q4                             = EnemyInstancesWayPointPathsIndexTimeOutQ4Prt(enemy_instances, way_point_paths_index_sheet);

    EnemyInstancesUpdateEnemyPositions *enemy_instances_update_positions_sheet = EnemyInstancesUpdateEnemyPositionsPrt(enemy_instances_update);
    uint8_t *enemy_instances_way_point_index                                   = EnemyInstancesUpdateEnemyPositionsWayPointIndexPrt(enemy_instances_update, enemy_instances_update_positions_sheet);
    v2 *enemy_instances_positions                                              = (v2 *)EnemyInstancesUpdateEnemyPositionsPositionsPrt(enemy_instances_update, enemy_instances_update_positions_sheet);

    u16 enemy_positions_count = *EnemyInstancesUpdateEnemyPositionsCountPrt(enemy_instances_update);

    for (u8 wave_instance_index = 0; wave_instance_index < enemy_positions_count; wave_instance_index++)
    {
        if ((enemy_instances_update->InstancesLive & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u16 enemy_instances_index = wave_instance.EnemyInstancesStartIndex + wave_instance_index;
        u8 enemy_index = enemy_instances_enemy_index[enemy_instances_index];

        u16 start_time_q4 = enemy_instances_start_time_q4[enemy_instances_index];
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;
        f32 enemy_instance_time = enemy_instances_update->WaveTime - start_time;

        u8  movement_speed_q4 = enemy_movement_speed_q4[enemy_index];
        f32 movement_speed = ((f32)movement_speed_q4) * kQ4ToFloat;
        f32 frame_move_dist = movement_speed * game_state->TimeDelta;

        s8 way_point_path_id = enemy_instance_way_point_path_index[enemy_instances_index];
        u8 way_point_path_index = ((u8)abs(way_point_path_id)) + (game_state->PlayerGridX & (way_point_path_id < 0));

        EnemyInstancesWayPointPathsIndexWayPointPaths way_point_path_index_way_point = way_point_paths_index[way_point_path_index];
        
        u8 way_point_time_out_q4 = enemy_instance_way_point_time_out_q4[way_point_path_index];
        f32 way_point_time_out   = ((f32)way_point_time_out_q4) * kQ4ToFloat;

        for(u8 i = 0; i < 4; i++)
        {
            u8 way_point_index = enemy_instances_way_point_index[wave_instance_index];

            EnemyInstancesWayPointsRedusedXYQ4 way_point_xy_q4 = way_points[way_point_path_index_way_point.WayPointStartIndex + way_point_index];

            f32 way_point_x = ((f32)way_point_xy_q4.RedusedXQ4) * kQ4ToFloat;
            f32 way_point_y = ((f32)way_point_xy_q4.RedusedYQ4) * kQ4ToFloat;

            v2 way_point        = V2(way_point_x, way_point_y);
            v2 current_position = enemy_instances_positions[wave_instance_index];

            v2  way_point_v         = v2_sub(way_point, current_position);
            f32 way_point_dist      = v2_length(way_point_v);
            f32 way_point_move_dist = fminf(way_point_dist, frame_move_dist);

            if (way_point_dist > 0.0f)
            {
                v2 way_point_move_v = v2_scale(way_point_v, way_point_move_dist / way_point_dist);
                v2 next_position = v2_add(current_position, way_point_move_v);

                enemy_instances_positions[wave_instance_index] = next_position;
            }

            if (way_point_dist > frame_move_dist)
            {
                break;
            }

            frame_move_dist -= way_point_dist;

            s32 is_end_of_path = way_point_index == (way_point_path_index_way_point.WayPointCount - 1);
            s32 is_time_out    = enemy_instance_time > way_point_time_out;

            if (is_end_of_path && is_time_out)
            {
                enemy_instances_update->InstancesLive &= ~(1ULL << wave_instance_index);
                break;
            }
            else if (is_end_of_path)
            {
                break;
            }

            enemy_instances_way_point_index[wave_instance_index]++;
        }
    }
}

static void
enemy_instances_next_wave(EnemyInstancesUpdateContext *context)
{
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];
    
    u16 *enemy_positions_count_prt = EnemyInstancesUpdateEnemyPositionsCountPrt(enemy_instances_update);

    if ((*enemy_positions_count_prt) != wave_instance.EnemyInstancesCount)
    {
        return;
    }

    if (enemy_instances_update->InstancesLive)
    {
        return;
    }

    enemy_instances_update->WaveIndex++;
    enemy_instances_update->WaveSpawnLastTime   = 0.0f;
    enemy_instances_update->WaveTime            = 0.0f;
    *enemy_positions_count_prt = 0;
}

static void
enemy_instances_update(EnemyInstancesUpdateContext *context)
{  
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    b32 is_empty_wave = wave_instance.EnemyInstancesCount == 0;
    b32 is_all_waves_complete = enemy_instances_update->WaveIndex == kEnemyInstancesMaxWavesPerLevel;

    if (is_empty_wave || is_all_waves_complete)
    {
        return;
    }

    enemy_instances_spawn(context);
    enemy_instances_move(context);

    enemy_instances_update->WaveTime += game_state->TimeDelta;
    enemy_instances_next_wave(context);
}
