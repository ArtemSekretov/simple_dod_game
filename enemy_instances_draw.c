
static void
enemy_instances_draw(EnemyInstancesDrawContext *context)
{
    FrameData *frame_data                        = context->FrameDataBin;
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesUpdate *enemy_instances_update = context->EnemyInstancesUpdateBin;
    GameState *game_state                        = context->GameStateBin;
    WaveUpdate *wave_update                      = context->WaveUpdateBin;

    u64 enemy_instances_live = enemy_instances_update->InstancesLive;
    u8 flat_wave_index = (game_state->LevelIndex << 2) + wave_update->WaveIndex;

    EnemyInstancesLevelWaveIndex *level_wave_index_sheet = EnemyInstancesLevelWaveIndexPrt(enemy_instances);
    EnemyInstancesEnemyInstances *enemy_instances_sheet  = EnemyInstancesEnemyInstancesPrt(enemy_instances);
    EnemyInstancesEnemyTypes     *enemy_sheet            = EnemyInstancesEnemyTypesPrt(enemy_instances);
    
    u8 *enemy_instances_enemy_index = EnemyInstancesEnemyInstancesEnemyIndexPrt(enemy_instances, enemy_instances_sheet);
    u8 *enemy_radius_q4 = EnemyInstancesEnemyTypesRadiusQ4Prt(enemy_instances, enemy_sheet);

    EnemyInstancesLevelWaveIndexLevelWave *level_wave_index_instance = EnemyInstancesLevelWaveIndexLevelWavePrt(enemy_instances, level_wave_index_sheet);
    EnemyInstancesLevelWaveIndexLevelWave wave_instance = level_wave_index_instance[flat_wave_index];

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr = FrameDataFrameDataCountPrt(frame_data);

    EnemyInstancesUpdateEnemyPositions *enemy_instances_update_positions_sheet = EnemyInstancesUpdateEnemyPositionsPrt(enemy_instances_update);
    v2 *enemy_instances_positions                                              = (v2 *)EnemyInstancesUpdateEnemyPositionsPositionsPrt(enemy_instances_update, enemy_instances_update_positions_sheet);

    u16 enemy_positions_count = *EnemyInstancesUpdateEnemyPositionsCountPrt(enemy_instances_update);

    for (u8 wave_instance_index = 0; wave_instance_index < enemy_positions_count; wave_instance_index++)
    {
        if ((enemy_instances_live & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u16 enemy_instances_index = wave_instance.EnemyInstancesStartIndex + wave_instance_index;
        u8 flat_enemy_variation_index = enemy_instances_enemy_index[enemy_instances_index];

        u8 radius_q4 = enemy_radius_q4[flat_enemy_variation_index];
        f32 radius = ((f32)radius_q4) * kQ4ToFloat;

        v2 enemy_instance_position = enemy_instances_positions[wave_instance_index];

        u16 frame_data_count = *frame_data_count_ptr;
        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

        object_data->PositionAndScale[0] = enemy_instance_position.x;
        object_data->PositionAndScale[1] = enemy_instance_position.y;
        object_data->PositionAndScale[2] = radius * 2.0f;

        object_data->Color[0] = 0.0f;
        object_data->Color[1] = 1.0f;
        object_data->Color[2] = 1.0f;

        *frame_data_count_ptr = (frame_data_count + 1) % kFrameDataMaxObjectDataCapacity;
    }
}