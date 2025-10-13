
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
enemy_instances_spawn(EnemyInstancesContext *context)
{
    EnemyInstances *enemy_instances          = context->Root;
    WaveUpdate *wave_update                  = context->WaveUpdateBin;
    EnemyInstancesWave *enemy_instances_wave = EnemyInstancesEnemyInstancesWaveMapPrt(enemy_instances);

    f32 wave_time = *WaveUpdateTimePrt(wave_update);

    EnemyInstancesSpawnPoints *spawn_points_sheet = EnemyInstancesSpawnPointsPrt(enemy_instances);

    EnemyInstancesWaveEnemyInstances *enemy_instances_wave_sheet = EnemyInstancesWaveEnemyInstancesPrt(enemy_instances_wave);

    u16 *enemy_instances_start_time_q4          = EnemyInstancesWaveEnemyInstancesStartTimeQ4Prt(enemy_instances_wave, enemy_instances_wave_sheet);
    u8  *enemy_instances_flat_spawn_point_index = EnemyInstancesWaveEnemyInstancesFlatSpawnPointIndexPrt(enemy_instances_wave, enemy_instances_wave_sheet);

    EnemyInstancesSpawnPointsXYQ4 *spawn_points_xy_q4 = EnemyInstancesSpawnPointsXYQ4Prt(enemy_instances, spawn_points_sheet);

    EnemyInstancesEnemyPositions *enemy_instances_positions_sheet = EnemyInstancesEnemyPositionsPrt(enemy_instances);
    uint8_t *enemy_instances_way_point_index                      = EnemyInstancesEnemyPositionsWayPointIndexPrt(enemy_instances, enemy_instances_positions_sheet);
    v2 *enemy_instances_positions                                 = (v2 *)EnemyInstancesEnemyPositionsPositionsPrt(enemy_instances, enemy_instances_positions_sheet);

    u16 *enemy_positions_count_prt = EnemyInstancesEnemyPositionsCountPrt(enemy_instances);
    u64 *instances_live_ptr        = EnemyInstancesInstancesLivePrt(enemy_instances);
    u64 *instances_reset_prt       = EnemyInstancesInstancesResetPrt(enemy_instances);

    u16 enemy_instances_wave_count    = *EnemyInstancesWaveEnemyInstancesCountPrt(enemy_instances_wave);
    u16 enemy_instances_wave_capacity = *EnemyInstancesWaveEnemyInstancesCapacityPrt(enemy_instances_wave);

    enemy_instances_wave_count = min(enemy_instances_wave_count, enemy_instances_wave_capacity);

    while ((*enemy_positions_count_prt) < enemy_instances_wave_count)
    {
        u16 wave_instance_index = (*enemy_positions_count_prt);
        
        u16 start_time_q4 = enemy_instances_start_time_q4[wave_instance_index];
        
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;

        if (wave_time < start_time)
        {
            break;
        }

        u8 flat_spawn_point_index = enemy_instances_flat_spawn_point_index[wave_instance_index];

        v2 spawn_point = spawn_position(flat_spawn_point_index, spawn_points_xy_q4, enemy_instances_positions);

        enemy_instances_positions[wave_instance_index] = spawn_point;
        enemy_instances_way_point_index[wave_instance_index] = 0;
        *instances_live_ptr |= 1ULL << wave_instance_index;
        *instances_reset_prt |= 1ULL << wave_instance_index;

        (*enemy_positions_count_prt)++;
    }
}

