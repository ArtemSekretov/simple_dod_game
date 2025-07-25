static void
bullets_spawn(BulletsUpdateContext *context)
{
    BulletSourceInstances *bullet_source_instances = context->BulletSourceInstancesBin;
    Bullets *bullets                               = context->BulletsBin;
    BulletsUpdate *bullets_update                  = context->Root;
    WaveUpdate *wave_update                        = context->WaveUpdateBin;

    f32 wave_time = *WaveUpdateWaveTimePrt(wave_update);

    BulletSourceInstancesSourceInstances *bullet_source_instances_sheet = BulletSourceInstancesSourceInstancesPrt(bullet_source_instances);

    u8 *bullet_source_instances_source_index = BulletSourceInstancesSourceInstancesSourceIndexPrt(bullet_source_instances, bullet_source_instances_sheet);
    u16 *bullet_source_start_time_q4         = BulletSourceInstancesSourceInstancesStartTimeQ4Prt(bullet_source_instances, bullet_source_instances_sheet);

    BulletsSourceTypes *bullets_types_sheet      = BulletsSourceTypesPrt(bullets);
    BulletsSourceBulletTypes *bullet_types_sheet = BulletsSourceBulletTypesPrt(bullets);

    BulletsSourceTypesSourceBulletTypes *types_enemy_bullet_types = BulletsSourceTypesSourceBulletTypesPrt(bullets, bullets_types_sheet);

    BulletsSourceBulletTypesLocalXYQ7 *bullets_local_positions_q7 = BulletsSourceBulletTypesLocalXYQ7Prt(bullets, bullet_types_sheet);
    u8 *bullets_time_cast_q4  = BulletsSourceBulletTypesTimeCastQ4Prt(bullets, bullet_types_sheet);
    u8 *bullets_time_loop_q4  = BulletsSourceBulletTypesTimeLoopQ4Prt(bullets, bullet_types_sheet);
    u8 *bullets_time_delay_q4 = BulletsSourceBulletTypesTimeDelayQ4Prt(bullets, bullet_types_sheet);
    u8 *bullets_type_quantity = BulletsSourceBulletTypesQuantityPrt(bullets, bullet_types_sheet);
    u8 *bullets_type_index    = BulletsSourceBulletTypesBulletTypeIndexPrt(bullets, bullet_types_sheet);

    BulletsUpdateBulletPositions *bullet_update_positions_sheet = BulletsUpdateBulletPositionsPrt(bullets_update);
    v2 *bullets_update_positions       = (v2 *)BulletsUpdateBulletPositionsCurrentPositionPrt(bullets_update, bullet_update_positions_sheet);
    v2 *bullets_update_end_positions   = (v2 *)BulletsUpdateBulletPositionsEndPositionPrt(bullets_update, bullet_update_positions_sheet);
    uint8_t *bullets_update_type_index = BulletsUpdateBulletPositionsTypeIndexPrt(bullets_update, bullet_update_positions_sheet);

    BulletsUpdateSourceBullets *bullets_update_sheet                 = BulletsUpdateSourceBulletsPrt(bullets_update);
    BulletsUpdateSourceBulletsSpawnCount *bullets_update_spawn_count = BulletsUpdateSourceBulletsSpawnCountPrt(bullets_update, bullets_update_sheet);

    BulletSourceInstancesPositions *bullet_instances_positions_sheet = BulletSourceInstancesPositionsPrt(bullet_source_instances);
    v2 *bullet_instances_positions                                   = (v2 *)BulletSourceInstancesPositionsPositionsPrt(bullet_source_instances, bullet_instances_positions_sheet);

    f32 bullet_end_length = 5.0f * max(kPlayAreaWidth, kPlayAreaHeight);

    u64 bullet_source_instances_live  = *BulletSourceInstancesInstancesLivePrt(bullet_source_instances);
    u16 bullet_source_positions_count = *BulletSourceInstancesPositionsCountPrt(bullet_source_instances);
    
    u16 *bullet_positions_count_ptr = BulletsUpdateBulletPositionsCountPrt(bullets_update);
    u32 *wave_spawn_count_ptr       = BulletsUpdateWaveSpawnCountPrt(bullets_update);

    for (u8 wave_instance_index = 0; wave_instance_index < bullet_source_positions_count; wave_instance_index++)
    {
        if ((bullet_source_instances_live & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u8 enemy_index = bullet_source_instances_source_index[wave_instance_index];

        u16 start_time_q4 = bullet_source_start_time_q4[wave_instance_index];
        f32 start_time = ((f32)start_time_q4) * kQ4ToFloat;

        f32 enemy_instance_time = wave_time - start_time;

        v2 bullet_instance_position = bullet_instances_positions[wave_instance_index];

        BulletsSourceTypesSourceBulletTypes bullets_type = types_enemy_bullet_types[enemy_index];

        for (u8 i = 0; i < bullets_type.SourceBulletTypeCount; i++)
        {
            u8 bullet_index = bullets_type.SourceBulletTypeStartIndex + i;

            BulletsSourceBulletTypesLocalXYQ7 bullet_local_position_q7 = bullets_local_positions_q7[bullet_index];
            v2 local_position = V2(((f32)bullet_local_position_q7.LocalXQ7) * kQ7ToFloat,
                                   ((f32)bullet_local_position_q7.LocalYQ7) * kQ7ToFloat);

            f32 local_position_length = v2_length(local_position);
            v2 end_vector = v2_scale(local_position, bullet_end_length / local_position_length);

            v2 spawn_position = v2_add(bullet_instance_position, local_position);
            v2 end_position = v2_add(bullet_instance_position, end_vector);

            u8 bullet_time_cast_q4 = bullets_time_cast_q4[bullet_index];
            f32 bullet_time_cast = ((f32)bullet_time_cast_q4) * kQ4ToFloat;

            u8 bullet_time_loop_q4 = bullets_time_loop_q4[bullet_index];
            f32 bullet_time_loop = ((f32)bullet_time_loop_q4) * kQ4ToFloat;

            u8 bullet_time_delay_q4 = bullets_time_delay_q4[bullet_index];
            f32 bullet_time_delay = ((f32)bullet_time_delay_q4) * kQ4ToFloat;

            u8 bullet_quantity = bullets_type_quantity[bullet_index];
            u8 bullet_type_index = bullets_type_index[bullet_index];
       
            f32 time_start = enemy_instance_time - bullet_time_cast;

            if (time_start < 0.0f)
            {
                continue;
            }

            u8 expected_spawn_count = (u8)(time_start / bullet_time_loop);

            if (bullet_quantity > 1)
            {
                expected_spawn_count *= bullet_quantity;
                expected_spawn_count += min(bullet_quantity - 1, (u8)(fmodf(time_start, bullet_time_loop) / bullet_time_delay));
            }

            BulletsUpdateSourceBulletsSpawnCount *bullets_spawn_count = &bullets_update_spawn_count[wave_instance_index];

            u8 actual_spawn_count = bullets_spawn_count->SpawnCount[i];
            
            if (expected_spawn_count <= actual_spawn_count)
            {
                continue;
            }

            u16 bullet_positions_count = *bullet_positions_count_ptr;

            bullets_update_positions[bullet_positions_count] = spawn_position;
            bullets_update_end_positions[bullet_positions_count] = end_position;
            bullets_update_type_index[bullet_positions_count] = bullet_type_index;

            *bullet_positions_count_ptr = (bullet_positions_count + 1) % kBulletsUpdateSourceBulletsMaxInstanceCount;
            (*wave_spawn_count_ptr)++;
            bullets_spawn_count->SpawnCount[i]++;
        }
    }
}

static void
bullets_move(BulletsUpdateContext *context)
{
    Bullets* bullets              = context->BulletsBin;
    BulletsUpdate *bullets_update = context->Root;
    GameState *game_state         = context->GameStateBin;

    f32 time_delta = *GameStateTimeDeltaPrt(game_state);

    BulletsUpdateBulletPositions *bullet_update_positions_sheet = BulletsUpdateBulletPositionsPrt(bullets_update);
    v2 *bullets_positions                                 = (v2 *)BulletsUpdateBulletPositionsCurrentPositionPrt(bullets_update, bullet_update_positions_sheet);
    v2 *bullets_end_positions                             = (v2 *)BulletsUpdateBulletPositionsEndPositionPrt(bullets_update, bullet_update_positions_sheet);
    uint8_t *bullets_type_index                           = BulletsUpdateBulletPositionsTypeIndexPrt(bullets_update, bullet_update_positions_sheet);

    BulletsBulletTypes *bullet_types_sheet = BulletsBulletTypesPrt(bullets);

    u8 *bullet_types_radius_q8         = BulletsBulletTypesRadiusQ8Prt(bullets, bullet_types_sheet);
    u8 *bullet_types_movement_speed_q4 = BulletsBulletTypesMovementSpeedQ4Prt(bullets, bullet_types_sheet);
    u32 wave_spawn_count               = *BulletsUpdateWaveSpawnCountPrt(bullets_update);

    s32 update_count = min(kBulletsUpdateSourceBulletsMaxInstanceCount, wave_spawn_count);
    
    for (s32 i = 0; i < update_count; i++)
    {
        v2 bullet_position     = bullets_positions[i];
        v2 bullet_end_position = bullets_end_positions[i];
        u8 bullet_type_index   = bullets_type_index[i];

        u8 bullet_radius_q8 = bullet_types_radius_q8[bullet_type_index];
        f32 bullet_radius   = ((f32)bullet_radius_q8) * kQ8ToFloat;

        u8 bullet_movement_speed_q4 = bullet_types_movement_speed_q4[bullet_type_index];
        f32 bullet_movement_speed   = ((f32)bullet_movement_speed_q4) * kQ4ToFloat;

        if ((fabsf(bullet_position.x) - bullet_radius) > kPlayAreaHalfWidth)
        {
            continue;
        }

        if ((fabsf(bullet_position.y) - bullet_radius) > kPlayAreaHalfHeight)
        {
            continue;
        }

        f32 source_bullet_frame_move_dist = time_delta * bullet_movement_speed;

        v2 dv            = v2_sub(bullet_end_position, bullet_position);
        f32 dv_length    = v2_length(dv);
        v2 move_v        = v2_scale(dv, source_bullet_frame_move_dist / dv_length);
        v2 move_position = v2_add(bullet_position, move_v);

        bullets_positions[i] = move_position;
    }
}

static void
bullets_update(BulletsUpdateContext *context)
{
    BulletsUpdate *bullets_update = context->Root;
    GameState *game_state         = context->GameStateBin;
    WaveUpdate *wave_update       = context->WaveUpdateBin;

    u8 level_index = *GameStateLevelIndexPrt(game_state);

    u8 wave_index = *WaveUpdateWaveIndexPrt(wave_update);

    BulletsUpdateSourceBullets *bullets_update_sheet                 = BulletsUpdateSourceBulletsPrt(bullets_update);
    BulletsUpdateSourceBulletsSpawnCount *bullets_update_spawn_count = BulletsUpdateSourceBulletsSpawnCountPrt(bullets_update, bullets_update_sheet);

    u16 *bullet_positions_count_ptr = BulletsUpdateBulletPositionsCountPrt(bullets_update);
    u32 *wave_spawn_count_ptr       = BulletsUpdateWaveSpawnCountPrt(bullets_update);
    u8 *last_flat_wave_index_ptr    = BulletsUpdateLastFlatWaveIndexPrt(bullets_update);
    
    u8 flat_wave_index = (level_index << 2) + wave_index;

    if (flat_wave_index != (*last_flat_wave_index_ptr))
    {
        memset(bullets_update_spawn_count, 0, kBulletsUpdateMaxSourceBulletTypesPerSourceType * kBulletsUpdateMaxInstancesPerWave);
        *wave_spawn_count_ptr = 0;
        *bullet_positions_count_ptr = 0;

        *last_flat_wave_index_ptr = flat_wave_index;
    }

    bullets_spawn(context);
    bullets_move(context);
}