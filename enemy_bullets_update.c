static void
enemy_bullets_spawn(EnemyBulletsUpdateContext *context)
{
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->EnemyInstancesUpdateBin;
    EnemyBullets *enemy_bullets                  = context->EnemyBulletsBin;
    EnemyBulletsUpdate *enemy_bullets_update     = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet  = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet   = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesEnemyTypes *enemy_instances_types_sheet = EnemyInstancesEnemyTypesPrt(enemy_instances);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    u8 *enemy_instances_enemy_index = EnemyInstancesEnemyInstancesEnemyIndexPrt(enemy_instances, enemy_instances_sheet);
    EnemyInstancesEnemyTypesEnemyBulletTypes *enemy_types_enemy_bullet_types = EnemyInstancesEnemyTypesEnemyBulletTypesPrt(enemy_instances, enemy_instances_types_sheet);

    u16 *enemy_instances_start_time_q4 = EnemyInstancesEnemyInstancesStartTimeQ4Prt(enemy_instances, enemy_instances_sheet);

    EnemyBulletsEnemyBulletTypes *enemy_bullet_types_sheet = EnemyBulletsEnemyBulletTypesPrt(enemy_bullets);

    EnemyBulletsEnemyBulletTypesLocalXYQ7 *enemy_bullets_local_positions_q7 = EnemyBulletsEnemyBulletTypesLocalXYQ7Prt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullets_time_cast_q4  = EnemyBulletsEnemyBulletTypesTimeCastQ4Prt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullets_time_loop_q4  = EnemyBulletsEnemyBulletTypesTimeLoopQ4Prt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullets_time_delay_q4 = EnemyBulletsEnemyBulletTypesTimeDelayQ4Prt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullets_type_quantity = EnemyBulletsEnemyBulletTypesQuantityPrt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullets_type_index    = EnemyBulletsEnemyBulletTypesBulletTypeIndexPrt(enemy_bullets, enemy_bullet_types_sheet);

    EnemyBulletsUpdateBulletPositions *enemy_bullet_update_positions_sheet = EnemyBulletsUpdateBulletPositionsPrt(enemy_bullets_update);
    v2 *enemy_bullets_update_positions                                     = (v2 *)EnemyBulletsUpdateBulletPositionsCurrentPositionPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);
    v2 *enemy_bullets_update_end_positions                                 = (v2 *)EnemyBulletsUpdateBulletPositionsEndPositionPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);
    uint8_t *enemy_bullets_update_type_index                               = EnemyBulletsUpdateBulletPositionsTypeIndexPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);

    EnemyBulletsUpdateEnemyBullets *enemy_bullets_update_sheet                 = EnemyBulletsUpdateEnemyBulletsPrt(enemy_bullets_update);
    EnemyBulletsUpdateEnemyBulletsSpawnCount *enemy_bullets_update_spawn_count = EnemyBulletsUpdateEnemyBulletsSpawnCountPrt(enemy_bullets_update, enemy_bullets_update_sheet);

    EnemyInstancesUpdateEnemyPositions *enemy_instances_update_positions_sheet = EnemyInstancesUpdateEnemyPositionsPrt(enemy_instances_update);
    v2 *enemy_instances_positions                                              = (v2 *)EnemyInstancesUpdateEnemyPositionsPositionsPrt(enemy_instances_update, enemy_instances_update_positions_sheet);

    f32 bullet_end_length = 5.0f * max(kEnemyInstancesWidth, kEnemyInstancesHeight);

    for (u8 wave_instance_index = 0; wave_instance_index < enemy_instances_update->EnemyPositionsCount; wave_instance_index++)
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

        v2 enemy_instance_position = enemy_instances_positions[wave_instance_index];

        EnemyInstancesEnemyTypesEnemyBulletTypes enemy_bullets_type = enemy_types_enemy_bullet_types[enemy_index];

        for (u8 i = 0; i < enemy_bullets_type.EnemyBulletTypeCount; i++)
        {
            u8 enemy_bullet_index = enemy_bullets_type.EnemyBulletTypeStartIndex + i;

            EnemyBulletsEnemyBulletTypesLocalXYQ7 bullet_local_position_q7 = enemy_bullets_local_positions_q7[enemy_bullet_index];
            v2 local_position = V2(((f32)bullet_local_position_q7.LocalXQ7) * kQ7ToFloat,
                                   ((f32)bullet_local_position_q7.LocalYQ7) * kQ7ToFloat);

            f32 local_position_length = v2_length(local_position);
            v2 end_vector = v2_scale(local_position, bullet_end_length / local_position_length);

            v2 spawn_position = v2_add(enemy_instance_position, local_position);
            v2 end_position = v2_add(enemy_instance_position, end_vector);

            u8 bullet_time_cast_q4 = enemy_bullets_time_cast_q4[enemy_bullet_index];
            f32 bullet_time_cast = ((f32)bullet_time_cast_q4) * kQ4ToFloat;

            u8 bullet_time_loop_q4 = enemy_bullets_time_loop_q4[enemy_bullet_index];
            f32 bullet_time_loop = ((f32)bullet_time_loop_q4) * kQ4ToFloat;

            u8 bullet_time_delay_q4 = enemy_bullets_time_delay_q4[enemy_bullet_index];
            f32 bullet_time_delay = ((f32)bullet_time_delay_q4) * kQ4ToFloat;

            u8 bullet_quantity = enemy_bullets_type_quantity[enemy_bullet_index];
            u8 bullet_type_index = enemy_bullets_type_index[enemy_bullet_index];
       
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

            EnemyBulletsUpdateEnemyBulletsSpawnCount *enemy_bullets_spawn_count = &enemy_bullets_update_spawn_count[wave_instance_index];

            u8 actual_spawn_count = enemy_bullets_spawn_count->SpawnCount[i];
            
            if (expected_spawn_count <= actual_spawn_count)
            {
                continue;
            }

            enemy_bullets_update_positions[enemy_bullets_update->BulletPositionsCount] = spawn_position;
            enemy_bullets_update_end_positions[enemy_bullets_update->BulletPositionsCount] = end_position;
            enemy_bullets_update_type_index[enemy_bullets_update->BulletPositionsCount] = bullet_type_index;

            enemy_bullets_update->BulletPositionsCount = (enemy_bullets_update->BulletPositionsCount + 1) % kEnemyBulletsUpdateEnemyBulletsMaxInstanceCount;
            enemy_bullets_update->WaveSpawnCount++;
            enemy_bullets_spawn_count->SpawnCount[i]++;
        }
    }
}