static void
enemy_instances_move(EnemyInstancesContext *context)
{
    EnemyInstances *enemy_instances          = context->Root;
    GameState *game_state                    = context->GameStateBin;
    WaveUpdate *wave_update                  = context->WaveUpdateBin;
    EnemyInstancesWave *enemy_instances_wave = EnemyInstancesEnemyInstancesWaveMapPrt(enemy_instances);
    CollisionInstancesDamage *collision_instances_damage_bin = context->CollisionInstancesDamageBin;

    f32 time_delta   = *GameStateTimeDeltaPrt(game_state);
    u8 player_grid_x = *GameStatePlayerGridXPrt(game_state);

    f32 wave_time = *WaveUpdateTimePrt(wave_update);

    EnemyInstancesEnemyTypes *enemy_sheet                         = EnemyInstancesEnemyTypesPrt(enemy_instances);
    EnemyInstancesWayPointPathsIndex *way_point_paths_index_sheet = EnemyInstancesWayPointPathsIndexPrt(enemy_instances);
    EnemyInstancesWayPoints *way_points_sheet                     = EnemyInstancesWayPointsPrt(enemy_instances);
    EnemyInstancesWaveEnemyInstances *enemy_instances_wave_sheet  = EnemyInstancesWaveEnemyInstancesPrt(enemy_instances_wave);

    u16 *enemy_instances_start_time_q4      = EnemyInstancesWaveEnemyInstancesStartTimeQ4Prt(enemy_instances_wave, enemy_instances_wave_sheet);
    u8 *enemy_instances_enemy_index         = EnemyInstancesWaveEnemyInstancesEnemyIndexPrt(enemy_instances_wave, enemy_instances_wave_sheet);
    s8 *enemy_instance_way_point_path_index = EnemyInstancesWaveEnemyInstancesWayPointPathIndexPrt(enemy_instances_wave, enemy_instances_wave_sheet);

    u8 *enemy_movement_speed_q4 = EnemyInstancesEnemyTypesMovementSpeedQ4Prt(enemy_instances, enemy_sheet);

    EnemyInstancesWayPointsRedusedXYQ4 *way_points = EnemyInstancesWayPointsRedusedXYQ4Prt(enemy_instances, way_points_sheet);

    EnemyInstancesWayPointPathsIndexWayPointPaths *way_point_paths_index = EnemyInstancesWayPointPathsIndexWayPointPathsPrt(enemy_instances, way_point_paths_index_sheet);
    u8 *enemy_instance_way_point_time_out_q4                             = EnemyInstancesWayPointPathsIndexTimeOutQ4Prt(enemy_instances, way_point_paths_index_sheet);

    EnemyInstancesEnemyPositions *enemy_instances_positions_sheet = EnemyInstancesEnemyPositionsPrt(enemy_instances);
    uint8_t *enemy_instances_way_point_index                      = EnemyInstancesEnemyPositionsWayPointIndexPrt(enemy_instances, enemy_instances_positions_sheet);
    v2 *enemy_instances_positions                                 = (v2 *)EnemyInstancesEnemyPositionsPositionsPrt(enemy_instances, enemy_instances_positions_sheet);

    u16 enemy_positions_count    = *EnemyInstancesEnemyPositionsCountPrt(enemy_instances);
    u16 enemy_positions_capacity = *EnemyInstancesEnemyPositionsCapacityPrt(enemy_instances);
    u64 *instances_live_ptr   = EnemyInstancesInstancesLivePrt(enemy_instances);
    u64 *instances_reset_prt  = EnemyInstancesInstancesResetPrt(enemy_instances);

    u16 *enemy_types_health_prt = EnemyInstancesEnemyTypesHealthPrt(enemy_instances, enemy_sheet);

    CollisionInstancesDamageInstances *collision_instances_damage_instances_sheet = CollisionInstancesDamageInstancesPrt(collision_instances_damage_bin);
    u16 *instances_damage_prt = CollisionInstancesDamageInstancesDamagePrt(collision_instances_damage_bin, collision_instances_damage_instances_sheet);

    enemy_positions_count = min(enemy_positions_count, enemy_positions_capacity);

    for (u8 wave_instance_index = 0; wave_instance_index < enemy_positions_count; wave_instance_index++)
    {
        if (((*instances_live_ptr) & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        *instances_reset_prt &= ~(1ULL << wave_instance_index);

        u8 enemy_index = enemy_instances_enemy_index[wave_instance_index];

        u16 health = enemy_types_health_prt[enemy_index];
        u16 damage = instances_damage_prt[wave_instance_index];

        if (damage > health)
        {
            *instances_live_ptr &= ~(1ULL << wave_instance_index);
            continue;
        }

        u16 start_time_q4 = enemy_instances_start_time_q4[wave_instance_index];
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;
        f32 enemy_instance_time = wave_time - start_time;

        u8  movement_speed_q4 = enemy_movement_speed_q4[enemy_index];
        f32 movement_speed = ((f32)movement_speed_q4) * kQ4ToFloat;
        f32 frame_move_dist = movement_speed * time_delta;

        s8 way_point_path_id = enemy_instance_way_point_path_index[wave_instance_index];
        u8 way_point_path_index = ((u8)abs(way_point_path_id)) + (player_grid_x * (way_point_path_id < 0));

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

            b32 is_end_of_path = way_point_index == (way_point_path_index_way_point.WayPointCount - 1);
            b32 is_time_out    = enemy_instance_time > way_point_time_out;

            if (is_end_of_path && is_time_out)
            {
                *instances_live_ptr &= ~(1ULL << wave_instance_index);
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
enemy_instances_update(EnemyInstancesContext *context)
{  
    EnemyInstances *enemy_instances = context->Root;
    LevelUpdate *level_update       = context->LevelUpdateBin;
    WaveUpdate *wave_update         = context->WaveUpdateBin;

    u8 level_index = *LevelUpdateIndexPrt(level_update);

    u8 wave_index  = *WaveUpdateIndexPrt(wave_update);
    u32 wave_state = *WaveUpdateStatePrt(wave_update);

    u8 flat_wave_index = (level_index << 2) + wave_index;

    u32 *wave_state_ptr            = EnemyInstancesWaveStatePrt(enemy_instances);
    u16 *enemy_positions_count_prt = EnemyInstancesEnemyPositionsCountPrt(enemy_instances);

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave *wave_instance = &level_wave_index_instance[flat_wave_index];

    if (wave_state & kWaveUpdateStateReset)
    {
        EnemyInstancesWave *enemy_instances_wave              = EnemyInstancesEnemyInstancesWaveMapPrt(enemy_instances);
        BulletSourceInstances *enemy_bullets_source_instances = EnemyInstancesBulletSourceInstancesMapPrt(enemy_instances);
        CollisionSourceInstances *collision_source_instances  = EnemyInstancesCollisionSourceInstancesMapPrt(enemy_instances);

        EnemyInstancesEnemyInstances *enemy_instances_sheet = EnemyInstancesEnemyInstancesPrt(enemy_instances);

        u16 *enemy_instances_start_time_q4         = EnemyInstancesEnemyInstancesStartTimeQ4Prt(enemy_instances, enemy_instances_sheet);
        u8 *enemy_instances_enemy_index            = EnemyInstancesEnemyInstancesEnemyIndexPrt(enemy_instances, enemy_instances_sheet);
        u8 *enemy_instances_flat_spawn_point_index = EnemyInstancesEnemyInstancesFlatSpawnPointIndexPrt(enemy_instances, enemy_instances_sheet);
        s8 *enemy_instance_way_point_path_index    = EnemyInstancesEnemyInstancesWayPointPathIndexPrt(enemy_instances, enemy_instances_sheet);

        enemy_instances_wave->EnemyInstancesCountOffset    = (u16)(((uintptr_t)&wave_instance->EnemyInstancesCount) - ((uintptr_t)enemy_instances_wave));
        enemy_instances_wave->EnemyInstancesCapacityOffset = (u16)(((uintptr_t)&wave_instance->EnemyInstancesCount) - ((uintptr_t)enemy_instances_wave));
        enemy_bullets_source_instances->SourceInstancesCountOffset   = (u16)(((uintptr_t)&wave_instance->EnemyInstancesCount) - ((uintptr_t)enemy_bullets_source_instances));
        enemy_bullets_source_instances->SourceInstancesCapacityOffset = (u16)(((uintptr_t)&wave_instance->EnemyInstancesCount) - ((uintptr_t)enemy_bullets_source_instances));
        
        CollisionSourceInstancesSourceInstances *collision_source_instances_source = CollisionSourceInstancesSourceInstancesPrt(collision_source_instances);
        collision_source_instances_source->SourceTypeIndexOffset = (u16)(((uintptr_t)&enemy_instances_enemy_index[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)collision_source_instances));

        EnemyInstancesWaveEnemyInstances *enemy_instances_wave_enemy_instances = EnemyInstancesWaveEnemyInstancesPrt(enemy_instances_wave);
        enemy_instances_wave_enemy_instances->StartTimeQ4Offset         = (u16)(((uintptr_t)&enemy_instances_start_time_q4[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_instances_wave));
        enemy_instances_wave_enemy_instances->EnemyIndexOffset          = (u16)(((uintptr_t)&enemy_instances_enemy_index[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_instances_wave));
        enemy_instances_wave_enemy_instances->FlatSpawnPointIndexOffset = (u16)(((uintptr_t)&enemy_instances_flat_spawn_point_index[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_instances_wave));
        enemy_instances_wave_enemy_instances->WayPointPathIndexOffset   = (u16)(((uintptr_t)&enemy_instance_way_point_path_index[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_instances_wave));

        BulletSourceInstancesSourceInstances *enemy_bullet_source_instances_source = BulletSourceInstancesSourceInstancesPrt(enemy_bullets_source_instances);
        enemy_bullet_source_instances_source->StartTimeQ4Offset = (u16)(((uintptr_t)&enemy_instances_start_time_q4[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_bullets_source_instances));
        enemy_bullet_source_instances_source->SourceIndexOffset = (u16)(((uintptr_t)&enemy_instances_enemy_index[wave_instance->EnemyInstancesStartIndex]) - ((uintptr_t)enemy_bullets_source_instances));

        *enemy_positions_count_prt = 0;
        *wave_state_ptr &= ~(kEnemyInstancesWaveSpawnedAll | kEnemyInstancesAllWavesComplete);
    }

    b32 is_empty_wave = wave_instance->EnemyInstancesCount == 0;
    b32 is_all_waves_complete = wave_index == kEnemyInstancesMaxWavesPerLevel;

    if (is_empty_wave || is_all_waves_complete)
    {
        *wave_state_ptr |= kEnemyInstancesAllWavesComplete;
        return;
    }

    enemy_instances_move(context);
    enemy_instances_spawn(context);

    b32 is_wave_spawned_all = (*enemy_positions_count_prt) == wave_instance->EnemyInstancesCount;
    if (is_wave_spawned_all)
    {
        *wave_state_ptr |= kEnemyInstancesWaveSpawnedAll;
    }
}