static void
enemy_bullets_move(EnemyBulletsUpdateContext *context)
{
    EnemyBullets* enemy_bullets              = context->EnemyBulletsBin;
    EnemyBulletsUpdate *enemy_bullets_update = context->Root;
    GameState *game_state                    = context->GameStateBin;

    EnemyBulletsUpdateBulletPositions *enemy_bullet_update_positions_sheet = EnemyBulletsUpdateBulletPositionsPrt(enemy_bullets_update);
    v2 *enemy_bullets_positions                                            = (v2 *)EnemyBulletsUpdateBulletPositionsCurrentPositionPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);
    v2 *enemy_bullets_end_positions                                        = (v2 *)EnemyBulletsUpdateBulletPositionsEndPositionPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);
    uint8_t *enemy_bullets_type_index                                      = EnemyBulletsUpdateBulletPositionsTypeIndexPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);

    EnemyBulletsBulletTypes *enemy_bullet_types_sheet = EnemyBulletsBulletTypesPrt(enemy_bullets);

    u8 *enemy_bullet_types_radius_q8         = EnemyBulletsBulletTypesRadiusQ8Prt(enemy_bullets, enemy_bullet_types_sheet);
    u8 *enemy_bullet_types_movement_speed_q4 = EnemyBulletsBulletTypesMovementSpeedQ4Prt(enemy_bullets, enemy_bullet_types_sheet);
   
    s32 update_count = min(kEnemyBulletsUpdateEnemyBulletsMaxInstanceCount, enemy_bullets_update->WaveSpawnCount);
    
    for (s32 i = 0; i < update_count; i++)
    {
        v2 bullet_position     = enemy_bullets_positions[i];
        v2 bullet_end_position = enemy_bullets_end_positions[i];
        u8 bullet_type_index   = enemy_bullets_type_index[i];

        u8 bullet_radius_q8 = enemy_bullet_types_radius_q8[bullet_type_index];
        f32 bullet_radius   = ((f32)bullet_radius_q8) * kQ8ToFloat;

        u8 bullet_movement_speed_q4 = enemy_bullet_types_movement_speed_q4[bullet_type_index];
        f32 bullet_movement_speed   = ((f32)bullet_movement_speed_q4) * kQ4ToFloat;

        if ((fabsf(bullet_position.x) - bullet_radius) > kEnemyInstancesHalfWidth)
        {
            continue;
        }

        if ((fabsf(bullet_position.y) - bullet_radius) > kEnemyInstancesHalfHeight)
        {
            continue;
        }

        f32 enemy_bullet_frame_move_dist = game_state->TimeDelta * bullet_movement_speed;

        v2 dv            = v2_sub(bullet_end_position, bullet_position);
        f32 dv_length    = v2_length(dv);
        v2 move_v        = v2_scale(dv, enemy_bullet_frame_move_dist / dv_length);
        v2 move_position = v2_add(bullet_position, move_v);

        enemy_bullets_positions[i] = move_position;
    }
}

static void
enemy_bullets_update(EnemyBulletsUpdateContext *context)
{
    EnemyBulletsUpdate *enemy_bullets_update     = context->Root;
    EnemyInstancesUpdate *enemy_instances_update = context->EnemyInstancesUpdateBin;
    GameState *game_state                        = context->GameStateBin;

    EnemyBulletsUpdateEnemyBullets *enemy_bullets_update_sheet                 = EnemyBulletsUpdateEnemyBulletsPrt(enemy_bullets_update);
    EnemyBulletsUpdateEnemyBulletsSpawnCount *enemy_bullets_update_spawn_count = EnemyBulletsUpdateEnemyBulletsSpawnCountPrt(enemy_bullets_update, enemy_bullets_update_sheet);

    u8 flat_wave_index = (game_state->LevelIndex << 2) + enemy_instances_update->WaveIndex;

    if (flat_wave_index != enemy_bullets_update->LastFlatWaveIndex)
    {
        memset(enemy_bullets_update_spawn_count, 0, kEnemyBulletsUpdateMaxEnemyBulletTypesPerEnemyType * kEnemyBulletsUpdateMaxInstancesPerWave);
        enemy_bullets_update->WaveSpawnCount = 0;
        enemy_bullets_update->BulletPositionsCount = 0;

        enemy_bullets_update->LastFlatWaveIndex = flat_wave_index;
    }

    enemy_bullets_spawn(context);
    enemy_bullets_move(context);
}